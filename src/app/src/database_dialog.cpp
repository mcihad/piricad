// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/app/database_dialog.hpp"

#include "kentos_cad/app/controller.hpp"

#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/core/document.hpp"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QStyle>
#include <QVBoxLayout>

namespace kentos::app {
namespace {

/// A value quoted for the command line.
///
/// A connection string holds `=` and spaces, and a project name can hold
/// anything a person types. The parser treats a quoted value as literal — that is
/// what quoting means there — so this is the whole escaping rule.
QString quoted(const QString& value)
{
    QString out = value;
    out.replace('\\', QStringLiteral("\\\\"));
    out.replace('"', QStringLiteral("\\\""));
    return '"' + out + '"';
}

/// The table name out of a `public.ada_parsel  (geom, EPSG:5254, ~14 satır)` line.
QString table_of(const QString& row)
{
    const QString head = row.section(QLatin1String("  ("), 0, 0).trimmed();
    return head.section('.', -1);
}

/// The project name out of a `deneme  (3 MB, 2026-08-23 …)` line.
QString project_of(const QString& row)
{
    return row.section(QLatin1String("  ("), 0, 0).trimmed();
}

} // namespace

DatabaseDialog::DatabaseDialog(Controller& controller, QWidget* parent)
    : QDialog(parent), controller_(controller)
{
    setWindowTitle(tr("Veritabanı — PostGIS"));
    setModal(false);
    setMinimumSize(820, 580);
    resize(900, 660);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 14, 14, 12);
    layout->setSpacing(10);
    layout->addWidget(buildConnection());
    layout->addWidget(buildContents(), 1);

    // The text is set explicitly rather than left to QDialogButtonBox's standard
    // label: Qt's own translations are not loaded here, and a Turkish window with
    // an English "Close" on it is the kind of seam a user notices immediately.
    auto* buttons = new QDialogButtonBox(this);
    buttons->addButton(tr("Kapat"), QDialogButtonBox::RejectRole);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    updateEnabled();
}

