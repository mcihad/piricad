// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/command/registry.hpp"

#include "piricad/core/text.hpp"

#include <algorithm>
#include <array>
#include <numeric>
#include <string_view>
#include <vector>

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

void note_use(std::vector<std::string>& recent, std::string_view id, std::size_t limit)
{
    if (id.empty()) return;
    std::erase(recent, std::string(id));
    recent.insert(recent.begin(), std::string(id));
    if (recent.size() > limit) recent.resize(limit);
}

bool toggle_member(std::vector<std::string>& set, std::string_view id)
{
    if (id.empty()) return false;
    const auto at = std::ranges::find(set, id);
    if (at != set.end()) {
        set.erase(at);
        return false;
    }
    set.emplace_back(id);
    return true;
}

bool move_member(std::vector<std::string>& set, std::string_view id, int by)
{
    const auto at = std::ranges::find(set, id);
    if (at == set.end() || by == 0) return false;

    const auto from = at - set.begin();
    const auto last = static_cast<std::ptrdiff_t>(set.size()) - 1;
    const auto to   = std::clamp<std::ptrdiff_t>(from + by, 0, last);
    if (to == from) return false;

    // A rotation keeps the others in their order, which is what moving one row means.
    if (to < from)
        std::rotate(set.begin() + to, at, at + 1);
    else
        std::rotate(at, at + 1, set.begin() + to + 1);
    return true;
}

