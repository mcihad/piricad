// SPDX-License-Identifier: GPL-3.0-or-later
// VERİTABANI — the drawing's road into and out of a PostGIS database.
//
// CLAUDE.md Article 2.9 makes PostGIS a first-class store rather than an export
// target, because that is where Turkish municipalities and TKGM actually keep
// their corporate data. A program that can only read a dump cannot sit inside
// that workflow.
//
// TWO THINGS GO IN AND THEY ARE NOT THE SAME THING; `kentos_cad/io/postgis.hpp`
// states the reasoning in full and it is worth repeating here, because this is the
// surface a user and the AI both see:
//
//   `katmanyaz` writes a LAYER as an ordinary spatial table — one row per entity,
//   a geometry column in the drawing's own SRID, one column per declared
//   attribute. QGIS, ogr2ogr and a plain SELECT all read it.
//
//   `projekaydet` stores a PROJECT WHOLE, as the bytes of its `.pcad` file,
//   because a drawing is more than its geometry: style table, embedded gösterim
//   pictures, layer tree, settings, CRS. Shredding all of that into tables would
//   be inventing a second file format that nobody round-trip-checks.
//
// ONE COMMAND WITH AN `islem` ARGUMENT, not seven commands. `AÇ` and `KAYDET` are
// separate because they are the verbs a CAD user already has in their hands; a
// database has no such folk vocabulary, and seven near-identical names in the
// reference would be worse for a person and for the AI schema than one verb with
// a named operation — which is exactly what `ZOOM mod=` already does.
//
// THE PASSWORD NEVER REACHES THE JOURNAL. A connection string is recorded with
// its `password=` field replaced, because the journal is a plain JSONL file that
// gets attached to bug reports and checked into projects. libpq already reads
// `PGPASSWORD` and `~/.pgpass`, so the supported way to authenticate is to not
// type the password here at all; see `docs/komutlar/veritabani.md`.
//
// The work itself lives in /src/io behind `Bus::on_database_request`, for the same
// reason `AÇ` does: Article 3.2 forbids /src/command from including /src/io, while
// the registry that generates the CLI help, the AI schema and the docs lives here.
#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include "kentos_cad/core/text.hpp"

#include <array>
#include <string>

