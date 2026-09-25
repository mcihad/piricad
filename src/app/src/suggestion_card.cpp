// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/app/suggestion_card.hpp"

#include "kentos_cad/app/ai_service.hpp"
#include "kentos_cad/app/tokens.hpp"

#include "kentos_cad/command/session.hpp"

#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QStyle>
#include <QVBoxLayout>

namespace kentos::app {
namespace {

/// The Turkish word for a settled state, for the card's own outcome line.
QString stateWord(ai::PlanState state)
{
    switch (state) {
    case ai::PlanState::Pending: return SuggestionCard::tr("bekliyor");
    case ai::PlanState::Applied: return SuggestionCard::tr("uygulandı");
    case ai::PlanState::Rejected: return SuggestionCard::tr("reddedildi");
    case ai::PlanState::Withdrawn: return SuggestionCard::tr("geri çekildi");
    case ai::PlanState::Failed: return SuggestionCard::tr("uygulanamadı");
    case ai::PlanState::Running: return SuggestionCard::tr("uygulanıyor");
    }
    return {};
}

} // namespace

SuggestionCard::SuggestionCard(AiService& service, const QString& planId, QWidget* parent)
    : QWidget(parent), service_(service), plan_(planId)
{
    setObjectName(QStringLiteral("suggestionCard"));

    auto* column = new QVBoxLayout(this);
    column->setContentsMargins(10, 8, 10, 8);
    column->setSpacing(6);

    const ai::Plan* plan = service_.plans().find(plan_.toStdString());
    pending_             = plan != nullptr && plan->state == ai::PlanState::Pending;

    // WHAT THIS CARD IS ABOUT TO SHOW, remembered so the approval can be bound to
    // it. Everything below draws the plan's command lines; if the plan changes
    // before the button is pressed — the client that filed it may append a step —
    // the person would be approving lines they never saw (TODOS S-04).
    shown_content_ = plan != nullptr ? plan->content_fingerprint() : 0;

    auto* head = new QHBoxLayout;
    head->setContentsMargins(0, 0, 0, 0);
    head->setSpacing(6);
    auto* title = new QLabel(tr("Öneri %1 · %2 adım")
                                 .arg(plan_)
                                 .arg(plan != nullptr ? static_cast<int>(plan->steps.size()) : 0),
                             this);
    title->setObjectName(QStringLiteral("bubbleWho"));
    head->addWidget(title);
    // THE LABEL IS PART OF THE CARD, not a decoration on it (ai.md R17).
    mark_ = new Badge(tr("ÖNERİ"), Tone::Accent, this);
    head->addWidget(mark_);
    head->addStretch(1);
    column->addLayout(head);

    if (plan == nullptr) {
        auto* gone = new QLabel(tr("Bu öneri artık defterde yok."), this);
        gone->setObjectName(QStringLiteral("formHelp"));
        column->addWidget(gone);
        pending_ = false;
        return;
    }

    // THE STEPS AS COMMAND LINES. Not a paraphrase and not the raw arguments: the
    // line is what the engineer can read, check against the drawing, and type
    // themselves — which is the whole reason this program's clients are equal
    // (Article 1.2). One `QLabel` per step, mono, so a long line wraps where a
    // reader can still count the arguments.
    for (const ai::PlanStep& step : plan->steps) {
        auto* line = new QLabel(QString::fromStdString(step.line), this);
        line->setObjectName(QStringLiteral("bubbleBodyMono"));
        line->setWordWrap(true);
        line->setTextInteractionFlags(Qt::TextSelectableByMouse);
        column->addWidget(line);
    }

    // WHAT IT WOULD DO, found out by running it and taking it back (TODOS F-05):
    // counted here and outlined, dashed, on the canvas — so the engineer
    // approves a result they have seen rather than a list of commands. A plan
    // that would stop half way says where, in the danger ink. The line has its
    // place whether or not the preview is in yet: a plan filed while a job held
    // the drawing is previewed when the job returns (`AiService::previewWaiting`).
    if (pending_) {
        preview_ = new QLabel(this);
        preview_->setObjectName(QStringLiteral("formHelp"));
        preview_->setWordWrap(true);
        preview_->setAccessibleName(tr("Önizleme"));
        preview_->setVisible(false);
        column->addWidget(preview_);
        showPreview();
        connect(&service_, &AiService::previewChanged, this, [this] {
            if (pending_) showPreview();
        });
    }

    // WHERE THE NUMBERS CAME FROM. A coordinate in an applied plan traces to a
    // recorded tool-call result and to nothing the model wrote (CLAUDE.md 5.8),
    // and the person signing is the person who should be shown that.
    std::vector<QString> handles;
    for (const ai::PlanStep& step : plan->steps)
        for (const std::string& handle : step.handles)
            handles.push_back(QString::fromStdString(handle));
    if (!handles.empty()) {
        QStringList named;
        for (const QString& handle : handles)
            if (!named.contains(handle)) named << handle;
        auto* trace =
            new QLabel(tr("Koordinat kaynağı: %1").arg(named.join(QStringLiteral(", "))), this);
        trace->setObjectName(QStringLiteral("formHelp"));
        trace->setWordWrap(true);
        column->addWidget(trace);
    }

    // WHAT THE CLIENT ASSUMED to compose it, in its own words (`varsayimlar`):
    // work that goes ahead on an assumption says which, before the person
    // approves it and in the record after (TODOS A-03).
    if (const std::vector<std::string> noted = plan->assumptions(); !noted.empty()) {
        QStringList lines;
        for (const std::string& one : noted)
            lines << QStringLiteral("• ") + QString::fromStdString(one);
        auto* assumed =
            new QLabel(tr("Varsayımlar:\n%1").arg(lines.join(QStringLiteral("\n"))), this);
        assumed->setObjectName(QStringLiteral("formHelp"));
        assumed->setWordWrap(true);
        column->addWidget(assumed);
    }

    // AND WHY IT WAITS, when the policy was asked and gave it to a person.
    if (pending_ && !plan->waiting_reason.empty()) {
        auto* why = new QLabel(
            tr("Onay bekliyor: %1").arg(QString::fromStdString(plan->waiting_reason)), this);
        why->setObjectName(QStringLiteral("formHelp"));
        why->setWordWrap(true);
        column->addWidget(why);
    }

    auto* who = new QLabel(
        tr("İsteyen: %1%2")
            .arg(plan->requester.empty() ? tr("(bilinmiyor)")
                                         : QString::fromStdString(plan->requester),
                 plan->model.empty() ? QString()
                                     : tr(" · model: %1").arg(QString::fromStdString(plan->model))),
        this);
    who->setObjectName(QStringLiteral("formHelp"));
    who->setWordWrap(true);
    column->addWidget(who);

    // THE DRAWING MOVED UNDER IT. A plan is composed against one document
    // revision; if the engineer has drawn since, the object keys in it may mean
    // something else, and that is a fact the card must state rather than resolve.
    if (plan->revision != service_.revision()) {
        auto* drift = new Banner(Tone::Warn, tr("Çizim bu öneriden sonra değişti"),
                                 tr("Öneri %1 numaralı sürüme göre hazırlandı, çizim şimdi %2. "
                                    "Uygulamadan önce adımları gözden geçirin.")
                                     .arg(plan->revision)
                                     .arg(service_.revision()),
                                 this);
        column->addWidget(drift);
    }

    actions_  = new QWidget(this);
    auto* row = new QHBoxLayout(actions_);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(8);
    row->addStretch(1);

    // `Reddet` FIRST, `Uygula` LAST, and `Uygula` is the only primary on the
    // card (the component standard's one-primary rule). The word is `Uygula`:
    // `Onayla` would be the engineer's seal, which this program does not give
    // (ai.md P4).
    reject_ = new Button(ButtonRole::Secondary, tr("Reddet"), Glyph::Close, actions_);
    reject_->setControlSize(ControlSize::Compact);
    row->addWidget(reject_);
    apply_ = new Button(ButtonRole::Primary, tr("Uygula"), Glyph::Check, actions_);
    apply_->setControlSize(ControlSize::Compact);
    apply_->setToolTip(tr("Adımlar tek işlem olarak uygulanır; tek Ctrl+Z ile geri alınır."));
    row->addWidget(apply_);
    column->addWidget(actions_);

    connect(apply_, &QPushButton::clicked, this, [this] { (void)decide(true); });
    connect(reject_, &QPushButton::clicked, this, [this] { (void)decide(false); });

    outcome_ = new QLabel(this);
    outcome_->setObjectName(QStringLiteral("formHelp"));
    outcome_->setWordWrap(true);
    outcome_->setVisible(false);
    column->addWidget(outcome_);

    if (!pending_)
        showOutcome(plan->state == ai::PlanState::Applied, QString::fromStdString(plan->refusal));
}

QString SuggestionCard::operatorName() const
{
    const std::string named{service_.appSettings().get("core.ai.sorumlu").as_text()};
    if (!named.empty()) return QString::fromStdString(named);

    // NOT A SIGNATURE, AND THE RECORD SHOULD SAY SO. When nobody has named the
    // responsible engineer, the audit line carries the operating system's user
    // name marked as such, rather than an empty field that reads like a record
    // with the name removed.
    const QString user = qEnvironmentVariable("USER", qEnvironmentVariable("USERNAME"));
    return user.isEmpty() ? tr("(adsız kullanıcı)")
                          : tr("%1 (işletim sistemi kullanıcısı)").arg(user);
}

core::Status SuggestionCard::decide(bool apply)
{
    if (!pending_)
        return core::err(core::ErrorCode::InvalidArgument, "Bu öneri zaten karara bağlandı.");

    // NOT WHILE A JOB HOLDS THE DRAWING (`command::Bus::writable`). Applied now,
    // the plan would be refused and a refused plan cannot be applied again, so
    // it stays waiting — the card says why, and the same button works once the
    // job is over. Rejecting touches nothing and is always open.
    if (apply) {
        if (const auto open = service_.writable(); !open) {
            if (outcome_ != nullptr) {
                outcome_->setText(QString::fromStdString(open.error().message));
                outcome_->setProperty("tone", QVariant());
                outcome_->style()->unpolish(outcome_);
                outcome_->style()->polish(outcome_);
                outcome_->setVisible(true);
            }
            return open;
        }
        if (outcome_ != nullptr) outcome_->setVisible(false);
    }

    // ==== ONE OF THE TWO CALLS TO `ai::Gate::approve` IN THE PROGRAM =========
    // Everything above this line is words on a screen; this is the line that
    // turns a person's CLICK into a value nothing else can make. The other call
    // is the policy path, which acts on permission the same person gave
    // beforehand; `ci-gate-ai.sh` fails the build on a third (ai.md P15).
    //
    // THE POLICY IN FORCE AT THE MOMENT OF THE CLICK travels with the approval,
    // rather than being looked up when the record is written: the setting can
    // change between the two, and what a record must preserve is the rule the
    // decision was made UNDER (TODOS S-06).
    const ai::Approval approval = service_.gate().approve(
        plan_.toStdString(), operatorName().toStdString(),
        apply ? ai::Decision::Apply : ai::Decision::Reject,
        QDateTime::currentDateTimeUtc().toMSecsSinceEpoch(),
        std::string(service_.appSettings().get("core.ai.onay_politikasi").as_text()),
        shown_content_);
    // ========================================================================

    const core::Status settled = service_.settle(approval);
    pending_                   = false;
    showOutcome(apply && settled.ok(),
                settled.ok() ? QString() : QString::fromStdString(settled.error().message));
    emit SuggestionCard::settled(plan_, apply && settled.ok());
    return settled;
}

core::Status SuggestionCard::probeApply()
{
    return decide(true);
}

core::Status SuggestionCard::probeReject()
{
    return decide(false);
}

QString SuggestionCard::previewTextForProbe() const
{
    return preview_ != nullptr && preview_->isVisible() ? preview_->text() : QString();
}

void SuggestionCard::showPreview()
{
    const ai::Plan* plan = service_.plans().find(plan_.toStdString());
    if (preview_ == nullptr || plan == nullptr || plan->preview.is_null()) return;
    const core::Json& seen   = plan->preview;
    const core::Json* stops  = seen.find("duracagi_adim");
    const core::Json* why    = seen.find("hata");
    const core::Json* counts = seen.find("degisiklik_ozeti");
    QString text;
    if (stops != nullptr)
        text = tr("Uygulanırsa %1. adımda duracak: %2 Bütünüyle geri alınacak.")
                   .arg(stops->as_int())
                   .arg(why != nullptr ? QString::fromStdString(why->as_string()) : QString());
    else if (counts != nullptr)
        text = tr("Uygulanırsa: %1.").arg(QString::fromStdString(counts->as_string()));
    else
        text = tr("Uygulanırsa çizimde bir şey değişmeyecek.");
    if (const core::Json* skipped = seen.find("calistirilmayan"); skipped != nullptr)
        for (const core::Json& one : skipped->as_array())
            text += QStringLiteral("\n") +
                    tr("%1. adım önizlenmedi: %2.")
                        .arg(one.find("adim")->as_int())
                        .arg(QString::fromStdString(one.find("neden")->as_string()));
    // IN THE ACCENT INK THE CANVAS DRAWS THE RESULT IN, so the sentence and the
    // dashed outlines read as one answer; the danger ink when it stops.
    preview_->setText(text);
    preview_->setProperty("tone",
                          stops != nullptr ? QStringLiteral("danger") : QStringLiteral("accent"));
    preview_->style()->unpolish(preview_);
    preview_->style()->polish(preview_);
    preview_->setVisible(true);
}

void SuggestionCard::showOutcome(bool applied, const QString& trouble)
{
    if (actions_ != nullptr) actions_->setVisible(false);
    // WHAT IT WOULD HAVE DONE is no longer the question once it is decided.
    if (preview_ != nullptr) preview_->setVisible(false);
    if (outcome_ == nullptr) return;

    const ai::Plan* plan = service_.plans().find(plan_.toStdString());
    const QString state =
        plan != nullptr ? stateWord(plan->state) : stateWord(ai::PlanState::Withdrawn);

    if (!trouble.isEmpty()) {
        outcome_->setText(tr("Öneri uygulanamadı: %1 Çizimde hiçbir şey değişmedi.").arg(trouble));
        outcome_->setProperty("tone", QStringLiteral("danger"));
    } else if (applied) {
        // SAID WHO DECIDED: a card for a plan the person's policy applied must
        // not read as though somebody clicked it (S-06).
        const bool by_policy = plan != nullptr && plan->decided_by.starts_with("politika");
        outcome_->setText(by_policy ? tr("Öneri onay politikanızla uygulandı — tek Ctrl+Z ile "
                                         "geri alınır.")
                                    : tr("Öneri %1 — tek Ctrl+Z ile geri alınır.").arg(state));
        outcome_->setProperty("tone", QStringLiteral("accent"));
    } else {
        outcome_->setText(tr("Öneri %1. Çizimde hiçbir şey değişmedi.").arg(state));
        outcome_->setProperty("tone", QVariant());
    }
    outcome_->style()->unpolish(outcome_);
    outcome_->style()->polish(outcome_);
    outcome_->setVisible(true);
    if (mark_ != nullptr) mark_->setTone(applied ? Tone::Ok : Tone::Neutral);
}

void SuggestionCard::applyTheme(ThemeMode mode)
{
    theme_ = mode;
    applyThemeToChildren(this, mode);
    update();
}

void SuggestionCard::paintEvent(QPaintEvent*)
{
    const Tokens& t = theme_ == ThemeMode::Dark ? darkTokens() : lightTokens();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    // A dashed edge, because the card is a PROPOSAL and the shell's own surfaces
    // are drawn solid: the outline says "not yet part of the drawing" before a
    // single word is read, and it says it as a shape rather than a colour
    // (ui.md R31).
    QPen edge(pending_ ? t.accentEdge : t.border, 1.0);
    edge.setStyle(pending_ ? Qt::DashLine : Qt::SolidLine);
    p.setPen(edge);
    p.setBrush(t.bgWindow);
    p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 4.0, 4.0);
}

} // namespace kentos::app