bool worth_remembering(const CommandSpec& spec)
{
    return !has_flag(spec.flags, Flags::Transparent) && spec.id != "core.undo" &&
           spec.id != "core.redo";
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

namespace {

/// The words of folded text: runs of A–Z and 0–9. Whatever else is in it — a space, a
/// dash, a bracket, a symbol the fold leaves alone — ends a word.
std::vector<std::string> words_of(const std::string& folded)
{
    std::vector<std::string> words;
    std::string current;
    for (const char ch : folded) {
        const bool inside = (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9');
        if (inside) {
            current.push_back(ch);
        } else if (!current.empty()) {
            words.push_back(std::move(current));
            current.clear();
        }
    }
    if (!current.empty()) words.push_back(std::move(current));
    return words;
}

/// What a request carries and no command is named for: the particles, the verbs of
/// asking and doing, the English filler. Folded, so `için` is ICIN. A word the registry
/// really uses is never dropped: this list is only consulted for the words of a text that
/// matched nothing as a whole.
bool is_filler(const std::string& word)
{
    static constexpr auto kFiller = std::to_array<std::string_view>(
        {"VE", "ILE", "BIR", "BU", "SU", "ICIN", "YA", "DA", "DE", "MI", "MU", "GIBI", "NE",
         "NASIL", "YAP", "YAPMAK", "ET", "ETMEK", "BANA", "HANGI", "VAR", "ISTIYORUM", "LUTFEN",
         "KOMUT", "ARAC",
         // How many, which: a request counts and points at things, and no command is
         // named for "two" or "all of them".
         "IKI", "UC", "TUM", "HER", "BUTUN", "SECILI", "SECILEN", "ILGILI", "THE", "AN", "TO",
         "AND", "FOR", "HOW", "DO", "WANT", "WITH", "MAKE", "OF"});
    return word.size() < 2 || std::ranges::find(kFiller, word) != kFiller.end();
}

/// A consonant that softens between a stem and a vowel ending: KİTAP becomes KİTABI,
/// UZUNLUK becomes UZUNLUĞU. Folded, so Ğ is G and Ç stays C.
bool softens(char stem_last, char before_vowel)
{
    return (stem_last == 'K' && before_vowel == 'G') || (stem_last == 'P' && before_vowel == 'B') ||
           (stem_last == 'T' && before_vowel == 'D');
}

/// How well a typed `word` is the same word as one a command declares: 3 the same; 2 the
/// start of it, or it with endings on (`ALANI` is ALAN); 1 only inside a longer one; 0 not
/// at all. A stem has to be three letters at least, and four letters to take a long run
/// of endings: SİL is not the stem of SİLİNDİR, and OF is an abbreviation, not a stem.
int word_match(const std::string& typed, const std::string& declared)
{
    if (typed == declared) return 3;
    const std::size_t tl = typed.size();
    const std::size_t dl = declared.size();

    if (tl >= 3 && dl > tl && declared.compare(0, tl, typed) == 0) return 2;

    const std::size_t room = dl >= 4 ? 6 : 4;
    if (dl >= 3 && tl > dl && tl - dl <= room && typed.compare(0, dl, declared) == 0) return 2;

    if (dl >= 4 && tl >= dl && tl - dl <= room + 1 &&
        typed.compare(0, dl - 1, declared, 0, dl - 1) == 0 &&
        softens(declared.back(), typed[dl - 1]))
        return 2;

    if (tl >= 4 && declared.find(typed) != std::string::npos) return 1;

    // The same stem under different endings (`NESNEYİ`, NESNELERİ), or a stem that begins a
    // compound name (`alanı` is ALAN-ÖLÇ, `yazıyı` is YAZI-DÜZENLE): a run of four letters or
    // more at the front that is most of the shorter word.
    std::size_t same = 0;
    while (same < tl && same < dl && typed[same] == declared[same])
        ++same;
    if (same >= 4 && same * 100 >= std::min(tl, dl) * 60) return 1;
    return 0;
}

} // namespace

SearchMatch search_query(const CommandSpec& spec, std::string_view query)
{
    if (SearchMatch whole = search_match(spec, query); whole.tier != SearchMatch::kNone)
        return whole;

    std::vector<std::string> typed;
    for (std::string& word : words_of(core::turkish_fold_key(query)))
        if (!is_filler(word)) typed.push_back(std::move(word));
    if (typed.empty()) return {};

    // Where a command says what it is, nearest first: its names and the names other
    // programs know it by (0), its title and summary (1), its parameters' names (2).
    struct Field
    {
        std::vector<std::string> words;
        int tier;
        const KnownName* known;
    };

    std::vector<Field> fields;
    for (const std::string& name : spec.names)
        fields.push_back({words_of(core::turkish_fold_key(name)), 0, nullptr});
    for (const KnownName& known : spec.known_as)
        fields.push_back({words_of(core::turkish_fold_key(known.name)), 0, &known});
    fields.push_back(
        {words_of(core::turkish_fold_key(spec.title + " " + spec.summary)), 1, nullptr});
    for (const Param& p : spec.params)
        fields.push_back({words_of(core::turkish_fold_key(p.name)), 2, nullptr});

    // WHAT A WORD COSTS. Where it was found counts three a step (a name, then the title
    // and summary, then a parameter's name); HOW it was found counts under that: spelled
    // exactly 0, with endings or only begun 1, inside a longer word 2. The second is
    // Turkish doing the sorting: a command is asked for in the bare imperative (`birleştir`,
    // `yuvarla`, `kaydır`) and the thing it works on takes an ending (`çizgiyi`, `köşeyi`,
    // `alanı`) — so of "iki çizgiyi birleştir", BİRLEŞTİR is named exactly and ÇİZGİ is only
    // the object, and the sentence is answered by the first. A word nothing answers costs
    // more than any word that was found.
    constexpr int kUnanswered = 12;
    int cost                  = 0;
    std::size_t found         = 0;
    const KnownName* source   = nullptr;
    for (const std::string& word : typed) {
        int best                   = -1;
        const KnownName* best_from = nullptr;
        for (const Field& field : fields)
            for (const std::string& declared : field.words) {
                const int strength = word_match(word, declared);
                if (strength == 0) continue;
                const int price = field.tier * 3 + (3 - strength);
                if (best < 0 || price < best) {
                    best      = price;
                    best_from = field.known;
                }
            }
        if (best < 0) {
            cost += kUnanswered;
            continue;
        }
        ++found;
        cost += best;
        if (best_from != nullptr && source == nullptr) source = best_from;
    }

    // Half of what was asked, rounded up: "çizgiyi paralel çiz" is answered by the command
    // that is PARALEL even though nothing is named ÇİZ; one word of five is not an answer.
    if (found < (typed.size() + 1) / 2) return {};

    SearchMatch out;
    out.tier  = 7 + cost;
    out.known = source;
    return out;
}

Registry& registry()
{
    static Registry r;
    return r;
}

} // namespace piricad::command
