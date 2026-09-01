// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/script/lua_runner.hpp"

#include "piricad/core/text.hpp"

#include <sol/sol.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace piricad::script {
namespace {

using core::ErrorCode;

/// How many VM instructions between cancellation checks.
///
/// `.claude/script.md` R13 asks for at least every 50 ms. Ten thousand
/// instructions is tens of microseconds on any machine this runs on, so the
/// margin is three orders of magnitude and the hook still costs nothing
/// measurable — a script that loops forever stops when it is told to, which is
/// what P11 means by "cancellation is not advisory".
constexpr int kHookInstructions = 10000;

/// The token the running chunk is watching.
///
/// Thread-local rather than a member, because a Lua debug hook is a C function
/// pointer with no place to carry one. R13 puts a long-running script on its own
/// `std::jthread`, so one per thread is exactly one per run.
thread_local const std::stop_token* g_token = nullptr;

void cancel_hook(lua_State* L, lua_Debug*)
{
    if (g_token != nullptr && g_token->stop_requested())
        luaL_error(L, "Betik iptal edildi."); // never returns
}

/// True when `path` resolves inside `root`.
///
/// Resolved with `weakly_canonical` on BOTH sides before comparing, which is the
/// whole point: `proje/../../etc/passwd` is a path inside the project directory
/// spelled as text and outside it in fact, and a string prefix test says yes to it
/// (`.claude/script.md` P8).
bool inside(const std::filesystem::path& root, const std::filesystem::path& path)
{
    if (root.empty()) return false;

    std::error_code ec;
    const std::filesystem::path base = std::filesystem::weakly_canonical(root, ec);
    if (ec) return false;
    const std::filesystem::path target = std::filesystem::weakly_canonical(path, ec);
    if (ec) return false;

    auto b = base.begin();
    auto t = target.begin();
    for (; b != base.end(); ++b, ++t) {
        if (t == target.end() || *t != *b) return false;
    }
    return true;
}

} // namespace

// -----------------------------------------------------------------------------

struct LuaRunner::Impl
{
    explicit Impl(command::Bus& b, Sandbox s) : bus(b), sandbox(s) {}

    command::Bus& bus;
    Sandbox sandbox;
    std::string project_root;

    std::uint64_t consent{0};
    bool has_consent{false};

    sol::state lua;
    bool opened{false};

    /// The bus error that stopped the run, kept because a Lua error carries only
    /// a string and the caller is owed the CODE as well (`.claude/script.md` R21).
    core::Error pending{ErrorCode::None, {}};
    bool has_pending{false};

    std::size_t commands{0};

    /// Raises a Lua error. `throw`, not `luaL_error`: sol2 catches at the binding
    /// boundary and calls `lua_error` from a frame where every C++ object has been
    /// destroyed, whereas `luaL_error` longjmps straight past those destructors.
    [[noreturn]] void fail(ErrorCode code, std::string message)
    {
        pending     = core::Error{code, message};
        has_pending = true;
        throw std::runtime_error(std::move(message));
    }

    void open();
    void bind();
};

void LuaRunner::Impl::open()
{
    if (opened) return;

    // WHAT EACH LEVEL OPENS, and the list is the sandbox (§4.3, P8).
    //
    // `güvenli` gets the pure libraries and nothing that reaches outside the
    // interpreter: no `io`, no `os`, no `package`, no `require`, no `debug`. That
    // is not a hardening pass over an open interpreter, which is how sandboxes
    // leak; it is the set of libraries that were opened at all.
    lua.open_libraries(sol::lib::base, sol::lib::string, sol::lib::table, sol::lib::math,
                       sol::lib::utf8, sol::lib::coroutine);

    if (sandbox == Sandbox::Full) {
        // `tam`, and only after `grant_full` matched this script's identity —
        // `run_text` checks that before it gets here (R12, P7).
        lua.open_libraries(sol::lib::io, sol::lib::os, sol::lib::package, sol::lib::string);
    } else {
        // Even inside `base`, four functions load code from somewhere else and
        // `os.exit` takes the application down. None of them is reachable below
        // `tam`.
        lua["dofile"]         = sol::nil;
        lua["loadfile"]       = sol::nil;
        lua["load"]           = sol::nil;
        lua["loadstring"]     = sol::nil;
        lua["require"]        = sol::nil;
        lua["collectgarbage"] = sol::nil;
    }

    bind();
    opened = true;
}