QWidget* DatabaseDialog::buildConnection()
{
    auto* box  = new QGroupBox(tr("PostGIS bağlantısı"), this);
    auto* rows = new QVBoxLayout(box);
    rows->setSpacing(8);

    auto* form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignRight);
    form->setHorizontalSpacing(12);
    form->setVerticalSpacing(7);

    // Filled from the APPLICATION settings, which is where the last connection
    // was remembered. Not from the project: a drawing mailed to a colleague must
    // not carry a pointer at a database.
    command::Bus& bus = controller_.bus();

    host_ = new QLineEdit(
        QString::fromUtf8(bus.setting("core.veritabani.sunucu").as_text().data(),
                          static_cast<int>(bus.setting("core.veritabani.sunucu").as_text().size())),
        box);
    host_->setPlaceholderText(tr("localhost — ya da tam bir bağlantı dizesi"));
    host_->setAccessibleName(tr("PostGIS sunucu adresi"));
    host_->setToolTip(tr("Sunucu adresi. İçinde '=' geçen tam bir libpq bağlantı dizesi "
                         "yazarsanız diğer alanlar yok sayılır."));

    port_ = new QSpinBox(box);
    port_->setRange(1, 65535);
    port_->setValue(static_cast<int>(bus.setting("core.veritabani.port").as_int()));
    port_->setAccessibleName(tr("PostGIS bağlantı noktası"));

    const auto text_of = [&bus](const char* id) {
        const std::string_view v = bus.setting(id).as_text();
        return QString::fromUtf8(v.data(), static_cast<int>(v.size()));
    };

    database_ = new QLineEdit(text_of("core.veritabani.ad"), box);
    database_->setPlaceholderText(tr("veritabanı adı"));
    database_->setAccessibleName(tr("PostGIS veritabanı adı"));

    user_ = new QLineEdit(text_of("core.veritabani.kullanici"), box);
    user_->setPlaceholderText(tr("kullanıcı adı"));
    user_->setAccessibleName(tr("PostGIS kullanıcı adı"));

    password_ = new QLineEdit(box);
    password_->setEchoMode(QLineEdit::Password);
    password_->setPlaceholderText(tr("boş bırakın — ~/.pgpass ya da PGPASSWORD"));
    password_->setAccessibleName(tr("PostGIS parolası"));
    password_->setToolTip(tr("Parola HİÇBİR YERE kaydedilmez. Her açılışta boş başlar. "
                             "Kalıcı olması için ~/.pgpass dosyasını kullanın."));

    form->addRow(tr("Adres:"), host_);
    form->addRow(tr("Port:"), port_);
    form->addRow(tr("Veritabanı:"), database_);
    form->addRow(tr("Kullanıcı:"), user_);
    form->addRow(tr("Parola:"), password_);
    rows->addLayout(form);

    auto* actions = new QHBoxLayout;
    connect_      = new QPushButton(tr("Bağlan"), box);
    disconnect_   = new QPushButton(tr("Bağlantıyı Kes"), box);
    refresh_      = new QPushButton(tr("Yenile"), box);
    refresh_->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
    refresh_->setToolTip(tr("Sunucudaki tabloları ve kayıtlı projeleri yeniden okur"));
    connect_->setDefault(true);
    connect_->setAccessibleName(tr("PostGIS sunucusuna bağlan"));
    disconnect_->setAccessibleName(tr("PostGIS bağlantısını kes"));
    refresh_->setAccessibleName(tr("PostGIS listelerini yenile"));
    actions->addWidget(connect_);
    actions->addWidget(disconnect_);
    actions->addWidget(refresh_);
    actions->addStretch(1);

    status_ = new QLabel(tr("Bağlı değil. Bağlantı bilgilerini girip bağlanın."), box);
    status_->setObjectName(QStringLiteral("databaseStatus"));
    status_->setWordWrap(true);
    status_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    actions->addWidget(status_, 1);
    rows->addLayout(actions);

    connect(connect_, &QPushButton::clicked, this, &DatabaseDialog::connectToServer);
    connect(disconnect_, &QPushButton::clicked, this, &DatabaseDialog::disconnectFromServer);
    connect(refresh_, &QPushButton::clicked, this, &DatabaseDialog::refresh);
    connect(host_, &QLineEdit::returnPressed, this, &DatabaseDialog::connectToServer);
    connect(password_, &QLineEdit::returnPressed, this, &DatabaseDialog::connectToServer);

    return box;
}

QWidget* DatabaseDialog::buildContents()
{
    auto* split = new QSplitter(Qt::Horizontal, this);
    split->setChildrenCollapsible(false);

    // ---- tables ----
    auto* left     = new QGroupBox(tr("Mekansal tablolar"), split);
    auto* leftRows = new QVBoxLayout(left);

    tables_ = new QListWidget(left);
    tables_->setToolTip(tr("Sunucudaki geometri sütunu olan tablolar."));
    tables_->setAccessibleName(tr("PostGIS mekansal tabloları"));
    tables_->setAlternatingRowColors(true);
    leftRows->addWidget(tables_, 1);

    writeLayer_ = new QPushButton(tr("Etkin Katmanı Yaz…"), left);
    writeLayer_->setToolTip(tr("VERİTABANI katmanyaz — katmanı bir mekansal tablo olarak yazar. "
                               "Aynı adlı tablo varsa YERİNE yazılır."));
    leftRows->addWidget(writeLayer_);

    // ---- projects ----
    auto* right     = new QGroupBox(tr("Kayıtlı KentOSCad projeleri"), split);
    auto* rightRows = new QVBoxLayout(right);

    projects_ = new QListWidget(right);
    projects_->setToolTip(tr("Bu veritabanına kaydedilmiş KentOSCad projeleri."));
    projects_->setAccessibleName(tr("Kayıtlı KentOSCad projeleri"));
    projects_->setAlternatingRowColors(true);
    rightRows->addWidget(projects_, 1);

    auto* buttons = new QHBoxLayout;
    saveProject_  = new QPushButton(tr("Projeyi Kaydet…"), right);
    openProject_  = new QPushButton(tr("Projeyi Aç"), right);
    dropProject_  = new QPushButton(tr("Projeyi Sil"), right);
    dropProject_->setIcon(style()->standardIcon(QStyle::SP_TrashIcon));
    buttons->addWidget(saveProject_);
    buttons->addWidget(openProject_);
    buttons->addWidget(dropProject_);
    rightRows->addLayout(buttons);

    split->addWidget(left);
    split->addWidget(right);
    split->setStretchFactor(0, 1);
    split->setStretchFactor(1, 1);
    split->setSizes({430, 430});

    connect(writeLayer_, &QPushButton::clicked, this, &DatabaseDialog::writeLayer);
    connect(saveProject_, &QPushButton::clicked, this, &DatabaseDialog::saveProject);
    connect(openProject_, &QPushButton::clicked, this, &DatabaseDialog::openProject);
    connect(dropProject_, &QPushButton::clicked, this, &DatabaseDialog::dropProject);
    connect(projects_, &QListWidget::itemDoubleClicked, this, &DatabaseDialog::openProject);
    connect(projects_, &QListWidget::itemSelectionChanged, this, &DatabaseDialog::updateEnabled);

    return split;
}