namespace kentos::command {
namespace {

/// One place where "no database engine" is reported, so a headless test and a
/// user read the same sentence.
bool engine_missing(Context& ctx, Bus& bus)
{
    if (bus.on_database_request) return false;
    ctx.session().fail(core::err(core::ErrorCode::Unsupported,
                                 "Veritabanı motoru bağlı değil. Bu yapı PostgreSQL "
                                 "desteği olmadan derlenmiş olabilir."));
    return true;
}

/// Sends the request and reports.
///
/// A database operation that could not be carried out FAILS the command rather
/// than echoing a complaint: inside a script that has to abort the run, and a
/// `projekaydet` that silently did nothing is the worst outcome this product has.
Task<void> submit(Context& ctx, Bus& bus, const DatabaseRequest& request)
{
    auto result = co_await bus.on_database_request(request);
    if (!result) {
        ctx.session().fail(result.error());
        co_return;
    }
    ctx.echo(result.value());
}

/// The operations this command understands, in the order a session uses them.
///
/// The table is the ONE list: the parser reads it, the help text is built from it
/// and the docs page names the same words. A new verb is one row.
struct Operation
{
    const char* name; ///< what the user types, ASCII and lowercase
    DatabaseRequest::Verb verb;
    bool needs_target; ///< whether `hedef` must be given
    const char* target_prompt;
};

constexpr std::array<Operation, 8> kOperations{{
    {"baglan", DatabaseRequest::Verb::Connect, true,
     "Bağlantı dizesi, örnek: host=localhost dbname=kentoscad user=kentoscad"}, // ui-label
    {"kes", DatabaseRequest::Verb::Disconnect, false, nullptr},
    {"tablolar", DatabaseRequest::Verb::Tables, false, nullptr},
    {"katmanyaz", DatabaseRequest::Verb::WriteLayer, false, "Yazılacak tablonun adı"},
    {"projekaydet", DatabaseRequest::Verb::SaveProject, true, "Projenin veritabanındaki adı"},
    {"projeac", DatabaseRequest::Verb::OpenProject, true, "Açılacak projenin adı"},
    {"projeler", DatabaseRequest::Verb::Projects, false, nullptr},
    {"projesil", DatabaseRequest::Verb::DropProject, true, "Silinecek projenin adı"},
}};

/// Finds an operation by what the user typed.
///
/// Turkish-aware comparison, so `KATMANYAZ`, `katmanyaz` and `KatmanYaz` are one
/// word and the dotted-i trap in CLAUDE.md 5.6 cannot bite: `İ` upper-cases from
/// `i` here, which `std::toupper` gets wrong.
const Operation* find_operation(std::string_view typed)
{
    for (const Operation& op : kOperations)
        if (core::turkish_key_equals(typed, op.name)) return &op;
    return nullptr;
}

/// The list of operation names, for a prompt and for an error message.
std::string operation_list()
{
    std::string out;
    for (const Operation& op : kOperations) {
        if (!out.empty()) out += " | ";
        out += op.name;
    }
    return out;
}

Task<void> run(Context& ctx)
{
    auto typed = co_await ctx.text("islem", "İşlem: " + operation_list());
    if (!typed || typed->empty()) co_return;

    const Operation* op = find_operation(*typed);
    if (op == nullptr) {
        ctx.session().fail(core::err(core::ErrorCode::InvalidArgument,
                                     "Bilinmeyen işlem: '" + *typed +
                                         "'. Geçerli işlemler: " + operation_list() + "."));
        co_return;
    }

    Bus& bus = ctx.session().bus();
    if (engine_missing(ctx, bus)) co_return;

    DatabaseRequest request;
    request.verb = op->verb;

    // `hedef` is asked for interactively only when the operation cannot proceed
    // without it. `katmanyaz` has a prompt but no requirement: an empty table name
    // means "name it after the layer", which is what a user wants nine times out
    // of ten.
    if (op->needs_target) {
        auto target = co_await ctx.text("hedef", op->target_prompt);
        if (!target || target->empty()) co_return;
        request.target = *target;
    } else if (const Value given = ctx.argument("hedef"); !given.empty()) {
        request.target = given.as_text();
    }

    if (op->verb == DatabaseRequest::Verb::WriteLayer) {
        // The ACTIVE layer when none is named, so `VERİTABANI katmanyaz` alone is
        // the short form. Resolved here rather than in /src/io, so the journal line
        // names the concrete layer and a replay writes the same one (§2.2).
        const Value given = ctx.argument("katman");
        if (!given.empty()) {
            request.layer = given.as_text();
        } else if (const core::Layer* active = bus.document().layer(bus.active_layer());
                   active != nullptr) {
            request.layer = active->name;
        }

        if (request.layer.empty()) {
            ctx.session().fail(core::err(core::ErrorCode::InvalidArgument,
                                         "Yazılacak katman belirlenemedi. katman=<ad> verin."));
            co_return;
        }
        ctx.record("katman", Value::text(request.layer));
    }

    // Replacing the document is not a transactional edit — see the note on `AÇ`.
    // Every other verb writes outward or reads, so none of them takes a
    // transaction either, and `request.tx` stays null throughout.
    ctx.record("islem", Value::text(op->name));
    if (!request.target.empty())
        ctx.record("hedef", Value::text(op->verb == DatabaseRequest::Verb::Connect
                                            ? redact_conninfo(request.target)
                                            : request.target));

    co_await submit(ctx, bus, request);
}

} // namespace

KENTOS_COMMAND(database)
{
    return CommandSpec{
        .id       = "core.database",
        .names    = {"VERİTABANI", "VERITABANI", "DATABASE", "VT"},
        .category = Category::File,
        .params =
            {
                Param::text("islem", Arity::exactly(1),
                            "baglan | kes | tablolar | katmanyaz | projekaydet | projeac | "
                            "projeler | projesil"),
                Param::text("hedef", Arity::optional(),
                            "baglan: bağlantı dizesi; katmanyaz: tablo adı; proje işlemleri: "
                            "proje adı"),
                Param::text("katman", Arity::optional(),
                            "katmanyaz: yazılacak katman; yoksa etkin katman"),
            },
        // Nothing here is undoable. Writing outward changes no entity, and
        // `projeac` REPLACES the document exactly as `AÇ` does: there is nothing
        // to undo back into, so the stack is cleared instead.
        .undo = UndoPolicy::None,

        // JOURNALLED — deliberately not `ReadOnly`, which is the flag that keeps
        // `KAYDET` and `DIŞAAKTAR` out of the journal. `projeac` decides what the
        // document CONTAINS, exactly as `AÇ` does, and a journal that did not
        // record it could not reproduce the drawing it produced (Article 6.4).
        // The cost is that replaying such a journal reconnects and repeats the
        // writes; `docs/komutlar/database.md` says so where a user will read it.
        //
        // NOT `AiAccessible`, and for the reason `AÇ` is not: `projeac` throws
        // away the drawing on screen and `projesil` deletes somebody's stored
        // work, neither of them undoably. `.claude/ai.md` keeps a destructive,
        // non-undoable act out of the suggestion pipeline — a proposal must not
        // be able to cost a user work they cannot get back. The AI reaches a
        // PostGIS layer the same way it reaches everything else: by drawing and
        // measuring on a document somebody else opened for it.
        .flags   = Flags::Interactive | Flags::Scriptable,
        .summary = "PostGIS veritabanına bağlanır; katmanları tablo, projeleri kayıt olarak yazar.",
        .run     = &run,
    };
}

} // namespace kentos::command
