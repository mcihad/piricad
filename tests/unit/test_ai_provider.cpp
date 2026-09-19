// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — tests: YAPAYZEKAMODELİ (ai/commands/provider_command.cpp).
//
// What is proven here is the COMMAND, not the file and not the socket: the
// declared words a client is told it may send, the `ekle` semantics, the
// refusals, the store's one-default invariant as the command drives it, what the
// journal records — and, the point of the whole file, what it CANNOT record.
//
// AN API KEY IS NOT EXPRESSIBLE AS AN ARGUMENT OF THIS COMMAND, and that is the
// property under test: CLAUDE.md 5.21 and `.claude/ai.md` P11 keep a credential
// out of every `Value`, journal line and log, and the cheapest way to keep a rule
// like that is a command that has nowhere to put one. `anahtar_ref` carries the
// NAME of a key-store entry, and a value that looks like a key is refused before
// it is ever recorded.
//
// The other halves are proved elsewhere: `tests/unit/test_ai_chat.cpp` over the
// profile store itself (JSON round trip, `upsert`'s floor, the permit), and the
// application's own file and key store in `app::ProviderService` — this suite
// links no Qt, so the engine here is a double, exactly as `test_print.cpp` does
// with the print engine.
#include "kentos_test.hpp"

#include "kentos_cad/ai/commands.hpp"
#include "kentos_cad/ai/provider.hpp"
#include "kentos_cad/ai/provider_catalog.hpp"
#include "kentos_cad/ai/redact.hpp"

#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/journal.hpp"
#include "kentos_cad/command/registry.hpp"
#include "kentos_cad/core/document.hpp"

#include "kentos_cad/script/json_runner.hpp"
#include <fstream>
#include <iterator>

#include <string>
#include <vector>

using namespace kentos;
using namespace kentos::command;

namespace {

/// A bus with a provider store that records instead of writing a file.
///
/// The seam is what the application installs (`app::ProviderService`); a double
/// here proves the command layer with no Qt, no configuration directory and no
/// network.
/// Two endpoints, one local and default, one cloud — enough for `listele`,
/// `varsayilan`, `sil` and `dene` to have something to act on.
ai::ProviderProfiles two_profiles()
{
    ai::ProviderProfiles out;

    ai::ProviderProfile local;
    local.name        = "Ollama";
    local.dialect     = ai::Dialect::OllamaNative;
    local.base_url    = "http://localhost:11434";
    local.path        = "/api/chat";
    local.model       = "llama3.1";
    local.auth_header = "";
    local.auth_scheme = "";
    local.context     = {8192, ai::ContextSource::Builtin};
    (void)out.upsert(local);

    ai::ProviderProfile cloud;
    cloud.name     = "DeepSeek";
    cloud.dialect  = ai::Dialect::OpenAiChat;
    cloud.base_url = "https://api.deepseek.com";
    cloud.path     = "/chat/completions";
    cloud.model    = "deepseek-chat";
    cloud.key_ref  = "deepseek";
    (void)out.upsert(cloud);

    (void)out.set_default("Ollama");
    return out;
}

struct Rig
{
    core::Document doc;
    Registry reg;
    Journal journal;
    UndoStack undo;
    Bus bus{doc, reg, journal, undo};
    std::string said;

    /// Every request the command sent, in order.
    std::vector<AiProviderRequest> asked;

    /// The store the double keeps, so `listele`, `sil` and `varsayilan` answer
    /// the way the real service answers.
    // A STORE BUILT IN THE TEST, not a shipped one. The starting profiles come
    // from the data catalogue now (`ai::seed_profiles`), and a unit test that
    // read `/data` would be testing the package rather than the command.
    ai::ProviderProfiles profiles{two_profiles()};

    explicit Rig(bool with_engine = true)
    {
        register_builtin_commands(reg);
        ai::register_ai_commands(reg);
        bus.on_echo = [this](std::string_view s) { said.append(s).append("\n"); };
        if (!with_engine) return;

        bus.on_ai_provider_request =
            [this](const AiProviderRequest& request) -> Task<core::Result<std::string>> {
            return this->handle(request);
        };
    }