QString DatabaseDialog::conninfo() const
{
    // A host box holding an `=` is a whole connection string the user pasted, and
    // taking it verbatim is what lets them reach `sslmode`, `service=` or a URI
    // without this window growing a field for every libpq keyword.
    const QString host = host_->text().trimmed();
    if (host.contains('=') || host.startsWith(QLatin1String("postgres"))) return host;

    QString out = QStringLiteral("host=%1 port=%2")
                      .arg(host.isEmpty() ? QStringLiteral("localhost") : host)
                      .arg(port_->value());
    if (!database_->text().trimmed().isEmpty())
        out += QStringLiteral(" dbname=") + database_->text().trimmed();
    if (!user_->text().trimmed().isEmpty())
        out += QStringLiteral(" user=") + user_->text().trimmed();

    // An empty password field means "ask libpq", which reads ~/.pgpass and
    // PGPASSWORD. Sending `password=` empty would instead tell libpq the password
    // IS empty and stop it looking.
    if (!password_->text().isEmpty()) out += QStringLiteral(" password=") + password_->text();
    return out;
}

QString DatabaseDialog::run(const QString& line)
{
    QString said;
    // One echo, captured for the duration of this call only. The transcript panel
    // is still connected and still shows it: this listens as well, it does not
    // intercept.
    const auto connection = connect(&controller_, &Controller::echoed, this,
                                    [&said](const QString& text) { said = text; });
    controller_.runLine(line, command::Origin::Gui);
    disconnect(connection);
    return said;
}

void DatabaseDialog::connectToServer()
{
    const QString said = run(QStringLiteral("VERİTABANI baglan hedef=") + quoted(conninfo()));

    connected_ = said.startsWith(tr("Bağlanıldı"));
    status_->setText(said.isEmpty() ? tr("Bağlanılamadı.") : said);

    if (connected_) {
        rememberConnection();
        refresh();
    }
    updateEnabled();
}

void DatabaseDialog::disconnectFromServer()
{
    status_->setText(run(QStringLiteral("VERİTABANI kes")));
    connected_ = false;
    tables_->clear();
    projects_->clear();
    updateEnabled();
}

void DatabaseDialog::rememberConnection()
{
    // Through `TERCİH`, not through a direct write: an application preference set
    // by this window and one set from a script must land in the same place by the
    // same route (Article 1.2).
    if (!host_->text().contains('=')) {
        controller_.runLine(QStringLiteral("TERCİH veritabani_sunucu ") + quoted(host_->text()),
                            command::Origin::Gui);
        controller_.runLine(QStringLiteral("TERCİH veritabani_port %1").arg(port_->value()),
                            command::Origin::Gui);
        controller_.runLine(QStringLiteral("TERCİH veritabani_adi ") + quoted(database_->text()),
                            command::Origin::Gui);
        controller_.runLine(QStringLiteral("TERCİH veritabani_kullanici ") + quoted(user_->text()),
                            command::Origin::Gui);
    }
}

