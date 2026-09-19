// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/app/print_service.hpp"

#include "kentos_cad/app/layout_render.hpp"

#include "kentos_cad/app/backend_factory.hpp"
#include "kentos_cad/core/document.hpp"
#include "kentos_cad/io/pdf_encrypt.hpp"
#include "kentos_cad/render/backend.hpp"
#include "kentos_cad/render/drawlist.hpp"
#include "kentos_cad/render/scene.hpp"
#include "kentos_cad/render/view.hpp"

#include <QDate>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMarginsF>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QPrinter>
#include <QPrinterInfo>
#include <QStandardPaths>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <memory>
#include <system_error>
#include <utility>

namespace kentos::app {
namespace {

constexpr double kMmPerInch = 25.4;

/// Puts `window` onto a paint device whose paintable area is `width_px` ×
/// `height_px` at `dpi`, through the render pipeline the canvas uses. ONE
/// function for the PDF, the printer and the preview, so the three cannot
/// disagree about what a sheet shows.
void paint_window(QPaintDevice& target, int width_px, int height_px, double dpi,
                  const core::Document& document, core::Box2 window)
{
    render::ViewTransform view;
    view.set_viewport(width_px, height_px);
    view.fit(window, 0.0);

    render::SceneOptions options;
    options.pixels_per_paper_mm = dpi / kMmPerInch;
    options.cull                = true;
    options.lod                 = true;
    options.line_weights        = true;

    render::DrawList list;
    render::build_scene(document, view, options, list);

    render::Overlay overlay;
    overlay.background_rgba = 0xFFFFFFFFu; // paper

    render::FrameContext ctx;
    ctx.width_px           = width_px;
    ctx.height_px          = height_px;
    ctx.device_pixel_ratio = 1.0f;
    ctx.target             = static_cast<QPaintDevice*>(&target);

    const std::unique_ptr<render::Backend> backend = make_preview_backend();
    backend->render(list, overlay, ctx);
}

/// The page layout a profile describes, in Qt's terms.
QPageLayout layout_of(const io::PrintProfile& profile)
{
    const QPageSize size(
        QSizeF(static_cast<double>(profile.width_mm), static_cast<double>(profile.height_mm)),
        QPageSize::Millimeter, QString(), QPageSize::ExactMatch);
    const auto margin = static_cast<double>(profile.margin_mm);
    return QPageLayout(size, profile.landscape ? QPageLayout::Landscape : QPageLayout::Portrait,
                       QMarginsF(margin, margin, margin, margin), QPageLayout::Millimeter);
}

QString utf8(const std::string& s)
{
    return QString::fromUtf8(s.data(), static_cast<int>(s.size()));
}

} // namespace

PrintService::PrintService(command::Bus& bus, const core::Document& document, QObject* parent)
    : QObject(parent), bus_(bus), document_(document)
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    path_             = dir + QStringLiteral("/yazdirma-profilleri.json");
    auto loaded       = io::PrintProfiles::load(path_.toStdString());
    if (loaded) {
        profiles_ = std::move(loaded.value());
    } else {
        // A file that does not parse is REPORTED and left alone; the built-in
        // set stands in for this run and is not written over the user's file.
        // Kept rather than echoed: the bus cannot be heard yet (see `announce`).
        profiles_ = io::PrintProfiles::builtin();
        trouble_  = loaded.error().message;
    }

    // `request` by value into the coroutine, for the reason `FileService` gives.
    bus_.on_print_request =
        [this](const command::PrintRequest& request) -> command::Task<core::Result<std::string>> {
        return this->handle(request);
    };
}

PrintService::~PrintService()
{
    bus_.on_print_request = nullptr;
}

void PrintService::announce()
{
    if (trouble_.empty()) return;
    bus_.echo(trouble_ + " Bu oturumda yerleşik profiller kullanılıyor; dosyanız olduğu gibi "
                         "bırakıldı.");
    trouble_.clear();
}