    Task<core::Result<std::string>> handle(AiProviderRequest request)
    {
        asked.push_back(request);
        switch (request.verb) {
        case AiProviderRequest::Verb::Profiles: co_return profiles.listing();

        case AiProviderRequest::Verb::SetProfile: {
            ai::ProviderProfile p;
            p.name = request.name;
            if (!request.dialect.empty())
                p.dialect = ai::dialect_from_id(request.dialect).value_or(p.dialect);
            if (!request.base_url.empty()) p.base_url = request.base_url;
            // THE DIALECT'S OWN ENDPOINT PATH IS THE APPLICATION'S JOB
            // (`app::ProviderService::apply_dialect_defaults`), and the
            // application is not linked here. The double fills in a valid one so
            // that `upsert`'s floor is cleared; what this suite checks is the
            // REQUEST the command produced, asserted field by field below.
            p.path  = request.path.empty() ? "/chat/completions" : request.path;
            p.model = request.model.empty() ? "deneme-model" : request.model;
            if (!request.key_ref.empty()) p.key_ref = request.key_ref;
            if (request.context > 0)
                p.context = ai::ContextWindow{request.context, ai::ContextSource::User};
            if (request.max_tokens >= 0) p.max_tokens = request.max_tokens;
            if (request.temperature >= 0.0) p.temperature = request.temperature;
            if (request.stream >= 0) p.stream = request.stream == 1;
            if (request.tools >= 0) p.tools = request.tools == 1;
            if (request.reasoning_text >= 0) p.reasoning.show_text = request.reasoning_text == 1;
            if (auto st = profiles.upsert(std::move(p)); !st) co_return st.error();
            co_return "Model profili kaydedildi: " +
                ai::describe_provider(*profiles.find(request.name));
        }

        case AiProviderRequest::Verb::RemoveProfile:
            if (auto st = profiles.remove(request.name); !st) co_return st.error();
            co_return "Model profili silindi: " + request.name;

        case AiProviderRequest::Verb::SetDefault:
            if (auto st = profiles.set_default(request.name); !st) co_return st.error();
            co_return "Varsayılan model sağlayıcısı: " + profiles.default_name();

        case AiProviderRequest::Verb::Test: {
            const ai::ProviderProfile* p = profiles.find(request.name);
            if (p == nullptr)
                co_return core::err(core::ErrorCode::NotFound,
                                    "Böyle bir model sağlayıcısı yok: '" + request.name + "'.");
            co_return "Model denemesi başladı: " + p->name;
        }
        }
        co_return core::err(core::ErrorCode::Internal, "bilinmeyen istek");
    }

    core::Result<DispatchResult> run(const char* line)
    {
        return bus.execute_line(line, Origin::Test);
    }

    void must(const char* line)
    {
        auto r = run(line);
        if (!r) FAIL_WITH(line, r.error().message);
    }
};

/// The comparable part of a journal: what was commanded, with which arguments.
std::string journal_of(const Journal& j)
{
    std::string out;
    for (const auto& e : j.entries()) {
        out += e.command_id;
        out += ' ';
        out += e.args.to_json().dump();
        out += '\n';
    }
    return out;
}

} // namespace