void DatabaseDialog::refresh()
{
    tables_->clear();
    projects_->clear();

    // The transcript lines are lists with a heading, and the heading is not a row.
    const auto fill = [](QListWidget* into, const QString& said) {
        const QStringList lines = said.split('\n');
        for (int i = 1; i < lines.size(); ++i) {
            const QString row = lines[i].trimmed();
            if (!row.isEmpty()) into->addItem(row);
        }
    };

    fill(tables_, run(QStringLiteral("VERİTABANI tablolar")));
    fill(projects_, run(QStringLiteral("VERİTABANI projeler")));
}

void DatabaseDialog::writeLayer()
{
    const QString layer = controller_.activeLayerName();
    if (layer.isEmpty()) return;

    bool accepted       = false;
    const QString table = QInputDialog::getText(
        this, tr("Katmanı Veritabanına Yaz"),
        tr("'%1' katmanı hangi tabloya yazılsın?\n"
           "Aynı adlı bir tablo varsa YERİNE yazılır.")
            .arg(layer),
        QLineEdit::Normal,
        table_of(tables_->currentItem() != nullptr ? tables_->currentItem()->text() : QString()),
        &accepted);
    if (!accepted) return;

    QString line = QStringLiteral("VERİTABANI katmanyaz katman=") + quoted(layer);
    if (!table.trimmed().isEmpty()) line += QStringLiteral(" hedef=") + quoted(table.trimmed());

    status_->setText(run(line));
    refresh();
}

void DatabaseDialog::saveProject()
{
    bool accepted      = false;
    const QString name = QInputDialog::getText(this, tr("Projeyi Veritabanına Kaydet"),
                                               tr("Proje hangi adla kaydedilsin?"),
                                               QLineEdit::Normal, QString(), &accepted);
    if (!accepted || name.trimmed().isEmpty()) return;

    status_->setText(run(QStringLiteral("VERİTABANI projekaydet hedef=") + quoted(name.trimmed())));
    refresh();
}

void DatabaseDialog::openProject()
{
    if (projects_->currentItem() == nullptr) return;
    const QString name = project_of(projects_->currentItem()->text());

    // Opening REPLACES the drawing on screen and clears the undo stack, exactly as
    // AÇ does. Asking first is the same courtesy every application extends before
    // throwing away unsaved work.
    if (QMessageBox::question(this, tr("Projeyi Aç"),
                              tr("'%1' açılacak ve ekrandaki çizimin yerini alacak.\n"
                                 "Kaydedilmemiş değişiklikler kaybolur. Sürdürülsün mü?")
                                  .arg(name)) != QMessageBox::Yes)
        return;

    status_->setText(run(QStringLiteral("VERİTABANI projeac hedef=") + quoted(name)));
}

void DatabaseDialog::dropProject()
{
    if (projects_->currentItem() == nullptr) return;
    const QString name = project_of(projects_->currentItem()->text());

    if (QMessageBox::question(this, tr("Projeyi Sil"),
                              tr("'%1' veritabanından KALICI olarak silinecek.\n"
                                 "Bu işlemin geri alması yoktur. Sürdürülsün mü?")
                                  .arg(name)) != QMessageBox::Yes)
        return;

    status_->setText(run(QStringLiteral("VERİTABANI projesil hedef=") + quoted(name)));
    refresh();
}

void DatabaseDialog::updateEnabled()
{
    connect_->setEnabled(!connected_);
    disconnect_->setEnabled(connected_);
    refresh_->setEnabled(connected_);

    host_->setEnabled(!connected_);
    port_->setEnabled(!connected_);
    database_->setEnabled(!connected_);
    user_->setEnabled(!connected_);
    password_->setEnabled(!connected_);

    tables_->setEnabled(connected_);
    projects_->setEnabled(connected_);
    writeLayer_->setEnabled(connected_);
    saveProject_->setEnabled(connected_);

    const bool picked = connected_ && projects_->currentItem() != nullptr;
    openProject_->setEnabled(picked);
    dropProject_->setEnabled(picked);

    status_->setProperty("connected", connected_);
    style()->unpolish(status_);
    style()->polish(status_);
}

} // namespace kentos::app