void LuaRunner::Impl::bind()
{
    sol::table h = lua.create_named_table("h");

    // ---- the one write path (R1, R4) ----------------------------------------
    //
    // Every mutation is a command line dispatched through the bus, parsed by the
    // ONE parser (CLAUDE.md 5.11). There is no second entry point, no "fast"
    // internal helper and no way to reach a `Transaction` from Lua (P3, P10).
    //
    // Variadic and joined with spaces, so both of these read naturally:
    //     h.komut("ÇİZGİ 0,0 10,10")
    //     h.komut("ÇİZGİ", "0,0", "10,10")
    h.set_function("komut", [this](sol::variadic_args va) {
        std::string line;
        for (auto arg : va) {
            if (!line.empty()) line += ' ';
            line += arg.as<std::string>();
        }
        if (line.empty()) fail(ErrorCode::InvalidArgument, "h.komut() boş komut satırı aldı.");

        auto result = bus.execute_line(line, command::Origin::Script);
        if (!result)
            fail(result.error().code, "Betik komutu (" + line + "): " + result.error().message);

        ++commands;
        return result.value().ops;
    });

    // ---- reads: VALUES ONLY (R9, R10, P4) -----------------------------------
    //
    // Not one of these hands back a `Document&`, a `Layer*`, an iterator or a
    // non-const span. A script that held a reference into the document would hold
    // it across a command that reallocates the store, and that is a crash with a
    // script's name on it.

    h.set_function("katmanlar", [this] {
        sol::table names = lua.create_table();
        int i            = 1;
        for (const core::Layer& layer : bus.document().layers())
            names[i++] = layer.name;
        return names;
    });

    h.set_function("katman_sayisi", [this] { return bus.document().layer_table().size(); });

    h.set_function("aktif_katman", [this]() -> std::string {
        const core::LayerId id             = bus.active_layer();
        const std::vector<core::Layer>& ls = bus.document().layers();
        if (id >= ls.size()) return {};
        return ls[id].name;
    });

    h.set_function("nesne_sayisi", [this] { return bus.document().live_entity_count(); });

    h.set_function("secim_sayisi", [this] { return bus.selection().size(); });

    h.set_function("krs", [this] { return bus.document().crs().id(); });

    // A setting as the Lua type it actually is, rather than everything as a
    // string: `if h.ayar("ızgara.acik") then` is what a user will write, and it
    // only works if a Bool arrives as a boolean.
    h.set_function("ayar", [this](const std::string& id) -> sol::object {
        const core::SettingValue v = bus.setting(id);
        switch (v.type()) {
        case core::SettingType::Bool: return sol::make_object(lua, v.as_bool());
        case core::SettingType::Text: return sol::make_object(lua, std::string(v.as_text()));
        case core::SettingType::Int:
        case core::SettingType::Length:
        case core::SettingType::Enum: return sol::make_object(lua, static_cast<double>(v.scalar()));
        }
        return sol::make_object(lua, sol::nil);
    });

    // ---- the filesystem, and only where the sandbox opens it (P8) -----------
    //
    // Lua's own `io` is never opened below `tam`. These two take its place at
    // `proje`, jailed to the project directory and refused outright at `güvenli`.
    // A helper that checked the level once at bind time would be a helper that
    // stayed open after the level narrowed, so each call asks.
    h.set_function("dosya_oku", [this](const std::string& path) -> std::string {
        if (sandbox == Sandbox::Safe)
            fail(ErrorCode::Unsupported,
                 "Dosya okuma 'güvenli' kum havuzunda kapalıdır. Gerekli seviye: 'proje' veya "
                 "'tam'.");
        if (sandbox == Sandbox::Project && !inside(project_root, path))
            fail(ErrorCode::Unsupported,
                 "'proje' kum havuzu yalnızca proje dizinine izin verir; şu yol dışarıda: " + path);

        std::ifstream in(path, std::ios::in | std::ios::binary);
        if (!in) fail(ErrorCode::IoFailure, "Dosya açılamadı: " + path);
        std::ostringstream buf;
        buf << in.rdbuf();
        return buf.str();
    });

    h.set_function("dosya_yaz", [this](const std::string& path, const std::string& text) {
        if (sandbox == Sandbox::Safe)
            fail(ErrorCode::Unsupported,
                 "Dosya yazma 'güvenli' kum havuzunda kapalıdır. Gerekli seviye: 'proje' veya "
                 "'tam'.");
        if (sandbox == Sandbox::Project && !inside(project_root, path))
            fail(ErrorCode::Unsupported,
                 "'proje' kum havuzu yalnızca proje dizinine izin verir; şu yol dışarıda: " + path);

        std::ofstream out(path, std::ios::out | std::ios::binary | std::ios::trunc);
        if (!out) fail(ErrorCode::IoFailure, "Dosya yazmak için açılamadı: " + path);
        out << text;
    });

    h.set_function("kum_havuzu", [this] { return std::string(sandbox_name(sandbox)); });
}

// -----------------------------------------------------------------------------

LuaRunner::LuaRunner(command::Bus& bus, Sandbox sandbox)
    : impl_(std::make_unique<Impl>(bus, sandbox))
{}

LuaRunner::~LuaRunner() = default;

void LuaRunner::set_project_root(std::string path)
{
    impl_->project_root = std::move(path);
}