TEST_CASE("YAPAYZEKAMODELİ: kayıtta; adlar, kategori, bayraklar ve bildirilen sözcükler")
{
    Rig f;

    const CommandSpec* spec = f.reg.resolve("YAPAYZEKAMODELİ");
    REQUIRE(spec != nullptr);
    CHECK_EQ(spec->id, std::string("core.ai_provider"));
    CHECK_EQ(spec->category, Category::System);
    CHECK(f.reg.resolve("YAPAYZEKAMODELI") == spec); // ASCII-folded Turkish
    CHECK(f.reg.resolve("AIMODEL") == spec);         // English
    CHECK(f.reg.resolve("YZM") == spec);             // abbreviation

    // Writes no entity, so nothing to undo — and NOT `ReadOnly`, because that
    // flag suppresses the journal entry (`Bus::finish`) and which model an office
    // pointed at is exactly what a journal is for.
    CHECK_EQ(spec->undo, UndoPolicy::None);
    CHECK(has_flag(spec->flags, Flags::Interactive));
    CHECK(has_flag(spec->flags, Flags::Scriptable));
    CHECK_FALSE(has_flag(spec->flags, Flags::ReadOnly));

    // A MODEL MAY NOT RECONFIGURE WHICH MODEL RUNS (CLAUDE.md 2.8, ai.md R13).
    CHECK_FALSE(has_flag(spec->flags, Flags::AiAccessible));
    CHECK_FALSE(has_flag(spec->flags, Flags::NoEffect));

    // The declared operations, as a client is told them.
    const Param* islem = nullptr;
    const Param* lehce = nullptr;
    for (const Param& p : spec->params) {
        if (p.name == "islem") islem = &p;
        if (p.name == "lehce") lehce = &p;
        // THE KEY ITSELF IS NOT A PARAMETER OF THIS COMMAND, and this is the
        // assertion that keeps it that way: a future edit adding `anahtar` would
        // put a credential in `Args`, in the journal and in the transcript
        // (CLAUDE.md 5.21, ai.md P11).
        CHECK(p.name != "anahtar");
        CHECK(p.name != "api_anahtari");
        CHECK(p.name != "key");
    }
    REQUIRE(islem != nullptr);
    CHECK_EQ(islem->kind, ParamKind::Text);
    CHECK_EQ(islem->choices.size(), std::size_t{5});
    CHECK(islem->choices.front() == "listele");
    CHECK(islem->choices.back() == "dene");

    // The dialect words are the FOUR `ai::dialect_id` spellings, taken from
    // `ai::dialects()` rather than written out twice.
    REQUIRE(lehce != nullptr);
    REQUIRE_EQ(lehce->choices.size(), ai::dialects().size());
    for (std::size_t i = 0; i < lehce->choices.size(); ++i)
        CHECK_EQ(lehce->choices[i], std::string(ai::dialect_id(ai::dialects()[i])));

    // And `anahtar_ref` exists, is text, and says in its own help what it holds.
    bool key_ref_declared = false;
    for (const Param& p : spec->params)
        if (p.name == "anahtar_ref") {
            key_ref_declared = true;
            CHECK_EQ(p.kind, ParamKind::Text);
            CHECK(p.help.find("anahtar") != std::string::npos);
        }
    CHECK(key_ref_declared);
}

TEST_CASE("YAPAYZEKAMODELİ: listeler, ekler, varsayılan yapar, siler, dener")
{
    Rig f;

    f.must("YAPAYZEKAMODELİ islem=listele");
    CHECK(f.said.find("Ollama") != std::string::npos);
    CHECK(f.said.find("* ") != std::string::npos); // the default is marked
    REQUIRE_EQ(f.asked.size(), std::size_t{1});
    CHECK(f.asked.front().verb == AiProviderRequest::Verb::Profiles);

    // `ekle` — the defaults plus what was given, and every field reaches the
    // engine as it was typed.
    f.must("YAPAYZEKAMODELİ islem=ekle ad=\"Kurum vLLM\" lehce=openai_chat "
           "adres=http://vllm.kurum.lan:8000/v1 yol=/chat/completions model=Qwen2.5-7B "
           "anahtar_ref=kurum_vllm baglam=32768 azami=2048 sicaklik=0.2 akis=evet "
           "dusunme=hayır araclar=evet");
    REQUIRE_EQ(f.asked.size(), std::size_t{2});
    const AiProviderRequest& added = f.asked.back();
    CHECK(added.verb == AiProviderRequest::Verb::SetProfile);
    CHECK_EQ(added.name, std::string("Kurum vLLM"));
    CHECK_EQ(added.dialect, std::string("openai_chat"));
    CHECK_EQ(added.base_url, std::string("http://vllm.kurum.lan:8000/v1"));
    CHECK_EQ(added.path, std::string("/chat/completions"));
    CHECK_EQ(added.model, std::string("Qwen2.5-7B"));
    CHECK_EQ(added.key_ref, std::string("kurum_vllm"));
    CHECK_EQ(added.context, 32768);
    CHECK_EQ(added.max_tokens, 2048);
    CHECK_EQ(added.temperature, 0.2);
    CHECK_EQ(added.stream, 1);
    CHECK_EQ(added.reasoning_text, 0);
    CHECK_EQ(added.tools, 1);

    REQUIRE(f.profiles.find("Kurum vLLM") != nullptr);
    const ai::ProviderProfile& stored = *f.profiles.find("Kurum vLLM");
    CHECK_EQ(stored.model, std::string("Qwen2.5-7B"));
    CHECK_EQ(stored.context.tokens, 32768);
    CHECK(stored.context.source == ai::ContextSource::User);
    CHECK_EQ(stored.key_ref, std::string("kurum_vllm"));
    // The institution's own vLLM is in the private network, so it is usable even
    // when the project is marked sensitive (ai.md R14 — the reach is read from
    // the URL's host, never from what a profile calls itself).
    CHECK(ai::reach_of(stored.base_url) == ai::Reach::PrivateNetwork);
    CHECK(ai::permit_for(stored, /*sensitive=*/true).ok());

    // NOT GIVEN IS NOT THE SAME AS ZERO: a field left off the line arrives as
    // "not given" so the built-in default stands, and `azami=0` means "do not
    // send the field at all" (`ai::ProviderProfile::max_tokens`).
    f.must("YAPAYZEKAMODELİ islem=ekle ad=Yalın adres=http://localhost:11434/v1 model=llama3.1");
    const AiProviderRequest& plain = f.asked.back();
    CHECK(plain.dialect.empty());
    CHECK_EQ(plain.max_tokens, -1);
    CHECK_EQ(plain.stream, -1);
    CHECK(plain.temperature < 0.0);
    f.must("YAPAYZEKAMODELİ islem=ekle ad=Yalın adres=http://localhost:11434/v1 model=llama3.1 "
           "azami=0");
    CHECK_EQ(f.asked.back().max_tokens, 0);

    // `varsayilan`, `sil` and `dene` name a profile and carry no fields.
    f.must("YAPAYZEKAMODELİ islem=varsayilan ad=\"Kurum vLLM\"");
    CHECK_EQ(f.profiles.default_name(), std::string("Kurum vLLM"));
    f.must("YAPAYZEKAMODELİ islem=dene ad=\"kurum vllm\""); // Turkish folding finds it
    CHECK(f.asked.back().verb == AiProviderRequest::Verb::Test);
    CHECK(f.said.find("Model denemesi başladı") != std::string::npos);

    // EXACTLY ONE IS THE DEFAULT, AND REMOVING IT MOVES IT rather than leaving
    // the store pointing at nothing.
    f.must("YAPAYZEKAMODELİ islem=sil ad=\"Kurum vLLM\"");
    CHECK(f.profiles.find("Kurum vLLM") == nullptr);
    CHECK_FALSE(f.profiles.default_name().empty());
    CHECK(f.profiles.find(f.profiles.default_name()) != nullptr);
    std::size_t marked = 0;
    for (const ai::ProviderProfile& p : f.profiles.all())
        if (p.name == f.profiles.default_name()) ++marked;
    CHECK_EQ(marked, std::size_t{1});
}

