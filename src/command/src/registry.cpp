// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/command/registry.hpp"

#include "piricad/core/text.hpp"

#include <algorithm>
#include <numeric>

namespace piricad::command {

core::Status Registry::add(CommandSpec spec)
{
    const std::string id  = spec.id;
    core::Status admitted = admit(std::move(spec));
    if (!admitted)
        refused_.push_back((id.empty() ? std::string("(kimliksiz)") : id) + ": " +
                           admitted.error().message);
    return admitted;
}

core::Status Registry::admit(CommandSpec spec)
{
    using core::ErrorCode;

    if (spec.id.empty()) return core::err(ErrorCode::InvalidArgument, "Komut kimliği boş olamaz.");
    if (spec.names.empty())
        return core::err(ErrorCode::InvalidArgument,
                         "'" + spec.id + "' komutu hiç ad tanımlamıyor.");
    if (!spec.run)
        return core::err(ErrorCode::InvalidArgument,
                         "'" + spec.id + "' komutunun çalıştırma işlevi yok.");
    if (by_id_.contains(spec.id))
        return core::err(ErrorCode::InvalidArgument, "Yinelenen komut kimliği: '" + spec.id + "'");

    std::vector<std::string> folded;
    folded.reserve(spec.names.size());
    for (const auto& n : spec.names) {
        std::string f = core::turkish_fold_key(n);
        if (auto it = by_name_.find(f); it != by_name_.end()) {
            return core::err(ErrorCode::InvalidArgument, "'" + n + "' komut adı zaten '" +
                                                             specs_[it->second].id +
                                                             "' komutuna ait.");
        }
        folded.push_back(std::move(f));
    }

    // A KNOWN NAME SAYS WHOSE WORD IT IS, and it is not one of the command's own
    // names: those resolve already, and declaring one again as "known" would
    // tell the search a command is called what it is called.
    for (const KnownName& known : spec.known_as) {
        if (known.name.empty() || known.program.empty())
            return core::err(ErrorCode::InvalidArgument,
                             "'" + spec.id + "' komutunun bilinen adında ad ya da program boş.");
        const std::string f = core::turkish_fold_key(known.name);
        if (std::find(folded.begin(), folded.end(), f) != folded.end())
            return core::err(ErrorCode::InvalidArgument,
                             "'" + known.name + "' zaten '" + spec.id +
                                 "' komutunun adı; bilinen ad olarak yinelenmez.");
    }

    const std::size_t index = specs_.size();
    by_id_.emplace(spec.id, index);
    // The id itself always resolves, so scripts and the AI can use it directly.
    by_name_.emplace(core::turkish_fold_key(spec.id), index);
    for (auto& f : folded)
        by_name_.emplace(std::move(f), index);

    specs_.push_back(std::move(spec));
    return core::ok();
}

const CommandSpec* Registry::by_id(std::string_view id) const
{
    auto it = by_id_.find(std::string(id));
    return it == by_id_.end() ? nullptr : &specs_[it->second];
}

const CommandSpec* Registry::resolve(std::string_view typed) const
{
    if (typed.empty()) return nullptr;
    auto it = by_name_.find(core::turkish_fold_key(typed));
    return it == by_name_.end() ? nullptr : &specs_[it->second];
}

std::vector<const CommandSpec*> Registry::known_as(std::string_view word) const
{
    std::vector<const CommandSpec*> out;
    const std::string folded = core::turkish_fold_key(word);
    if (folded.empty()) return out;
    for (const CommandSpec& spec : specs_)
        for (const KnownName& known : spec.known_as)
            if (core::turkish_fold_key(known.name) == folded) {
                out.push_back(&spec);
                break;
            }
    return out;
}

std::vector<std::string> Registry::complete(std::string_view prefix, std::size_t limit) const
{
    const std::string folded = core::turkish_fold_key(prefix);
    std::vector<std::string> out;

    for (const auto& spec : specs_) {
        for (const auto& n : spec.names) {
            if (core::turkish_fold_key(n).starts_with(folded)) {
                out.push_back(n);
                break; // one suggestion per command; aliases would drown the list
            }
        }
    }

    std::sort(out.begin(), out.end());
    if (out.size() > limit) out.resize(limit);
    return out;
}

std::uint64_t Registry::fingerprint() const
{
    // The seed names what is being hashed, the way every other content hash in
    // this program does (io/format.hpp). Sorted by id first, so the registration
    // ORDER cannot change the answer: two builds that register the same commands
    // must agree, and the roster's order is not part of the surface.
    std::vector<std::size_t> sorted(specs_.size());
    std::iota(sorted.begin(), sorted.end(), std::size_t{0});
    std::sort(sorted.begin(), sorted.end(),
              [this](std::size_t a, std::size_t b) { return specs_[a].id < specs_[b].id; });

    std::uint64_t h = core::fnv1a("piricad.ai.catalog");
    for (const std::size_t at : sorted) {
        const CommandSpec* spec = &specs_[at];
        h                       = core::fnv1a(spec->id, h);
        for (const std::string& name : spec->names)
            h = core::fnv1a(name, h);
        // A known name is part of what an agent is told (`ai::tool_for`), so a
        // new one has to read as a changed surface.
        for (const KnownName& known : spec->known_as) {
            h = core::fnv1a(known.name, h);
            h = core::fnv1a(known.program, h);
        }
        h = core::fnv1a_int(static_cast<std::int64_t>(spec->category), h);
        h = core::fnv1a_int(static_cast<std::int64_t>(spec->flags), h);
        h = core::fnv1a_int(static_cast<std::int64_t>(spec->undo), h);
        h = core::fnv1a(spec->summary, h);
        h = core::fnv1a_int(static_cast<std::int64_t>(spec->targets), h);
        for (const VerbTargets& row : spec->verb_targets) {
            h = core::fnv1a(row.param, h);
            h = core::fnv1a(row.word, h);
            h = core::fnv1a_int(static_cast<std::int64_t>(row.targets), h);
        }
        for (const Param& p : spec->params) {
            h = core::fnv1a(p.name, h);
            h = core::fnv1a_int(static_cast<std::int64_t>(p.kind), h);
            h = core::fnv1a_int(p.arity.min, h);
            h = core::fnv1a_int(p.arity.max, h);
            h = core::fnv1a(p.help, h);
            for (const std::string& word : p.choices)
                h = core::fnv1a(word, h);
            if (p.bounded) {
                h = core::fnv1a_int(p.low, h);
                h = core::fnv1a_int(p.high, h);
            }
        }
    }
    return h;
}

SearchMatch search_match(const CommandSpec& spec, std::string_view word)
{
    SearchMatch out;
    const std::string needle = core::turkish_fold_key(word);
    if (needle.empty()) return out;

    const auto better = [&out](int tier, const KnownName* known) {
        if (out.tier == SearchMatch::kNone || tier < out.tier) {
            out.tier  = tier;
            out.known = known;
        }
    };
    const auto contains = [&needle](const std::string& folded) {
        return folded.find(needle) != std::string::npos;
    };

    for (const std::string& name : spec.names) {
        const std::string f = core::turkish_fold_key(name);
        if (f == needle)
            better(0, nullptr);
        else if (f.starts_with(needle))
            better(2, nullptr);
        else if (contains(f))
            better(4, nullptr);
    }
    for (const KnownName& known : spec.known_as) {
        const std::string f = core::turkish_fold_key(known.name);
        if (f == needle)
            better(1, &known);
        else if (f.starts_with(needle))
            better(3, &known);
        else if (contains(f))
            better(5, &known);
    }
    if (contains(core::turkish_fold_key(spec.id))) better(4, nullptr);
    if (contains(core::turkish_fold_key(spec.title)) ||
        contains(core::turkish_fold_key(spec.summary)))
        better(6, nullptr);
    return out;
}

Registry& registry()
{
    static Registry r;
    return r;
}

} // namespace piricad::command