namespace {

/// WRITTEN BESIDE THE TARGET AND MOVED INTO PLACE.
///
/// A plot that was interrupted — the machine slept, the disk filled, somebody
/// closed the program — used to leave a truncated PDF at the path the user
/// named. It has the right name and a plausible size, and the way it is found
/// out is on the plotter. `io/project_writer.cpp` has done this since the native
/// format existed; this is the same answer for the other things this program
/// writes (TODOS C-05: write to a temporary, verify, then publish atomically).
///
/// `std::filesystem::rename` replaces the destination in one step on every
/// filesystem this product supports, so there is no moment at which neither the
/// old file nor the new one is there.
QString beside(const QString& target)
{
    return target + QStringLiteral(".yeni");
}

core::Status publish(const QString& temp, const QString& target)
{
    if (!QFileInfo::exists(temp) || QFileInfo(temp).size() == 0) {
        QFile::remove(temp);
        return core::err(core::ErrorCode::IoFailure, "PDF yazılamadı: " + target.toStdString());
    }
    std::error_code ec;
    std::filesystem::rename(std::filesystem::path(temp.toStdString()),
                            std::filesystem::path(target.toStdString()), ec);
    if (ec) {
        QFile::remove(temp);
        return core::err(core::ErrorCode::IoFailure,
                         "PDF yerine konamadı: " + target.toStdString() + " — " + ec.message());
    }
    return core::ok();
}

} // namespace

bool PrintService::encryptionAvailable() noexcept
{
    return io::pdf_encryption_available();
}

core::Box2 PrintService::fitWindow(const io::PrintProfile& profile, core::Box2 window)
{
    const auto pw = static_cast<double>(profile.printable_width_mm());
    const auto ph = static_cast<double>(profile.printable_height_mm());
    if (pw <= 0.0 || ph <= 0.0 || window.empty()) return window;
    const double aspect = pw / ph;
    const auto width    = static_cast<double>(window.max_x - window.min_x);
    const auto height   = static_cast<double>(window.max_y - window.min_y);
    if (width <= 0.0 || height <= 0.0) return window;

    double want_w = width;
    double want_h = height;
    if (width / height > aspect)
        want_h = width / aspect; // wider than the sheet: grow the height
    else
        want_w = height * aspect; // taller than the sheet: grow the width

    const auto cx = static_cast<double>(window.min_x + window.max_x) / 2.0;
    const auto cy = static_cast<double>(window.min_y + window.max_y) / 2.0;
    return core::Box2{core::mm_round(cx - want_w / 2.0), core::mm_round(cy - want_h / 2.0),
                      core::mm_round(cx + want_w / 2.0), core::mm_round(cy + want_h / 2.0)};
}

core::Box2 PrintService::windowFor(const io::PrintProfile& profile, core::Point2 centre,
                                   std::int64_t scale)
{
    if (scale <= 0) return {};
    // Paper millimetres times the denominator is ground millimetres, exactly:
    // 1 mm of paper at 1:1000 is 1 000 mm of ground. Integers throughout, so
    // the sheet's edges are the same on every machine (§7.3).
    const core::Mm half_w = profile.printable_width_mm() * scale / 2;
    const core::Mm half_h = profile.printable_height_mm() * scale / 2;
    if (half_w <= 0 || half_h <= 0) return {};
    return core::Box2{centre.x - half_w, centre.y - half_h, centre.x + half_w, centre.y + half_h};
}

double PrintService::scaleDenominator(const io::PrintProfile& profile, core::Box2 window)
{
    const auto pw = static_cast<double>(profile.printable_width_mm());
    if (pw <= 0.0 || window.empty()) return 0.0;
    return static_cast<double>(window.max_x - window.min_x) / pw;
}