TEST_CASE("YAPAYZEKAMODELİ: anahtarın kendisi hiçbir yoldan girmez")
{
    Rig f;

    // THE DEFECT THIS GUARDS: a user pastes the key into the box that takes the
    // key's NAME. The command refuses before the value is recorded, so it never
    // reaches the journal — and the message says what the box is for.
    auto leaked = f.run("YAPAYZEKAMODELİ islem=ekle ad=Bulut adres=https://api.deepseek.com/v1 "
                        "model=deepseek-chat anahtar_ref=sk-proj-4Kd93jfPQmz01LbnAZXyT7wRuVc8HsEg");
    CHECK_FALSE(leaked.ok());
    CHECK(leaked.error().message.find("API anahtarına benziyor") != std::string::npos);
    CHECK(f.asked.empty());               // nothing reached the engine
    CHECK(journal_of(f.journal).empty()); // and nothing reached the journal
    CHECK(f.profiles.find("Bulut") == nullptr);

    // A prefix-less token is caught by its shape, which is what `looks_like_secret`
    // is for (ai/redact.hpp).
    auto shaped = f.run("YAPAYZEKAMODELİ islem=ekle ad=Bulut adres=https://api.deepseek.com/v1 "
                        "model=deepseek-chat anahtar_ref=aZ19Kd93jfPQmz01LbnAZXyT7wRuVc8HsEg42");
    CHECK_FALSE(shaped.ok());
    CHECK(journal_of(f.journal).empty());

    // THE NAME, HOWEVER, IS ORDINARY DATA and belongs in the journal: a replay
    // that lost it would rebuild a profile that cannot authenticate.
    f.must("YAPAYZEKAMODELİ islem=ekle ad=Bulut adres=https://api.deepseek.com/v1 "
           "model=deepseek-chat anahtar_ref=DEEPSEEK_API_KEY");
    const std::string record = journal_of(f.journal);
    CHECK(record.find("core.ai_provider") != std::string::npos);
    CHECK(record.find("DEEPSEEK_API_KEY") != std::string::npos);
    CHECK(record.find("api.deepseek.com") != std::string::npos);
    CHECK(record.find("deepseek-chat") != std::string::npos);
    // And nothing in that line looks like a credential, by the same test the
    // command refused the pasted key with.
    CHECK_FALSE(ai::looks_like_secret(record));
    CHECK(record.find("sk-") == std::string::npos);
}

