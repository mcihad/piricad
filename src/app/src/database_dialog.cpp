// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/app/database_dialog.hpp"

#include "kentos_cad/app/controller.hpp"
#include "kentos_cad/app/fields.hpp"
#include "kentos_cad/app/widgets.hpp"

#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/core/document.hpp"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
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
    : DialogFrame(parent), controller_(controller)
{
    setHeading(Glyph::Cloud, tr("Veritabanı"), tr("— PostGIS"));
    setModal(false);
    setMinimumSize(820, 580);
    resize(900, 660);

    auto* body   = new QWidget(this);
    auto* layout = new QVBoxLayout(body);
    layout->setContentsMargins(18, 14, 18, 12);
    layout->setSpacing(14);
    layout->addWidget(buildConnection());
    layout->addWidget(buildContents(), 1);
    setBody(body);

    // The frame's footer, not a `QDialogButtonBox`. That box arrived wearing
    // Qt's English "Close" until the label was forced, and the platform's idea
    // of a button rather than the standard's; a `Button` of the secondary role
    // is what a close action at the end of a footer is.
    auto* close = new Button(ButtonRole::Secondary, tr("Kapat"), std::nullopt, this);
    connect(close, &QPushButton::clicked, this, &QDialog::reject);
    footer()->addWidget(close);

    updateEnabled();
}

