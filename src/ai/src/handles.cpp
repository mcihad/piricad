// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/ai/handles.hpp"

#include "kentos_cad/core/text.hpp"

#include <cstdio>

namespace kentos::ai {
namespace {

/// The handle id, as text: `@` and sixteen hex digits.
std::string format_id(std::uint64_t value)
{
    char buffer[20] = {};
    (void)std::snprintf(buffer, sizeof buffer, "@%016llx", static_cast<unsigned long long>(value));
    return std::string(buffer);
}

const char* kind_word(HandleKind kind)
{
    switch (kind) {
    case HandleKind::Points: return "nokta";
    case HandleKind::Entities: return "nesne";
    case HandleKind::Window: return "pencere";
    }
    return "değer";
}

} // namespace

std::optional<HandleRef> HandleRef::parse(std::string_view text)
{
    // 17 characters is `@` plus sixteen hex digits, which is the shortest a
    // handle can be; the schema's own `pattern` says the same thing, so a string
    // that fails here came from a client that ignored the schema.
    if (text.size() < 17 || text.front() != '@') return std::nullopt;

    HandleRef ref;
    const std::size_t dot = text.find('.', 1);
    ref.id = std::string(text.substr(0, dot == std::string_view::npos ? text.size() : dot));
    if (ref.id.size() != 17) return std::nullopt;
    for (std::size_t i = 1; i < ref.id.size(); ++i) {
        const char c   = ref.id[i];
        const bool hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
        if (!hex) return std::nullopt;
    }

    if (dot != std::string_view::npos) {
        const std::string_view digits = text.substr(dot + 1);
        if (digits.empty()) return std::nullopt;
        std::size_t index = 0;
        for (const char c : digits) {
            if (c < '0' || c > '9') return std::nullopt;
            index = index * 10 + static_cast<std::size_t>(c - '0');
        }
        ref.index = index;
    }
    return ref;
}

std::string HandleStore::next_id(HandleKind kind)
{
    // DETERMINISTIC, NOT RANDOM. A test that ran twice and a journal that was
    // replayed twice must produce the same handle, and `Math.random`-shaped ids
    // would make both unreproducible. The counter is per store, so two client
    // sessions mint the same ids over different values — which is why `resolve`
    // only ever looks in its own store and a handle from one session means
    // nothing in another.
    ++minted_;
    std::uint64_t h = core::fnv1a("kentos.ai.handle");
    h               = core::fnv1a_int(static_cast<std::int64_t>(minted_), h);
    h               = core::fnv1a_int(static_cast<std::int64_t>(kind), h);
    return format_id(h);
}

const HandleValue& HandleStore::mint_points(std::vector<core::Point2> points, std::string tool,
                                            std::uint64_t revision, Provenance provenance,
                                            std::string label, std::vector<std::string> labels)
{
    HandleValue value;
    value.id         = next_id(HandleKind::Points);
    value.kind       = HandleKind::Points;
    value.provenance = provenance;
    value.tool       = std::move(tool);
    value.revision   = revision;
    value.points     = std::move(points);
    value.label      = std::move(label);
    value.labels     = std::move(labels);

    // THE OLDEST GOES WHEN THE STORE IS FULL. A client that queries in a loop
    // and never draws would otherwise hold every result it ever asked for, and a
    // handle nobody will use again is memory this program owes to the drawing.
    if (values_.size() >= kCapacity) values_.erase(values_.begin());
    values_.push_back(std::move(value));
    return values_.back();
}

const HandleValue& HandleStore::mint_entities(std::vector<std::int64_t> keys, std::string tool,
                                              std::uint64_t revision)
{
    HandleValue value;
    value.id         = next_id(HandleKind::Entities);
    value.kind       = HandleKind::Entities;
    value.provenance = Provenance::Document;
    value.tool       = std::move(tool);
    value.revision   = revision;
    value.entities   = std::move(keys);

    if (values_.size() >= kCapacity) values_.erase(values_.begin());
    values_.push_back(std::move(value));
    return values_.back();
}

const HandleValue& HandleStore::mint_window(core::Box2 window, std::string tool,
                                            std::uint64_t revision)
{
    HandleValue value;
    value.id         = next_id(HandleKind::Window);
    value.kind       = HandleKind::Window;
    value.provenance = Provenance::Document;
    value.tool       = std::move(tool);
    value.revision   = revision;
    value.window     = window;

    if (values_.size() >= kCapacity) values_.erase(values_.begin());
    values_.push_back(std::move(value));
    return values_.back();
}

const HandleValue* HandleStore::find(std::string_view id) const
{
    for (const HandleValue& value : values_)
        if (value.id == id) return &value;
    return nullptr;
}

core::Result<HandleValue> HandleStore::resolve(const HandleRef& ref, std::uint64_t revision) const
{
    const HandleValue* value = find(ref.id);
    if (value == nullptr)
        return core::err(core::ErrorCode::NotFound,
                         "Böyle bir tutamak yok: '" + ref.id +
                             "'. Konum bir okuma aracının sonucundan gelir; önce sorgula, "
                             "secimi_al ya da gorunum_bilgisi çağırın.");

    // A HANDLE IS ONLY AS TRUE AS THE DRAWING IT WAS READ FROM. Between the read
    // and the write somebody may have moved, erased or replaced what it names —
    // and then drawing at those coordinates puts a line where nothing is any
    // more. Refusing is the only honest answer; the client re-reads and tries
    // again, which is cheap, while a wrong parcel boundary is not.
    if (value->revision != revision)
        return core::err(core::ErrorCode::ValidationFailed,
                         "Tutamak '" + ref.id +
                             "' çizimin eski bir hâlinden: o zamandan beri "
                             "çizim değişti. Okuma aracını yeniden çağırın.");

    if (!ref.index) return *value;

    const std::size_t at = *ref.index;
    HandleValue one      = *value;
    switch (value->kind) {
    case HandleKind::Points:
        if (at >= value->points.size())
            return core::err(core::ErrorCode::InvalidArgument,
                             "Tutamak '" + ref.id + "' " + std::to_string(value->points.size()) +
                                 " nokta taşıyor; " + std::to_string(at) + ". istendi.");
        one.points = {value->points[at]};
        return one;

    case HandleKind::Entities:
        if (at >= value->entities.size())
            return core::err(core::ErrorCode::InvalidArgument,
                             "Tutamak '" + ref.id + "' " + std::to_string(value->entities.size()) +
                                 " nesne taşıyor; " + std::to_string(at) + ". istendi.");
        one.entities = {value->entities[at]};
        return one;

    case HandleKind::Window:
        return core::err(core::ErrorCode::InvalidArgument,
                         "Pencere tutamağının parçası istenemez: '" + ref.id + "." +
                             std::to_string(at) + "'.");
    }
    return *value;
}

core::Json HandleStore::describe(const HandleValue& value)
{
    // WHAT A CLIENT IS TOLD, and what it is NOT told. The id, the kind, how many
    // values it holds and where they came from — never the coordinate list
    // itself. `.claude/ai.md` P9 forbids dumping the drawing into a prompt, and a
    // model that could read the numbers would be one step from writing them back
    // as literals, which is the thing the handle exists to prevent.
    core::Json out;
    out.set("tutamak", core::Json::string(value.id));
    out.set("tur", core::Json::string(kind_word(value.kind)));
    out.set("arac", core::Json::string(value.tool));
    out.set("surum", core::Json::integer(static_cast<std::int64_t>(value.revision)));

    // WHAT IT IS, IN WORDS, so a model can pick the right `.N` without being
    // handed the numbers: "görünümün ortası", "nesne 3: köşe 2".
    if (!value.label.empty()) out.set("ad", core::Json::string(value.label));
    switch (value.kind) {
    case HandleKind::Points:
        out.set("adet", core::Json::integer(static_cast<std::int64_t>(value.points.size())));
        if (!value.labels.empty()) {
            core::Json names = core::Json::array({});
            for (const std::string& one : value.labels)
                names.push(core::Json::string(one));
            out.set("noktalar", std::move(names));
        }
        break;
    case HandleKind::Entities:
        out.set("adet", core::Json::integer(static_cast<std::int64_t>(value.entities.size())));
        break;
    case HandleKind::Window: {
        // The window is the one exception, and deliberately so: the maintainer
        // asked for "the corner coordinates of the visible area" by name, and a
        // window a client cannot read is a window it cannot reason about. These
        // are the screen's own corners, not the drawing's contents.
        core::Json box = core::Json::array({});
        box.push(core::Json::integer(value.window.min_x));
        box.push(core::Json::integer(value.window.min_y));
        box.push(core::Json::integer(value.window.max_x));
        box.push(core::Json::integer(value.window.max_y));
        out.set("kutu_mm", std::move(box));
        break;
    }
    }

    switch (value.provenance) {
    case Provenance::Document: out.set("kaynak", core::Json::string("cizim")); break;
    case Provenance::Computed: out.set("kaynak", core::Json::string("hesap")); break;
    case Provenance::Attested:
        out.set("kaynak", core::Json::string("kullanici_onayli_olcum"));
        break;
    }
    return out;
}

HandleStore& HandleScopes::for_client(const std::string& requester)
{
    for (auto& [label, store] : stores_)
        if (label == requester) return store;

    if (stores_.size() >= kMaxClients) stores_.erase(stores_.begin());

    stores_.emplace_back(requester, HandleStore{});
    return stores_.back().second;
}

const HandleStore* HandleScopes::peek(std::string_view requester) const
{
    for (const auto& [label, store] : stores_)
        if (label == requester) return &store;
    return nullptr;
}

} // namespace kentos::ai