void LuaRunner::grant_full(std::uint64_t identity)
{
    impl_->consent     = identity;
    impl_->has_consent = true;
}

Sandbox LuaRunner::sandbox() const noexcept
{
    return impl_->sandbox;
}

core::Result<RunReport> LuaRunner::run_text(std::string_view lua, std::string label,
                                            std::stop_token token)
{
    const std::uint64_t identity = script_identity(lua);

    // CONSENT FIRST, before the interpreter is even opened. R12 makes `tam` a
    // decision about THIS script, and P7 forbids any setting, flag or header from
    // standing in for it — so a `tam` runner with no matching grant runs nothing.
    const bool consented = impl_->has_consent && impl_->consent == identity;
    if (impl_->sandbox == Sandbox::Full && !consented) {
        journal_run(impl_->bus, "lua", label, impl_->sandbox, identity, /*consented=*/false);
        return core::err(ErrorCode::Unsupported,
                         "'tam' kum havuzu bu betik için onaylanmamış. Onay betiğin kendisine "
                         "verilir; dosya değiştiyse yeniden sorulur.");
    }

    // The run's own record, before the first command (R11).
    journal_run(impl_->bus, "lua", label, impl_->sandbox, identity, consented);

    impl_->open();
    impl_->commands    = 0;
    impl_->has_pending = false;

    // ONE SCRIPT IS ONE UNDO STEP and ONE validation pass (R14, R15).
    if (auto st = impl_->bus.begin_batch(label); !st) return st.error();

    g_token = &token;
    lua_sethook(impl_->lua.lua_state(), cancel_hook, LUA_MASKCOUNT, kHookInstructions);

    sol::protected_function_result result =
        impl_->lua.safe_script(std::string_view(lua), sol::script_pass_on_error);

    lua_sethook(impl_->lua.lua_state(), nullptr, 0, 0);
    g_token = nullptr;

    if (!result.valid()) {
        // A failing script leaves NOTHING behind. Half-applied cadastral or zoning
        // edits are never acceptable (CLAUDE.md 1.6, §2.5).
        auto closed = impl_->bus.end_batch();
        if (closed && closed.value().mutated) {
            std::string discarded;
            (void)impl_->bus.undo_stack().undo(impl_->bus.document(), &discarded);
        }

        // The bus's own code and sentence when a command failed; Lua's when the
        // chunk itself did. Reporting every script failure as `ParseError` would
        // tell a user their syntax was wrong when their ada number was.
        if (impl_->has_pending) return impl_->pending;

        const sol::error err = result;
        return core::err(ErrorCode::ParseError, std::string("Lua betiği: ") + err.what());
    }

    auto closed = impl_->bus.end_batch();
    if (!closed) return closed.error();

    RunReport report;
    report.label    = std::move(label);
    report.commands = impl_->commands;
    report.ops      = closed.value().ops;
    return report;
}

core::Result<RunReport> LuaRunner::run_file(const std::string& path, std::stop_token token)
{
    if (impl_->sandbox == Sandbox::Safe)
        return core::err(ErrorCode::Unsupported,
                         "Betik dosya erişimi 'güvenli' kum havuzunda kapalıdır. "
                         "Gerekli seviye: 'proje' veya 'tam'.");
    if (impl_->sandbox == Sandbox::Project && !inside(impl_->project_root, path))
        return core::err(ErrorCode::Unsupported,
                         "'proje' kum havuzu yalnızca proje dizinine izin verir; şu yol "
                         "dışarıda: " +
                             path);

    std::ifstream in(path, std::ios::in | std::ios::binary);
    if (!in) return core::err(ErrorCode::IoFailure, "Betik dosyası açılamadı: " + path);

    std::ostringstream buf;
    buf << in.rdbuf();
    return run_text(buf.str(), path, std::move(token));
}

void install(command::Bus& bus, LuaRunner& runner)
{
    bus.on_run_script = [&runner](const std::string& path) -> core::Status {
        auto r = runner.run_file(path);
        if (!r) return r.error();
        return core::ok();
    };
}

void install(command::Bus& bus, JsonRunner& json, LuaRunner& lua)
{
    bus.on_run_script = [&json, &lua](const std::string& path) -> core::Status {
        // Folded, not lowercased: `.LUA` and `.lua` are the same extension and
        // `std::tolower` is banned on this alphabet (CLAUDE.md 5.6). The extension
        // is ASCII, but the rule has no exceptions for the cases that happen to be
        // safe — that is how the unsafe one gets written next to it.
        const std::string ext =
            core::turkish_fold_key(std::filesystem::path(path).extension().string());

        if (ext == core::turkish_fold_key(".lua")) {
            auto r = lua.run_file(path);
            if (!r) return r.error();
            return core::ok();
        }

        auto r = json.run_file(path);
        if (!r) return r.error();
        return core::ok();
    };
}

} // namespace piricad::script