TEST_CASE("YAPAYZEKAMODELİ: her ret kendi sebebini adıyla söyler")
{
    Rig f;

    // A word that is not an operation, from a script: the BUS refuses it before
    // the body runs, because the words are declared (`Param::choice`).
    auto unknown = f.run("YAPAYZEKAMODELİ islem=parlat");
    CHECK_FALSE(unknown.ok());
    CHECK(unknown.error().message.find("islem") != std::string::npos);
    CHECK(unknown.error().message.find("listele") != std::string::npos);
    CHECK(f.asked.empty());

    // An operation that needs a name and did not get one.
    auto nameless = f.run("YAPAYZEKAMODELİ islem=sil");
    CHECK_FALSE(nameless.ok());
    CHECK(nameless.error().message.find("profil adı gerekir") != std::string::npos);

    // A dialect this program does not speak.
    auto klingon = f.run("YAPAYZEKAMODELİ islem=ekle ad=Uzak adres=https://ornek.gecersiz/v1 "
                         "model=m lehce=klingon");
    CHECK_FALSE(klingon.ok());
    CHECK(klingon.error().message.find("lehce") != std::string::npos);
    CHECK(klingon.error().message.find("openai_chat") != std::string::npos);

    // A name no profile has, for each of the three verbs that need one.
    for (const char* line : {"YAPAYZEKAMODELİ islem=sil ad=\"Yok Böyle\"",
                             "YAPAYZEKAMODELİ islem=varsayilan ad=\"Yok Böyle\"",
                             "YAPAYZEKAMODELİ islem=dene ad=\"Yok Böyle\""}) {
        auto missing = f.run(line);
        CHECK_FALSE(missing.ok());
        CHECK(missing.error().message.find("Yok Böyle") != std::string::npos);
    }

    // An address the store cannot use, refused by the store through the engine.
    auto schemeless = f.run("YAPAYZEKAMODELİ islem=ekle ad=Çıplak adres=ornek.gecersiz model=m");
    CHECK_FALSE(schemeless.ok());
    CHECK(schemeless.error().message.find("http") != std::string::npos);

    // A temperature off the scale, likewise: the range is the store's, because
    // the bus checks a range with `as_int()` and would truncate 2.5 to 2.
    auto hot = f.run("YAPAYZEKAMODELİ islem=ekle ad=Sıcak adres=https://ornek.gecerli/v1 "
                     "model=m sicaklik=7.5");
    CHECK_FALSE(hot.ok());
    CHECK(hot.error().message.find("sıcaklık") != std::string::npos);

    // A store that is not attached at all says so rather than pretending.
    Rig headless(false);
    auto refused = headless.run("YAPAYZEKAMODELİ islem=listele");
    CHECK_FALSE(refused.ok());
    CHECK(refused.error().message.find("bağlı değil") != std::string::npos);
    CHECK(journal_of(headless.journal).empty());
}