QWidget* DatabaseDialog::buildConnection()
{
    auto* box  = new QWidget(this);
    auto* rows = new QVBoxLayout(box);
    rows->setContentsMargins(0, 0, 0, 0);
    rows->setSpacing(10);

    // A section heading from the set where a `QGroupBox` used to draw a frame of
    // its own around the fields. The form grammar of `form_örnek.png` is a
    // heading with a rule, then labelled rows — label above control.
    rows->addWidget(
        new FormSection(tr("PostGIS bağlantısı"), tr("parola hiçbir yere kaydedilmez"), box));

    // Filled from the APPLICATION settings, which is where the last connection
    // was remembered. Not from the project: a drawing mailed to a colleague must
    // not carry a pointer at a database.
    command::Bus& bus = controller_.bus();

    const auto text_of = [&bus](const char* id) {
        const std::string_view v = bus.setting(id).as_text();
        return QString::fromUtf8(v.data(), static_cast<int>(v.size()));
    };

    // The four text boxes are the shell's own `QLineEdit`, styled by the one
    // sheet at the standard's regular height; `Field` is for values with a kind,
    // and a host name has none. The port has one — a bounded whole number — and
    // gets the set's number input, its bounds as its validator.
    const auto textBox = [box](const QString& text) {
        auto* edit = new QLineEdit(text, box);
        edit->setFixedHeight(static_cast<int>(ControlSize::Regular));
        return edit;
    };

    host_ = textBox(text_of("core.veritabani.sunucu"));
    host_->setPlaceholderText(tr("localhost — ya da tam bir bağlantı dizesi"));
    host_->setAccessibleName(tr("PostGIS sunucu adresi"));
    host_->setToolTip(tr("Sunucu adresi. İçinde '=' geçen tam bir libpq bağlantı dizesi "
                         "yazarsanız diğer alanlar yok sayılır."));

    FieldSpec portSpec   = number_of(1, 65535);
    portSpec.placeholder = QStringLiteral("5432");
    port_                = new Field(portSpec, box);
    port_->setFixedHeight(static_cast<int>(ControlSize::Regular));
    port_->setValue(QString::number(bus.setting("core.veritabani.port").as_int()));
    port_->setAccessibleName(tr("PostGIS bağlantı noktası"));

    database_ = textBox(text_of("core.veritabani.ad"));
    database_->setPlaceholderText(tr("veritabanı adı"));
    database_->setAccessibleName(tr("PostGIS veritabanı adı"));

    user_ = textBox(text_of("core.veritabani.kullanici"));
    user_->setPlaceholderText(tr("kullanıcı adı"));
    user_->setAccessibleName(tr("PostGIS kullanıcı adı"));

    password_ = textBox(QString());
    password_->setEchoMode(QLineEdit::Password);
    password_->setPlaceholderText(tr("boş bırakın — ~/.pgpass ya da PGPASSWORD"));
    password_->setAccessibleName(tr("PostGIS parolası"));
    password_->setToolTip(tr("Parola HİÇBİR YERE kaydedilmez. Her açılışta boş başlar. "
                             "Kalıcı olması için ~/.pgpass dosyasını kullanın."));

    // The address wide with the port narrow beside it, then the three names in
    // one row: two lines instead of five, and every label above its own box.
    auto* grid = new QGridLayout;
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(8);
    grid->addWidget(new FormRow(tr("Adres"), host_, box), 0, 0, 1, 2);
    grid->addWidget(new FormRow(tr("Port"), port_, box), 0, 2);
    grid->addWidget(new FormRow(tr("Veritabanı"), database_, box), 1, 0);
    grid->addWidget(new FormRow(tr("Kullanıcı"), user_, box), 1, 1);
    grid->addWidget(new FormRow(tr("Parola"), password_, box), 1, 2);
    grid->setColumnStretch(0, 2);
    grid->setColumnStretch(1, 2);
    grid->setColumnStretch(2, 1);
    rows->addLayout(grid);

    // ONE PRIMARY on this screen, and it is `Bağlan`: nothing else here matters
    // until it has been pressed. Disconnecting is a secondary; re-reading the
    // lists is a ghost with the set's own arrows, where the platform's reload
    // icon used to arrive in whatever colour the platform had.
    auto* actions = new QHBoxLayout;
    actions->setSpacing(8);
    connect_    = new Button(ButtonRole::Primary, tr("Bağlan"), std::nullopt, box);
    disconnect_ = new Button(ButtonRole::Secondary, tr("Bağlantıyı Kes"), std::nullopt, box);
    refresh_    = new Button(ButtonRole::Ghost, tr("Yenile"), Glyph::Refresh, box);
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
    auto* left     = new QWidget(split);
    auto* leftRows = new QVBoxLayout(left);
    leftRows->setContentsMargins(0, 0, 8, 0);
    leftRows->setSpacing(8);
    leftRows->addWidget(new FormSection(tr("Mekansal tablolar"), QString(), left));

    tables_ = new QListWidget(left);
    tables_->setToolTip(tr("Sunucudaki geometri sütunu olan tablolar."));
    tables_->setAccessibleName(tr("PostGIS mekansal tabloları"));
    tables_->setAlternatingRowColors(true);
    leftRows->addWidget(tables_, 1);

    writeLayer_ = new Button(ButtonRole::Secondary, tr("Etkin Katmanı Yaz…"), Glyph::Export, left);
    writeLayer_->setToolTip(tr("VERİTABANI katmanyaz — katmanı bir mekansal tablo olarak yazar. "
                               "Aynı adlı tablo varsa YERİNE yazılır."));
    auto* leftButtons = new QHBoxLayout;
    leftButtons->addWidget(writeLayer_);
    leftButtons->addStretch(1);
    leftRows->addLayout(leftButtons);

    // ---- projects ----
    auto* right     = new QWidget(split);
    auto* rightRows = new QVBoxLayout(right);
    rightRows->setContentsMargins(8, 0, 0, 0);
    rightRows->setSpacing(8);
    rightRows->addWidget(new FormSection(tr("Kayıtlı KentOSCad projeleri"), QString(), right));

    projects_ = new QListWidget(right);
    projects_->setToolTip(tr("Bu veritabanına kaydedilmiş KentOSCad projeleri."));
    projects_->setAccessibleName(tr("Kayıtlı KentOSCad projeleri"));
    projects_->setAlternatingRowColors(true);
    rightRows->addWidget(projects_, 1);

    // The destructive one wears the destructive role and stands apart from the
    // two it could be mistaken for. Deleting a project has no undo (the command
    // says so, and asks); a button that looked like `Projeyi Aç` would not.
    auto* buttons = new QHBoxLayout;
    buttons->setSpacing(8);
    saveProject_ = new Button(ButtonRole::Secondary, tr("Projeyi Kaydet…"), Glyph::Save, right);
    openProject_ = new Button(ButtonRole::Secondary, tr("Projeyi Aç"), Glyph::Open, right);
    dropProject_ = new Button(ButtonRole::Danger, tr("Projeyi Sil"), Glyph::Trash, right);
    buttons->addWidget(saveProject_);
    buttons->addWidget(openProject_);
    buttons->addStretch(1);
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

QString DatabaseDialog::port() const
{
    const QString typed = port_->value().trimmed();
    return typed.isEmpty() ? QStringLiteral("5432") : typed;
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
                      .arg(port());
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
        controller_.runLine(QStringLiteral("TERCİH veritabani_port %1").arg(port()),
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
