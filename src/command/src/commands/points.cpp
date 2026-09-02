// SPDX-License-Identifier: GPL-3.0-or-later
// core.points — NOKTALAR. The surveyed point list, in and out.
//
// THE FIRST FILE A TURKISH SURVEYOR OPENS. A crew comes back with a numbered list
// from a total station, a GNSS receiver or a colleague's Netcad export, and every
// other thing in the job starts from it: the boundary is drawn between those
// points, the area is computed from them, and the setting-out list goes back to
// the field in the same shape.
//
// `Y` IS THE EASTING AND `X` IS THE NORTHING. Turkish practice writes
// `nokta no, Y, X, Z` with Y across and X up — the opposite of the mathematical
// convention — and reading them the other way round would put every point in the
// wrong place plausibly enough that nobody would notice until the ground did.
// `eksen=XY` is there for a file that genuinely came the other way, and it has to
// be SAID: no heuristic can tell a 485 320 easting from a 485 320 northing.
//
// The parsing lives in `/src/io` (io.md owns untrusted input); this is the
// command, and it reaches the work through `Bus::on_file_request` for the reason
// `İÇEAKTAR` does — Article 3.2 lets io depend on command and never the reverse.
#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include "kentos_cad/core/text.hpp"

#include <string>

namespace kentos::command {
namespace {

Task<void> run(Context& ctx)
{
    const Value path = ctx.argument("dosya");
    if (path.empty()) {
        ctx.echo("Hangi dosya? Kullanım: NOKTALAR dosya=\"olcu.txt\" [yon=oku|yaz]");
        co_return;
    }

    // Reading is the common case by a wide margin, so it is the default.
    bool writing = false;
    if (const Value direction = ctx.argument("yon"); !direction.empty()) {
        const std::string typed = direction.as_text();
        if (core::turkish_iequals(typed, "yaz") || core::turkish_iequals(typed, "write") ||
            core::turkish_iequals(typed, "disa")) {
            writing = true;
        } else if (!core::turkish_iequals(typed, "oku") && !core::turkish_iequals(typed, "read") &&
                   !core::turkish_iequals(typed, "ice")) {
            ctx.echo("Beklenen yön: oku | yaz. Girilen: '" + typed + "'");
            co_return;
        }
    }

    bool swapped = false;
    if (const Value axes = ctx.argument("eksen"); !axes.empty()) {
        const std::string typed = axes.as_text();
        if (core::turkish_iequals(typed, "XY")) {
            swapped = true;
        } else if (!core::turkish_iequals(typed, "YX")) {
            ctx.echo("Beklenen eksen sırası: YX (Türkiye'de olağan olan) veya XY. Girilen: '" +
                     typed + "'");
            co_return;
        }
    }

    Bus& bus = ctx.session().bus();
    if (!bus.on_file_request) {
        ctx.echo("Dosya motoru bağlı değil; bu ortamda nokta listesi okunup yazılamaz.");
        co_return;
    }

    FileRequest request;
    request.verb = writing ? FileRequest::Verb::ExportPoints : FileRequest::Verb::ImportPoints;
    request.path = path.as_text();
    request.swapped_axes = swapped;
    request.tx           = writing ? nullptr : &ctx.transaction();

    auto done = co_await bus.on_file_request(std::move(request));
    if (!done) {
        ctx.echo(done.error().message);
        co_return; // the bus rolls the whole import back
    }

    ctx.record("dosya", path);
    if (writing) ctx.record("yon", Value::text("yaz"));
    if (swapped) ctx.record("eksen", Value::text("XY"));
}

} // namespace

KENTOS_COMMAND(points)
{
    return CommandSpec{
        .id       = "core.points",
        .names    = {"NOKTALAR", "POINTS", "NKL"},
        .category = Category::File,
        .params =
            {
                Param::text("dosya", Arity::exactly(1), "Nokta listesi dosyasının yolu"),
                Param::text("yon", Arity::optional(), "oku (varsayılan) | yaz"),
                Param::text("eksen", Arity::optional(),
                            "Sütun sırası: YX (varsayılan, Türkiye'de olağan) | XY"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Scriptable | Flags::AiAccessible,
        .summary = "Ölçülmüş nokta listesini okur ve yazar (nokta no, Y, X, Z, kod).",
        .run     = &run,
    };
}

} // namespace kentos::command