TEST_CASE("YAPAYZEKAMODELİ: arayüz, komut satırı ve betik aynı depoyu ve aynı günlüğü bırakır")
{
    // THE EQUALITY PROOF (Article 6.4, test.md R3): three clients, one command,
    // one store and one byte-identical journal line. The profile fields travel
    // the same way whichever client said them, which is what makes the settings
    // page a client rather than a privilege (Article 1.2).
    const char* line = "YAPAYZEKAMODELİ islem=ekle ad=\"Kurum vLLM\" lehce=anthropic_messages "
                       "adres=https://api.anthropic.com/v1 yol=/messages "
                       "model=claude-sonnet-4-5 anahtar_ref=anthropic baglam=200000 azami=8192 "
                       "akis=evet dusunme=evet araclar=evet";

    // ---- client 1: the settings page. A button starts the command with every
    //      argument already answered, exactly as `Controller::runLine` does. ----
    Rig gui;
    {
        auto started = gui.bus.begin_interactive(line, Origin::Gui);
        REQUIRE(started.ok());
        auto& session = *started.value();
        CHECK(session.finished()); // nothing left to ask: the line said it all
        auto done = gui.bus.finish(session);
        if (!done) FAIL_WITH("arayüz", done.error().message);
    }

    // ---- client 2: the command line, typed by the user ----
    Rig cli;
    {
        auto r = cli.bus.execute_line(line, Origin::CommandLine);
        if (!r) FAIL_WITH("komut satırı", r.error().message);
    }

    // ---- client 3: a JSON script ----
    Rig scr;
    {
        script::JsonRunner runner(scr.bus, script::Sandbox::Project);
        auto r = runner.run_text(R"({
            "ad": "Model profili kanıtı",
            "komutlar": [
                {"cmd": "core.ai_provider",
                 "args": {"islem": "ekle",
                          "ad": "Kurum vLLM",
                          "lehce": "anthropic_messages",
                          "adres": "https://api.anthropic.com/v1",
                          "yol": "/messages",
                          "model": "claude-sonnet-4-5",
                          "anahtar_ref": "anthropic",
                          "baglam": 200000,
                          "azami": 8192,
                          "akis": true,
                          "dusunme": true,
                          "araclar": true}}
            ]
        })");
        if (!r) FAIL_WITH("betik", r.error().message);
    }

    // The same store, to the byte: `to_json` is deterministic and preserves key
    // order (core/json.hpp), so this is an exact comparison rather than a
    // heuristic.
    CHECK_EQ(gui.profiles.to_json(), cli.profiles.to_json());
    CHECK_EQ(cli.profiles.to_json(), scr.profiles.to_json());
    REQUIRE(gui.profiles.find("Kurum vLLM") != nullptr);
    CHECK(*gui.profiles.find("Kurum vLLM") == *scr.profiles.find("Kurum vLLM"));

    // The same journal line, to the byte — `origin` and the timestamp are the
    // only two fields that may legitimately differ (test_proof.cpp).
    CHECK_EQ(journal_of(gui.journal), journal_of(cli.journal));
    CHECK_EQ(journal_of(cli.journal), journal_of(scr.journal));
    CHECK_FALSE(journal_of(gui.journal).empty());

    // And each client is recorded honestly as itself.
    REQUIRE_FALSE(gui.journal.entries().empty());
    CHECK(gui.journal.entries().at(0).origin == Origin::Gui);
    CHECK(cli.journal.entries().at(0).origin == Origin::CommandLine);
    CHECK(scr.journal.entries().at(0).origin == Origin::Script);
}

// ================================================= the vendor catalogue ======