QImage PrintService::renderPreview(const core::Document& document, const io::PrintProfile& profile,
                                   core::Box2 window, int max_px)
{
    const auto sheet_w = static_cast<double>(profile.sheet_width_mm());
    const auto sheet_h = static_cast<double>(profile.sheet_height_mm());
    if (sheet_w <= 0.0 || sheet_h <= 0.0 || max_px <= 0) return {};

    // The whole sheet fits in `max_px`; the printable area is the same picture
    // at the same pixels-per-millimetre, so the margin reads as a margin.
    const double px_per_mm = static_cast<double>(max_px) / std::max(sheet_w, sheet_h);
    const int sheet_px_w   = std::max(1, static_cast<int>(std::lround(sheet_w * px_per_mm)));
    const int sheet_px_h   = std::max(1, static_cast<int>(std::lround(sheet_h * px_per_mm)));
    const int margin_px =
        static_cast<int>(std::lround(static_cast<double>(profile.margin_mm) * px_per_mm));
    const int paint_w = std::max(1, sheet_px_w - 2 * margin_px);
    const int paint_h = std::max(1, sheet_px_h - 2 * margin_px);

    QImage paint(paint_w, paint_h, QImage::Format_ARGB32_Premultiplied);
    paint.fill(Qt::white);
    paint_window(paint, paint_w, paint_h, px_per_mm * kMmPerInch, document,
                 fitWindow(profile, window));

    QImage sheet(sheet_px_w, sheet_px_h, QImage::Format_ARGB32_Premultiplied);
    sheet.fill(Qt::white);
    QPainter p(&sheet);
    p.drawImage(QPoint(margin_px, margin_px), paint);
    // The margin as a hairline, so the printable area reads as such.
    p.setPen(QPen(QColor(0, 0, 0, 40), 1.0));
    p.setBrush(Qt::NoBrush);
    p.drawRect(QRect(margin_px, margin_px, paint_w - 1, paint_h - 1));
    p.end();
    return sheet;
}

core::Status PrintService::persist()
{
    QDir().mkpath(QFileInfo(path_).absolutePath());
    return profiles_.save(path_.toStdString());
}

command::Task<core::Result<std::string>> PrintService::handle(command::PrintRequest request)
{
    using Verb = command::PrintRequest::Verb;
    switch (request.verb) {
    case Verb::Profiles: co_return profiles_.listing();

    case Verb::SetProfile: {
        // A profile is made from the DEFAULTS plus what was given, not from the
        // current default profile: "ekle ad=X kagit=A3" means an A3 sheet in
        // the built-in orientation and resolution, whatever the office's default is.
        io::PrintProfile p;
        p.name = request.profile;
        if (!request.paper.empty()) p.paper = request.paper;
        if (request.width_mm > 0) p.width_mm = request.width_mm;
        if (request.height_mm > 0) p.height_mm = request.height_mm;
        if (request.landscape >= 0) p.landscape = request.landscape == 1;
        if (request.dpi > 0) p.dpi = request.dpi;
        if (request.margin_mm >= 0) p.margin_mm = request.margin_mm;
        if (auto st = profiles_.upsert(std::move(p)); !st) co_return st.error();
        if (auto st = persist(); !st) co_return st.error();
        emit profilesChanged();
        co_return "Profil kaydedildi: " +
            io::describe_print_profile(*profiles_.find(request.profile)) + "\n" +
            profiles_.listing();
    }

    case Verb::RemoveProfile: {
        if (auto st = profiles_.remove(request.profile); !st) co_return st.error();
        if (auto st = persist(); !st) co_return st.error();
        emit profilesChanged();
        co_return "Profil silindi: " + request.profile + "\n" + profiles_.listing();
    }

    case Verb::SetDefault: {
        if (auto st = profiles_.set_default(request.profile); !st) co_return st.error();
        if (auto st = persist(); !st) co_return st.error();
        emit profilesChanged();
        co_return "Varsayılan profil: " + profiles_.default_name() + "\n" + profiles_.listing();
    }

    case Verb::ToPdf:
    case Verb::ToPrinter: {
        // AN OUTPUT LAYOUT IS ITS OWN SHEET. It carries paper, margin, resolution and a
        // map frame that knows where it looks, so it takes neither a profile nor
        // a window and goes down its own path (`layout_render.hpp`).
        if (!request.layout.empty()) co_return printLayout(request);

        auto resolved = profiles_.resolve(request);
        if (!resolved) co_return resolved.error();

        // A CENTRE AND A SCALE BECOME THE WINDOW HERE, because the third number
        // is the paper's printable size and this is the side that holds it.
        command::PrintRequest sheet = request;
        if (sheet.has_centre) {
            sheet.window = windowFor(resolved.value(), sheet.centre, sheet.scale);
            if (sheet.window.empty())
                co_return core::err(core::ErrorCode::InvalidArgument,
                                    "Bu ölçekte kâğıda sığacak bir alan çıkmıyor: olcek=" +
                                        std::to_string(sheet.scale) + ".");
        }
        if (sheet.window.empty())
            co_return core::err(core::ErrorCode::InvalidArgument,
                                "Yazdırma penceresi boş; iki köşe ya da bir merkez verin.");
        if (sheet.verb == Verb::ToPdf) co_return toPdf(sheet, resolved.value());
        co_return toPrinter(sheet, resolved.value());
    }
    }
    co_return core::err(core::ErrorCode::Internal, "İşlenmemiş yazdırma isteği.");
}

