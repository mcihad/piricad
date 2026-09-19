// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/app/layout_templates.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

namespace kentos::app {
namespace {

/// The extension a template file carries.
constexpr const char* kSuffix = ".yerleşim.json";

QString utf8(const std::string& s)
{
    return QString::fromUtf8(s.data(), static_cast<int>(s.size()));
}

/// The file name a template of `name` gets.
///
/// THE NAME IS NOT A PATH. A separator, a colon or a `..` in a template name is
/// replaced rather than refused, so the office can call a sheet `18. madde /
/// askı` and still have a file — but nothing it writes can leave the folder.
QString file_stem(const QString& name)
{
    QString out;
    out.reserve(name.size());
    for (const QChar ch : name) {
        if (ch == QLatin1Char('/') || ch == QLatin1Char('\\') || ch == QLatin1Char(':') ||
            ch == QLatin1Char('*') || ch == QLatin1Char('?') || ch == QLatin1Char('"') ||
            ch == QLatin1Char('<') || ch == QLatin1Char('>') || ch == QLatin1Char('|') ||
            ch.isNull())
            out += QLatin1Char('_');
        else
            out += ch;
    }
    out = out.trimmed();
    while (out.startsWith(QLatin1Char('.')))
        out.remove(0, 1);
    return out;
}

} // namespace

LayoutTemplates::LayoutTemplates(command::Bus& bus, QObject* parent) : QObject(parent), bus_(bus)
{
    folder_ = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) +
              QStringLiteral("/yerleşimler");

    bus_.on_layout_template_request = [this](const command::LayoutTemplateRequest& request)
        -> command::Task<core::Result<std::string>> { return this->handle(request); };
}

LayoutTemplates::~LayoutTemplates()
{
    bus_.on_layout_template_request = nullptr;
}

core::Result<QString> LayoutTemplates::pathFor(const QString& name) const
{
    const QString stem = file_stem(name);
    if (stem.isEmpty())
        return core::err(core::ErrorCode::InvalidArgument,
                         "Şablon adı dosya adı olarak kullanılamıyor: '" + name.toStdString() +
                             "'.");
    return folder_ + QStringLiteral("/") + stem + QString::fromUtf8(kSuffix);
}

QStringList LayoutTemplates::names() const
{
    QStringList out;
    QDir dir(folder_);
    if (!dir.exists()) return out;
    for (const QFileInfo& one : dir.entryInfoList(
             {QStringLiteral("*%1").arg(QString::fromUtf8(kSuffix))}, QDir::Files, QDir::Name)) {
        QString name = one.fileName();
        name.chop(static_cast<int>(QString::fromUtf8(kSuffix).size()));
        out << name;
    }
    return out;
}

command::Task<core::Result<std::string>>
LayoutTemplates::handle(command::LayoutTemplateRequest request)
{
    using Verb = command::LayoutTemplateRequest::Verb;

    switch (request.verb) {
    case Verb::List: {
        const QStringList have = names();
        if (have.isEmpty())
            co_return std::string(
                "Kayıtlı çıktı yerleşimi şablonu yok. Bir yerleşimi saklamak için: "
                "ÇIKTIŞABLON islem=kaydet ad=<ad>");
        std::string said = std::to_string(have.size()) + " çıktı yerleşimi şablonu:";
        for (const QString& one : have)
            said += "\n  " + one.toStdString();
        said += "\nKlasör: " + folder_.toStdString();
        co_return said;
    }

    case Verb::Save: {
        auto path = pathFor(utf8(request.name));
        if (!path) co_return path.error();
        QDir().mkpath(folder_);

        QFile file(path.value());
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
            co_return core::err(core::ErrorCode::IoFailure,
                                "Şablon yazılamadı: " + path.value().toStdString());
        // THE BYTES THE COMMAND HANDED OVER, unchanged. This service does not
        // know what a layout is and must not learn: `core::layout_to_json` is
        // the one serialiser (CLAUDE.md 5.10).
        file.write(request.json.data(), static_cast<qint64>(request.json.size()));
        file.close();

        emit changed();
        co_return "Çıktı yerleşimi şablonu kaydedildi: " + request.name + " — '" + request.layout +
            "' yerleşiminden, " + path.value().toStdString();
    }

    case Verb::Apply: {
        auto path = pathFor(utf8(request.name));
        if (!path) co_return path.error();

        QFile file(path.value());
        if (!file.exists()) {
            const QStringList have = names();
            std::string said       = "Çıktı yerleşimi şablonu yok: '" + request.name + "'.";
            if (!have.isEmpty())
                said += " Olanlar: " + have.join(QStringLiteral(", ")).toStdString();
            co_return core::err(core::ErrorCode::NotFound, said);
        }
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            co_return core::err(core::ErrorCode::IoFailure,
                                "Şablon okunamadı: " + path.value().toStdString());

        // THE TEXT GOES BACK TO THE COMMAND, which parses it and writes the
        // sheet through a transaction. Nothing is applied here.
        const QByteArray text = file.readAll();
        co_return std::string(text.constData(), static_cast<std::size_t>(text.size()));
    }

    case Verb::Remove: {
        auto path = pathFor(utf8(request.name));
        if (!path) co_return path.error();
        if (!QFile::exists(path.value()))
            co_return core::err(core::ErrorCode::NotFound,
                                "Çıktı yerleşimi şablonu yok: '" + request.name + "'.");
        if (!QFile::remove(path.value()))
            co_return core::err(core::ErrorCode::IoFailure,
                                "Şablon silinemedi: " + path.value().toStdString());
        emit changed();
        co_return "Çıktı yerleşimi şablonu silindi: " + request.name;
    }
    }
    co_return core::err(core::ErrorCode::Internal, "İşlenmemiş şablon isteği.");
}

} // namespace kentos::app