TEST_CASE("Sağlayıcı kataloğu: paket okunur ve her kayıt tutarlıdır")
{
    // THE SHIPPED FILE ITSELF, not a fixture. The endpoints and model ids in it
    // are the fastest-rotting facts in the program and they are DATA precisely so
    // they can be corrected without a rebuild — which is worth nothing if nothing
    // checks that the corrected file still parses (`ai/provider_catalog.hpp`).
    const std::string path = std::string(KENTOS_DATA_DIR) + "/catalogs/ai/saglayicilar.json";
    std::ifstream in(path, std::ios::binary);
    REQUIRE(in.good());
    const std::string text{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};

    const core::Result<ai::ProviderCatalog> parsed = ai::ProviderCatalog::from_json(text);
    REQUIRE(parsed.ok());
    const ai::ProviderCatalog& catalog = parsed.value();
    CHECK(catalog.empty() == false);
    CHECK(catalog.package_version().empty() == false);

    bool dialect_seen[4]    = {false, false, false, false};
    std::size_t local_seeds = 0;
    for (const ai::CatalogVendor& v : catalog.vendors()) {
        CAPTURE(v.id);
        CHECK(v.name.empty() == false);
        CHECK(v.models.empty() == false);
        CHECK(v.preferred_model() != nullptr);

        // THE ADDRESS AND THE PATH JOIN INTO THE URL THAT IS ACTUALLY POSTED TO,
        // so a doubled or missing slash is a 404 nobody would attribute to this
        // file. `endpoint_url` is the joiner, and it is checked here rather than
        // trusted.
        CHECK((v.base_url.rfind("http://", 0) == 0 || v.base_url.rfind("https://", 0) == 0));
        CHECK(v.base_url.back() != '/');
        CHECK(v.path.front() == '/');

        // NO CREDENTIAL IS IN THE CATALOGUE, only the NAME of the entry that
        // holds one (CLAUDE.md 5.21, ai.md P11).
        CHECK(ai::looks_like_secret(v.key_ref) == false);
        // A CLOUD ENDPOINT MUST NAME THE ENTRY ITS KEY LIVES IN — otherwise the
        // transport has nothing to look up and the request goes out
        // unauthenticated. The reverse does NOT hold: a local endpoint usually
        // needs no credential, but a self-hosted gateway in front of other
        // providers takes a master key, so `local` says where it runs and not
        // whether it authenticates.
        if (!v.local) CHECK(v.key_ref.empty() == false);
        // And a vendor that asks for no header must not name a key either, or
        // the key would be read out of the store and then dropped.
        if (v.auth_header.empty()) CHECK(v.key_ref.empty());

        // A list path and a list shape are one fact in two fields, so neither
        // may be set without the other.
        CHECK((v.models_path.empty()) == (v.list_shape == ai::ModelListShape::None));

        for (const ai::CatalogModel& m : v.models) {
            CAPTURE(m.id);
            CHECK(m.id.empty() == false);
            CHECK(m.context >= 0);
            CHECK(m.max_output >= 0);
        }

        dialect_seen[static_cast<std::size_t>(v.dialect)] = true;
        if (v.seed && v.local) ++local_seeds;

        // EVERY PROFILE THE CATALOGUE CAN PRODUCE MUST CLEAR THE STORE'S FLOOR.
        // A vendor that `upsert` refuses is a row that simply vanishes from a
        // fresh installation, silently.
        ai::ProviderProfiles one;
        CHECK(one.upsert(ai::profile_for(v)).ok());
    }
    for (bool seen : dialect_seen)
        CHECK(seen);

    // ai.md R13: local support is mandatory, so the seeded set must contain one.
    CHECK(local_seeds > 0);

    const ai::ProviderProfiles seeded = ai::seed_profiles(catalog);
    REQUIRE(seeded.fallback() != nullptr);
    CHECK(ai::reach_of(seeded.fallback()->base_url) == ai::Reach::Loopback);
    CHECK(seeded.all().size() > 1);
    CHECK(seeded.all().size() < catalog.vendors().size()); // seeded is a SUBSET

    // A PROFILE FINDS ITS VENDOR BY ADDRESS, which is how the dialog knows which
    // models to offer for an endpoint the user typed.
    const ai::CatalogVendor* deepseek = catalog.find("deepseek");
    REQUIRE(deepseek != nullptr);
    ai::ProviderProfile mine = ai::profile_for(*deepseek);
    CHECK(catalog.for_profile(mine) == deepseek);
    CHECK(deepseek->models.size() >= 2);

    // And an address nobody catalogued matches nothing rather than the nearest.
    mine.base_url = "https://kurum.ic.agi/llm/v1";
    CHECK(catalog.for_profile(mine) == nullptr);
}