core::Result<std::string> PrintService::printLayout(const command::PrintRequest& request)
{
    const core::Layout* sheet = document_.layouts().find(request.layout);
    if (sheet == nullptr)
        return core::err(core::ErrorCode::NotFound,
                         "Çıktı yerleşimi yok: '" + request.layout + "'.");
    if (sheet->pages.empty())
        return core::err(core::ErrorCode::InvalidArgument,
                         "'" + sheet->name + "' yerleşiminin hiç sayfası yok.");

    LayoutFacts facts;
    facts.sheet = utf8(sheet->name);
    facts.project =
        bus_.on_current_file ? QFileInfo(utf8(bus_.on_current_file())).fileName() : QString();
    facts.crs  = utf8(document_.crs().id());
    facts.date = QDate::currentDate().toString(QStringLiteral("dd.MM.yyyy"));

    // THE PAGE SIZE IS THE LAYOUT'S OWN, in paper millimetres, and the margin is
    // NOT given to Qt: the layout places its items in the full page and draws
    // its own margin guide. A Qt margin here would inset the whole sheet a
    // second time and move every item (`paint_layout_page`).
    const auto device_layout = [](const core::LayoutPage& one) {
        const QPageSize size(QSizeF(one.w / 1000.0, one.h / 1000.0), QPageSize::Millimeter,
                             QString(), QPageSize::ExactMatch);
        return QPageLayout(size, QPageLayout::Portrait, QMarginsF(0, 0, 0, 0),
                           QPageLayout::Millimeter);
    };
    const QPageLayout page = device_layout(sheet->pages.front());

    // WHAT THE SHEET COULD NOT HONOUR WHILE IT WAS BEING DRAWN — a table that ran
    // out of box, an item pointing at a map that is gone. Collected while
    // painting, because that is when it is known, and said in the answer.
    std::vector<std::string> trouble;

    // ---- the atlas, resolved before anything is drawn -----------------------
    //
    // A run that would produce nothing has to say so now rather than write zero
    // files and report success (TODOS L-10). Resolving first also means the page
    // count is known, which is what makes one document of many pages possible.
    const std::vector<core::AtlasTarget> targets = sheet->atlas.coverage_layer.empty()
                                                       ? std::vector<core::AtlasTarget>{}
                                                       : core::atlas_targets(document_, *sheet);
    if (!sheet->atlas.coverage_layer.empty() && targets.empty())
        return core::err(core::ErrorCode::NotFound, "'" + sheet->name + "' atlası '" +
                                                        sheet->atlas.coverage_layer +
                                                        "' katmanında basılacak nesne bulamadı.");

    // THE MAP FRAME IS AIMED PER TARGET, and the layout itself is never touched:
    // a print that edited the document would be a print that needs an undo.
    core::Layout aimed = *sheet;
    const auto aim_at  = [&](const core::AtlasTarget& one) {
        core::LayoutItem* map = nullptr;
        for (core::LayoutItem& item : aimed.items)
            if (item.kind == core::LayoutItemKind::Map && map == nullptr) map = &item;
        if (map == nullptr) return;
        const core::Mm pad_x = one.bounds.width() * sheet->atlas.margin_percent / 100;
        const core::Mm pad_y = one.bounds.height() * sheet->atlas.margin_percent / 100;
        map->extent          = core::Box2{one.bounds.min_x - pad_x, one.bounds.min_y - pad_y,
                                 one.bounds.max_x + pad_x, one.bounds.max_y + pad_y};
        // THE DECLARED SCALE STILL WINS when there is one: an atlas at 1:1000 is
        // a hundred sheets at 1:1000, which is the point of declaring it.
    };

    // ONE RUN OVER EVERY TARGET, or one run over nothing when this is not an
    // atlas — written as one loop so the ordinary sheet and the hundred-parcel
    // one go down the same path and cannot drift apart.
    const std::size_t runs = targets.empty() ? 1 : targets.size();

    const auto draw = [&](QPaintDevice& device, int resolution) {
        QPainter painter(&device);
        bool started = false;
        for (std::size_t run = 0; run < runs; ++run) {
            if (!targets.empty()) {
                aim_at(targets[run]);
                facts.sheet = utf8(sheet->name) + QStringLiteral(" — ") + utf8(targets[run].name);
                // THE PARCEL'S OWN FIELDS, so `Ada <ada>, Parsel <parsel>` on the
                // title block says what THIS sheet is about rather than what the
                // drawing is about.
                facts.fields = targets[run].fields;
            }
            for (std::size_t i = 0; i < sheet->pages.size(); ++i) {
                if (started) {
                    // THE SIZE IS SET BEFORE THE PAGE IS STARTED, and per page.
                    // Setting it once from `pages.front()` wrote every page of a
                    // mixed A4/A3 layout at A4: the second page's content was drawn
                    // at A3 dimensions into an A4 MediaBox and ran off the paper.
                    // Qt applies a page layout to the NEXT page, so the order here is
                    // load-bearing.
                    if (auto* writer = dynamic_cast<QPdfWriter*>(&device); writer != nullptr) {
                        writer->setPageLayout(device_layout(sheet->pages[i]));
                        writer->newPage();
                    } else if (auto* printer = dynamic_cast<QPrinter*>(&device);
                               printer != nullptr) {
                        printer->setPageLayout(device_layout(sheet->pages[i]));
                        printer->newPage();
                    }
                }
                const core::LayoutPage& one = sheet->pages[i];
                const double w_px           = one.w / 1000.0 / kMmPerInch * resolution;
                const double h_px           = one.h / 1000.0 / kMmPerInch * resolution;
                paint_layout_page(painter, QRectF(0, 0, w_px, h_px), document_, aimed,
                                  static_cast<int>(i), static_cast<double>(resolution), facts,
                                  /*margin_guide=*/false, &trouble);
                started = true;
            }
        }
    };

    if (request.verb == command::PrintRequest::Verb::ToPdf) {
        const QString path = utf8(request.path);
        if (path.isEmpty())
            return core::err(core::ErrorCode::InvalidArgument, "PDF dosyasının yolu boş.");
        const QFileInfo target(path);
        if (!target.absoluteDir().exists())
            return core::err(core::ErrorCode::IoFailure,
                             "PDF yazılamadı: dizin yok — " + target.absolutePath().toStdString());

        const QString temp = beside(path);
        QFile::remove(temp);
        {
            QPdfWriter writer(temp);
            writer.setPageLayout(page);
            writer.setResolution(sheet->dpi > 0 ? sheet->dpi : 300);
            writer.setCreator(QStringLiteral("KentOSCad"));
            writer.setTitle(request.title.empty() ? utf8(sheet->name) : utf8(request.title));
            draw(writer, writer.resolution());
        }
        // VERIFIED, THEN PUBLISHED. Until this line the user's path still holds
        // whatever it held before — an interrupted plot leaves the old sheet
        // rather than a truncated new one (TODOS C-05).
        if (auto st = publish(temp, path); !st) return st.error();

        const core::LayoutItem* map = sheet->first_map();
        // THE FIRST PAGE'S SIZE, AND "karma" WHEN THEY DIFFER. Printing one size
        // for a layout that holds two would be a report of something that did not
        // happen.
        const core::LayoutPage& first = sheet->pages.front();
        const bool mixed =
            std::any_of(sheet->pages.begin(), sheet->pages.end(), [&](const core::LayoutPage& one) {
                return one.w != first.w || one.h != first.h;
            });
        std::string said =
            "Çıktı yerleşimi yazıldı: " + path.toStdString() + " — " + sheet->name + ", " +
            (mixed
                 ? std::string("karma sayfa boyu")
                 : std::to_string(first.w / 1000) + "×" + std::to_string(first.h / 1000) + " mm") +
            ", " + std::to_string(sheet->pages.size()) + " sayfa";
        if (map != nullptr && core::map_scale(*map) > 0)
            said += ", ölçek 1:" + std::to_string(core::map_scale(*map));

        // REPORTED WITH THE SUCCESS, not instead of it. The sheet printed; these
        // are what it could not honour, and a caller that reports success without
        // them reports something untrue (TODOS C-03).
        if (!targets.empty()) said += " — atlas: " + std::to_string(targets.size()) + " nesne";

        for (const std::string& one : trouble)
            said += "\n  · " + one;
        return said;
    }

    QPrinterInfo info = request.printer.empty() ? QPrinterInfo::defaultPrinter()
                                                : QPrinterInfo::printerInfo(utf8(request.printer));
    if (info.isNull())
        return core::err(core::ErrorCode::NotFound,
                         request.printer.empty() ? "Sistemde varsayılan yazıcı tanımlı değil."
                                                 : "Yazıcı bulunamadı: '" + request.printer + "'.");
    QPrinter printer(info, QPrinter::HighResolution);
    printer.setPageLayout(page);
    printer.setResolution(sheet->dpi > 0 ? sheet->dpi : 300);
    draw(printer, printer.resolution());
    return "Çıktı yerleşimi yazıcıya gönderildi: " + info.printerName().toStdString() + " — " +
           sheet->name;
}