TEST_CASE("Sağlayıcı kataloğu: bozuk ve gelecekten gelen dosya reddedilir")
{
    CHECK(ai::ProviderCatalog::from_json("{ bu json değil").ok() == false);
    CHECK(ai::ProviderCatalog::from_json(R"({"schema_version":1})").ok() == false);
    CHECK(ai::ProviderCatalog::from_json(R"({"schema_version":1,"saglayicilar":[]})").ok() ==
          false);

    const auto future =
        ai::ProviderCatalog::from_json(R"({"schema_version":99,"saglayicilar":[]})");
    REQUIRE(future.ok() == false);
    CHECK(future.error().code == core::ErrorCode::Unsupported);

    // A dialect this build does not know is named in the refusal rather than
    // quietly defaulted: a profile on the wrong dialect talks nonsense to a real
    // endpoint.
    const auto alien = ai::ProviderCatalog::from_json(
        R"({"schema_version":1,"saglayicilar":[{"kimlik":"x","ad":"X","lehce":"mors",)"
        R"("adres":"https://x.y","yol":"/c","modeller":[{"kimlik":"m"}]}]})");
    REQUIRE(alien.ok() == false);
    CHECK(alien.error().message.find("mors") != std::string::npos);
}

TEST_CASE("Model listesi: üç biçim de çözülür, çöp girdi boş liste verir")
{
    const std::vector<std::string> openai = ai::parse_model_list(
        ai::ModelListShape::OpenAi,
        R"({"object":"list","data":[{"id":"gpt-4.1"},{"id":"o3"},{"id":"gpt-4.1"}]})");
    // Duplicates collapse: two rows naming one model are one entry in a chooser.
    REQUIRE(openai.size() == 2);
    CHECK(openai[0] == "gpt-4.1");
    CHECK(openai[1] == "o3");

    const std::vector<std::string> ollama = ai::parse_model_list(
        ai::ModelListShape::Ollama,
        R"({"models":[{"name":"llama3.1:8b","size":1},{"name":"qwen2.5:14b"}]})");
    REQUIRE(ollama.size() == 2);
    CHECK(ollama[0] == "llama3.1:8b");

    const std::vector<std::string> anthropic = ai::parse_model_list(
        ai::ModelListShape::Anthropic,
        R"({"data":[{"id":"claude-sonnet-4-5","display_name":"Claude Sonnet 4.5"}]})");
    REQUIRE(anthropic.size() == 1);
    CHECK(anthropic[0] == "claude-sonnet-4-5");

    // A BODY THAT WILL NOT PARSE IS AN EMPTY LIST, NOT AN ERROR: the chooser
    // falls back to the catalogue and the caller says the endpoint answered
    // with nothing usable. A gateway that returns HTML on an error path is the
    // ordinary case this covers.
    CHECK(ai::parse_model_list(ai::ModelListShape::OpenAi, "<html>502</html>").empty());
    CHECK(ai::parse_model_list(ai::ModelListShape::OpenAi, R"({"data":{}})").empty());
    CHECK(ai::parse_model_list(ai::ModelListShape::None, R"({"data":[{"id":"x"}]})").empty());
}

TEST_CASE("Model listesi isteği profilin kendi adresine gider")
{
    ai::CatalogVendor vendor;
    vendor.id          = "ornek";
    vendor.name        = "Örnek";
    vendor.base_url    = "https://ornek.gecerli/v1";
    vendor.path        = "/chat/completions";
    vendor.models_path = "/models";
    vendor.list_shape  = ai::ModelListShape::OpenAi;
    vendor.models.push_back(ai::CatalogModel{"model-a", "", 1000, 100, false});

    ai::ProviderProfile mine = ai::profile_for(vendor);
    // THE USER MOVED IT TO A MIRROR. The listing must follow the profile, not the
    // catalogue: showing models from a host they are not calling is worse than
    // showing none.
    mine.base_url = "https://ayna.kurum.ic/v1";

    const core::Result<ai::EndpointPermit> permit = ai::permit_for(mine, /*sensitive=*/false);
    REQUIRE(permit.ok());
    const std::optional<ai::HttpRequest> request =
        ai::model_list_request(mine, vendor, permit.value());
    REQUIRE(request.has_value());
    CHECK(request->method == "GET");
    CHECK(request->url == "https://ayna.kurum.ic/v1/models");

    // A vendor with no listing asks for nothing at all, so a caller cannot send a
    // GET to a path that does not exist.
    ai::CatalogVendor silent = vendor;
    silent.models_path.clear();
    silent.list_shape = ai::ModelListShape::None;
    CHECK(ai::model_list_request(mine, silent, permit.value()).has_value() == false);
}

TEST_CASE("Lehçe: ne yapabildiğini bildiriyor")
{
    // TODOS A-07. Without this the chat has to guess, and a chat that guesses
    // sends an image to a dialect with no place for one — which fails at the
    // provider, after the user waited.
    //
    // EVERY FIELD IS A FACT ABOUT THE WIRE LANGUAGE, not about a vendor or a
    // model: `ollama_native` frames NDJSON whatever model is behind it.
    for (const ai::Dialect one : ai::dialects()) {
        const ai::DialectCapabilities caps = ai::capabilities_of(one);
        INFO("lehçe: ", ai::dialect_id(one));
        // All four can be handed a tool catalogue; the field exists so a fifth
        // that cannot is describable rather than silently broken.
        CHECK(caps.tools);
        CHECK(caps.streaming);
    }

    // The local runner has no place for an image in its message shape — which is
    // not the same as "no local model can see".
    CHECK_FALSE(ai::capabilities_of(ai::Dialect::OllamaNative).vision);
    CHECK(ai::capabilities_of(ai::Dialect::OpenAiChat).vision);
    CHECK(ai::capabilities_of(ai::Dialect::AnthropicMessages).vision);

    // Anthropic has no schema-constrained response mode in this revision.
    CHECK_FALSE(ai::capabilities_of(ai::Dialect::AnthropicMessages).structured_output);
    CHECK(ai::capabilities_of(ai::Dialect::OpenAiResponses).structured_output);
}