core::Result<std::string> PrintService::toPdf(const command::PrintRequest& request,
                                              const io::PrintProfile& profile)
{
    const QString path = utf8(request.path);
    if (path.isEmpty())
        return core::err(core::ErrorCode::InvalidArgument, "PDF dosyasının yolu boş.");
    const QFileInfo target(path);
    if (!target.absoluteDir().exists())
        return core::err(core::ErrorCode::IoFailure,
                         "PDF yazılamadı: dizin yok — " + target.absolutePath().toStdString());

    io::PdfEncryption finish;
    finish.user_password  = request.user_password;
    finish.owner_password = request.owner_password;
    finish.allow_print    = request.allow_print;
    finish.allow_copy     = request.allow_copy;
    finish.allow_modify   = request.allow_modify;
    finish.author         = request.author;
    const bool post       = finish.encrypts() || !finish.author.empty();
    if (post && !io::pdf_encryption_available())
        return core::err(core::ErrorCode::Unsupported,
                         "Bu yapı PDF şifreleme ve yazar alanını içermiyor (KENTOS_WITH_QPDF). "
                         "sifre, sahip_sifresi ve yazar olmadan yazın.");

    // Qt writes the plain file; when a password or an author was asked for,
    // it goes to a sibling first and qpdf writes the final one.
    // ALWAYS A SIBLING, whether or not qpdf runs afterwards: the encrypted path
    // needed one anyway, and the plain one wrote straight onto the user's file.
    const QString plain = post ? path + QStringLiteral(".kentos-tmp") : beside(path);
    QFile::remove(plain);
    {
        QPdfWriter writer(plain);
        writer.setPageLayout(layout_of(profile));
        writer.setResolution(static_cast<int>(profile.dpi));
        writer.setCreator(QStringLiteral("KentOSCad"));
        if (!request.title.empty()) writer.setTitle(utf8(request.title));
        const QRect paint = writer.pageLayout().paintRectPixels(writer.resolution());
        if (paint.isEmpty()) {
            QFile::remove(plain);
            return core::err(core::ErrorCode::InvalidArgument,
                             "Kâğıtta yazdırılacak alan kalmadı; kenar boşluğunu küçültün.");
        }
        paint_window(writer, paint.width(), paint.height(),
                     static_cast<double>(writer.resolution()), document_,
                     fitWindow(profile, request.window));
    }
    if (!QFileInfo::exists(plain) || QFileInfo(plain).size() == 0) {
        QFile::remove(plain);
        return core::err(core::ErrorCode::IoFailure, "PDF yazılamadı: " + plain.toStdString());
    }
    if (post) {
        // qpdf READS the plain sibling and WRITES the target, which is already a
        // publish: the user's path is untouched until qpdf succeeds.
        auto st = io::pdf_encrypt(plain.toStdString(), path.toStdString(), finish);
        QFile::remove(plain);
        if (!st) return st.error();
    } else if (auto st = publish(plain, path); !st) {
        return st.error();
    }

    const double scale = scaleDenominator(profile, fitWindow(profile, request.window));
    std::string said   = "PDF yazıldı: " + path.toStdString() + " — " +
                       io::describe_print_profile(profile) +
                       ", ölçek 1:" + std::to_string(static_cast<long long>(std::llround(scale)));
    if (finish.encrypts()) said += ", şifreli";
    return said;
}

core::Result<std::string> PrintService::toPrinter(const command::PrintRequest& request,
                                                  const io::PrintProfile& profile)
{
    QPrinterInfo info = request.printer.empty() ? QPrinterInfo::defaultPrinter()
                                                : QPrinterInfo::printerInfo(utf8(request.printer));
    if (info.isNull()) {
        std::string names;
        for (const QString& n : QPrinterInfo::availablePrinterNames())
            names += (names.empty() ? "" : ", ") + n.toStdString();
        return core::err(core::ErrorCode::NotFound,
                         request.printer.empty()
                             ? "Sistemde varsayılan yazıcı yok. Yazıcılar: " +
                                   (names.empty() ? std::string("(hiç)") : names)
                             : "Yazıcı bulunamadı: '" + request.printer + "'. Yazıcılar: " +
                                   (names.empty() ? std::string("(hiç)") : names));
    }
    QPrinter printer(info, QPrinter::HighResolution);
    printer.setPageLayout(layout_of(profile));
    printer.setResolution(static_cast<int>(profile.dpi));
    printer.setDocName(request.title.empty() ? QStringLiteral("KentOSCad") : utf8(request.title));
    const QRect paint = printer.pageLayout().paintRectPixels(printer.resolution());
    if (paint.isEmpty())
        return core::err(core::ErrorCode::InvalidArgument,
                         "Kâğıtta yazdırılacak alan kalmadı; kenar boşluğunu küçültün.");
    paint_window(printer, paint.width(), paint.height(), static_cast<double>(printer.resolution()),
                 document_, fitWindow(profile, request.window));

    const double scale = scaleDenominator(profile, fitWindow(profile, request.window));
    return "Yazıcıya gönderildi: " + info.printerName().toStdString() + " — " +
           io::describe_print_profile(profile) +
           ", ölçek 1:" + std::to_string(static_cast<long long>(std::llround(scale)));
}

} // namespace kentos::app
