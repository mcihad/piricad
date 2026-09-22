// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/script/python_runner.hpp"

#include "kentos_cad/core/text.hpp"

#include <pybind11/embed.h>

#include <filesystem>
#include <fstream>
#include <functional>
#include <mutex>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace py = pybind11;

namespace kentos::script {
namespace {

using core::ErrorCode;

/// How long the interpreter may hold on before it must offer the lock to another
/// thread, in seconds.
///
/// This is the whole of this host's cancellation budget, and it is set rather
/// than assumed. `.claude/script.md` R13 asks for a stop to be noticed at least
/// every 50 ms; CPython's own default is already 5 ms, but a default is a thing
/// that changes and a rule is a thing that must hold. Five milliseconds is a
/// tenth of the budget, so the margin survives a slow machine.
constexpr double kSwitchIntervalSeconds = 0.005;

/// Brings up CPython, ONCE for the whole process.
///
/// WHY ONCE AND NEVER TORN DOWN. CPython is a process-wide singleton with static
/// state in every extension module ever imported, and
/// `Py_FinalizeEx` followed by a second `Py_InitializeFromConfig` is documented
/// as only partly supported — pybind11 adds its own caveats on top, because the
/// types an embedded module registered belong to internals that the finalise
/// does not fully unwind.
///
/// So the interpreter starts on first use and stays up. That is not a leak in any
/// sense that matters: an embedding host keeps one interpreter for as long as the
/// application runs, and tearing one down between two script runs would be
/// paying a documented crash risk for memory the process is about to hand back
/// anyway. Two runners, or a test fixture beside the application's own, therefore
/// SHARE it, which is why this is a free function over a static and not a member.
void ensure_interpreter()
{
    static std::once_flag once;
    std::call_once(once, [] {
        PyConfig config;

        // THE ISOLATED CONFIGURATION, and it is chosen for what it switches OFF
        // (`.claude/script.md` P2: the system Python is never fallen back on).
        // `isolated` implies `use_environment = 0`, so PYTHONPATH, PYTHONHOME and
        // PYTHONSTARTUP cannot redirect this interpreter; `user_site_directory =
        // 0` keeps a per-user package directory out of the path; `safe_path = 1`
        // keeps the SCRIPT'S OWN directory off the front of `sys.path`, which is
        // the difference between a drawing's neighbour file being data and being
        // an import; and `install_signal_handlers = 0` leaves the application's
        // own Ctrl+C alone.
        //
        // What it deliberately leaves ON is `site_import`: the standard library
        // and the packages installed into the interpreter we linked against are
        // importable. §4.2 makes Python the ECOSYSTEM layer — plugins, batch jobs
        // and data pipelines — and an interpreter that cannot import a library is
        // not an ecosystem. The sandbox level is not this list, and
        // `python_runner.hpp` says so out loud.
        PyConfig_InitIsolatedConfig(&config);

        // pybind11 clears the config, including on the failure path.
        py::initialize_interpreter(&config, /*argc=*/0, /*argv=*/nullptr,
                                   /*add_program_dir_to_path=*/false);

        {
            py::gil_scoped_acquire gil;
            py::module_::import("sys").attr("setswitchinterval")(kSwitchIntervalSeconds);
        }

        // The initialising thread holds the lock on return, and a runner may be
        // driven from any thread (R13 puts a long run on its own `std::jthread`).
        // Handing the lock back here is what makes `gil_scoped_acquire` below work
        // from anywhere; the state is deliberately dropped, because nothing ever
        // restores it.
        (void)PyEval_SaveThread();
    });
}

} // namespace

// -----------------------------------------------------------------------------

struct PythonRunner::Impl
{
    explicit Impl(command::Bus& b, Sandbox s) : bus(b), sandbox(s) {}

    command::Bus& bus;
    Sandbox sandbox;
    std::string project_root;

    std::uint64_t consent{0};
    bool has_consent{false};

    std::size_t commands{0};

    /// The bus error that stopped the run, kept because a Python exception
    /// carries only a message and the caller is owed the CODE as well
    /// (`.claude/script.md` R21).
    core::Error pending{ErrorCode::None, {}};
    bool has_pending{false};

    /// What `print()` has written since the last newline.
    ///
    /// `print` calls `write` twice — the text, then the newline — so a sink that
    /// echoed every call would put half a sentence on the command line and then
    /// an empty one.
    std::string out_buffer;

    /// Raises a Python exception from a binding. `throw`, not a CPython call:
    /// pybind11 converts a C++ exception at the boundary from a frame where every
    /// C++ object on the way has already been destroyed, which a raw `PyErr_` set
    /// followed by an unwind through the binding does not promise.
    [[noreturn]] void fail(ErrorCode code, std::string message)
    {
        pending     = core::Error{code, message};
        has_pending = true;
        throw std::runtime_error(std::move(message));
    }

    /// Refuses a path the level does not reach. Asked per call rather than once,
    /// because a check made at bind time stays open after the level narrows.
    void require_path(const std::string& path, std::string_view what)
    {
        if (sandbox == Sandbox::Safe)
            fail(ErrorCode::Unsupported, std::string(what) +
                                             " 'güvenli' kum havuzunda kapalıdır. Gerekli "
                                             "seviye: 'proje' veya 'tam'.");
        if (sandbox == Sandbox::Project && !path_within_root(project_root, path))
            fail(ErrorCode::Unsupported,
                 "'proje' kum havuzu yalnızca proje dizinine izin verir; şu yol dışarıda: " + path);
    }

    void write_out(std::string_view text);
    void flush_out();

    py::object build_module();
};

void PythonRunner::Impl::write_out(std::string_view text)
{
    out_buffer += text;
    for (std::size_t nl = out_buffer.find('\n'); nl != std::string::npos;
         nl             = out_buffer.find('\n')) {
        bus.echo(std::string_view(out_buffer).substr(0, nl));
        out_buffer.erase(0, nl + 1);
    }
}

void PythonRunner::Impl::flush_out()
{
    if (out_buffer.empty()) return;
    bus.echo(out_buffer);
    out_buffer.clear();
}

/// Builds the `kentos.cad` module for THIS runner.
///
/// Built at run time from lambdas that capture `this`, and not with
/// `PYBIND11_EMBEDDED_MODULE`: that macro registers a module for the process and
/// its body has no runner to talk to, so it would need a global pointer to the
/// "current" one. A module object per runner needs no such global, and two
/// runners over two buses cannot reach each other's document by accident.
py::object PythonRunner::Impl::build_module()
{
    py::object module_type = py::module_::import("types").attr("ModuleType");

    py::object package = module_type("kentos");
    py::object cad     = module_type("kentos.cad");

    package.attr("cad")     = cad;
    package.attr("__all__") = py::make_tuple("cad");

    // ---- the one write path (R1, R4) ----------------------------------------
    //
    // Every mutation is a command line dispatched through the bus, parsed by the
    // ONE parser (CLAUDE.md 5.11). There is no second entry point, no "fast"
    // internal helper and no way to reach a `Transaction` from Python (P3, P10).
    //
    // Variadic and joined with spaces, so both of these read naturally:
    //     cad.run("ÇİZGİ 0,0 10,10")
    //     cad.run("ÇİZGİ", "0,0", "10,10")
    //
    // The per-command surface — `cad.line(points=...)` for every command in
    // `Registry` — is projected on top of this one and lands beside this file; a
    // hand-written wrapper per command would be the binding table CLAUDE.md 5.10
    // forbids.
    cad.attr("run") = py::cpp_function(
        [this](const py::args& parts) {
            std::string line;
            for (const py::handle part : parts) {
                if (!line.empty()) line += ' ';
                line += py::cast<std::string>(py::str(part));
            }
            if (line.empty()) fail(ErrorCode::InvalidArgument, "cad.run() boş komut satırı aldı.");

            auto result = bus.execute_line(line, command::Origin::Script);
            if (!result)
                fail(result.error().code, "Betik komutu (" + line + "): " + result.error().message);

            ++commands;
            return result.value().ops;
        },
        py::name("run"), py::scope(cad),
        py::doc("Dispatch one command line through the bus. Returns the operation count."));

    // ---- reads: VALUES ONLY (R9, R10, P4) -----------------------------------
    //
    // Not one of these hands back a `Document&`, a `Layer*`, an iterator or a
    // non-const span. A script that held a reference into the document would hold
    // it across a command that reallocates the store, and that is a crash with a
    // script's name on it.

    cad.attr("layers") = py::cpp_function(
        [this] {
            py::list names;
            for (const core::Layer& layer : bus.document().layers())
                names.append(py::str(layer.name));
            return names;
        },
        py::name("layers"), py::scope(cad), py::doc("The layer names, in document order."));

    cad.attr("layer_count") = py::cpp_function(
        [this] { return bus.document().layer_table().size(); }, py::name("layer_count"),
        py::scope(cad), py::doc("How many layers the document has."));

    cad.attr("active_layer") = py::cpp_function(
        [this]() -> std::string {
            const core::LayerId id             = bus.active_layer();
            const std::vector<core::Layer>& ls = bus.document().layers();
            if (id >= ls.size()) return {};
            return ls[id].name;
        },
        py::name("active_layer"), py::scope(cad),
        py::doc("The active layer's name, empty when there is none."));

    cad.attr("entity_count") = py::cpp_function(
        [this] { return bus.document().live_entity_count(); }, py::name("entity_count"),
        py::scope(cad), py::doc("How many live entities the document holds."));

    cad.attr("selection_count") =
        py::cpp_function([this] { return bus.selection().size(); }, py::name("selection_count"),
                         py::scope(cad), py::doc("How many entities are selected."));

    cad.attr("crs") =
        py::cpp_function([this] { return bus.document().crs().id(); }, py::name("crs"),
                         py::scope(cad), py::doc("The document's coordinate reference system id."));

    // A setting as the Python type it actually is, rather than everything as a
    // string: `if cad.setting("ızgara.acik"):` is what a user will write, and it
    // only reads correctly if a Bool arrives as a bool.
    cad.attr("setting") = py::cpp_function(
        [this](const std::string& id) -> py::object {
            const core::SettingValue v = bus.setting(id);
            switch (v.type()) {
            case core::SettingType::Bool: return py::bool_(v.as_bool());
            case core::SettingType::Text: return py::str(std::string(v.as_text()));
            case core::SettingType::Int:
            case core::SettingType::Length:
            case core::SettingType::Enum: return py::int_(v.scalar());
            }
            return py::none();
        },
        py::name("setting"), py::scope(cad), py::arg("id"),
        py::doc("One session setting, as bool, str or int."));

    // ---- the filesystem, and only where the sandbox opens it (P8) -----------
    //
    // These are NOT a jail around `open()`: Python's own `open` is in the one
    // interpreter and cannot be taken away, which `python_runner.hpp` states
    // plainly. They are the path the BINDINGS offer, jailed to the project
    // directory at `proje` and refused at `güvenli`, so a script that means to
    // stay inside the rules has a way to and the journal records the level it ran
    // at either way.
    cad.attr("read_file") = py::cpp_function(
        [this](const std::string& path) -> std::string {
            require_path(path, "Dosya okuma");

            std::ifstream in(path, std::ios::in | std::ios::binary);
            if (!in) fail(ErrorCode::IoFailure, "Dosya açılamadı: " + path);
            std::ostringstream buf;
            buf << in.rdbuf();
            return buf.str();
        },
        py::name("read_file"), py::scope(cad), py::arg("path"),
        py::doc("Read a text file the sandbox level permits."));

    cad.attr("write_file") = py::cpp_function(
        [this](const std::string& path, const std::string& text) {
            require_path(path, "Dosya yazma");

            std::ofstream out(path, std::ios::out | std::ios::binary | std::ios::trunc);
            if (!out) fail(ErrorCode::IoFailure, "Dosya yazmak için açılamadı: " + path);
            out << text;
        },
        py::name("write_file"), py::scope(cad), py::arg("path"), py::arg("text"),
        py::doc("Write a text file the sandbox level permits."));

    // The LEVEL's own Turkish name, because that is what the user typed at MOD
    // and what the journal records; the FUNCTION's name is English like the rest
    // of the surface.
    cad.attr("sandbox") = py::cpp_function(
        [this] { return std::string(sandbox_name(sandbox)); }, py::name("sandbox"), py::scope(cad),
        py::doc("The sandbox level this run is under: güvenli, proje or tam."));

    return package;
}

// -----------------------------------------------------------------------------

PythonRunner::PythonRunner(command::Bus& bus, Sandbox sandbox)
    : impl_(std::make_unique<Impl>(bus, sandbox))
{}

PythonRunner::~PythonRunner() = default;

void PythonRunner::set_project_root(std::string path)
{
    impl_->project_root = std::move(path);
}

void PythonRunner::grant_full(std::uint64_t identity)
{
    impl_->consent     = identity;
    impl_->has_consent = true;
}

Sandbox PythonRunner::sandbox() const noexcept
{
    return impl_->sandbox;
}

core::Result<RunReport> PythonRunner::run_text(std::string_view source, std::string label,
                                               std::stop_token token)
{
    const std::uint64_t identity = script_identity(source);

    // CONSENT FIRST, before the interpreter is even reached. R12 makes `tam` a
    // decision about THIS script, and P7 forbids any setting, flag or header from
    // standing in for it — so a `tam` runner with no matching grant runs nothing.
    const bool consented = impl_->has_consent && impl_->consent == identity;
    if (impl_->sandbox == Sandbox::Full && !consented) {
        journal_run(impl_->bus, "python", label, impl_->sandbox, identity, /*consented=*/false);
        return core::err(ErrorCode::Unsupported,
                         "'tam' kum havuzu bu betik için onaylanmamış. Onay betiğin kendisine "
                         "verilir; dosya değiştiyse yeniden sorulur.");
    }

    // The run's own record, before the first command (R11).
    journal_run(impl_->bus, "python", label, impl_->sandbox, identity, consented);

    ensure_interpreter();

    impl_->commands    = 0;
    impl_->has_pending = false;
    impl_->out_buffer.clear();

    // ONE SCRIPT IS ONE UNDO STEP and ONE validation pass (R14, R15).
    if (auto st = impl_->bus.begin_batch(label); !st) return st.error();

    bool ok = false;
    std::string python_error;

    {
        py::gil_scoped_acquire gil;

        try {
            py::object package = impl_->build_module();
            py::object cad     = package.attr("cad");

            // Registered so `import kentos.cad` and `from kentos import cad`
            // resolve, then ALSO handed to the script's own globals, so a one-line
            // script needs no import at all. A one-liner typed at an evaluator
            // should not have to open with an import.
            py::dict modules      = py::module_::import("sys").attr("modules");
            modules["kentos"]     = package;
            modules["kentos.cad"] = cad;

            py::dict globals;
            globals["__builtins__"] = py::module_::import("builtins");
            globals["__name__"]     = py::str("__kentos_script__");
            globals["kentos"]       = package;
            globals["cad"]          = cad;

            // `print()` LANDS ON THE COMMAND LINE rather than on a stdout nobody
            // is looking at. A windowed application has no terminal, so a script
            // that reports its progress would otherwise report it into the void.
            py::object stream = py::module_::import("types").attr("SimpleNamespace")(
                py::arg("write") =
                    py::cpp_function([this](const std::string& text) { impl_->write_out(text); }),
                py::arg("flush")    = py::cpp_function([this] { impl_->flush_out(); }),
                py::arg("isatty")   = py::cpp_function([] { return false; }),
                py::arg("fileno")   = py::cpp_function([] { return -1; }),
                py::arg("writable") = py::cpp_function([] { return true; }));

            py::module_ sys      = py::module_::import("sys");
            py::object saved_out = sys.attr("stdout");
            py::object saved_err = sys.attr("stderr");
            sys.attr("stdout")   = stream;
            sys.attr("stderr")   = stream;

            // HOW A STOP ACTUALLY STOPS IT, and it costs nothing while the script
            // is behaving. CPython offers the lock to other threads every
            // `kSwitchIntervalSeconds`, so a callback on another thread can take
            // it and set an asynchronous exception on the running thread, which
            // CPython raises at the next bytecode boundary. No polling hook, no
            // per-instruction cost, and a bound of
            // one switch interval, and the script pays nothing for it.
            //
            // The one case it cannot interrupt is a C call that holds the lock and
            // never returns — a `time.sleep(1e9)` releases it and stops at once, a
            // C loop inside a third-party extension does not. The stop is then
            // noticed when that call returns, and `token.stop_requested()` below
            // means the run is reported as cancelled and rolled back either way,
            // however the script treated the exception.
            const auto thread_id =
                py::cast<unsigned long>(py::module_::import("threading").attr("get_ident")());

            // HELD IN AN `optional` SO IT CAN BE DESTROYED DELIBERATELY, with the
            // lock released. That is a deadlock and not a nicety:
            // `~stop_callback` waits for a callback that has already started on
            // another thread, and that callback is waiting for the lock this
            // thread is holding. A run that finishes at the same instant the user
            // presses stop would hang the application forever.
            std::optional<std::stop_callback<std::function<void()>>> interrupt;
            interrupt.emplace(token, std::function<void()>([thread_id] {
                                  py::gil_scoped_acquire nested;
                                  PyThreadState_SetAsyncExc(thread_id, PyExc_KeyboardInterrupt);
                              }));

            try {
                py::exec(std::string(source), globals);
                impl_->flush_out();
                ok = true;
            } catch (const py::error_already_set& e) {
                impl_->flush_out();
                python_error = e.what();
            }

            {
                py::gil_scoped_release unlock;
                interrupt.reset();
            }

            // A STOP THAT ARRIVED TOO LATE MUST NOT FIRE IN THE NEXT RUN. An
            // asynchronous exception is raised at the next bytecode boundary, and
            // if the script finished first that boundary belongs to whatever runs
            // next — a second script would die on an interrupt aimed at the first.
            // Nothing can set one any more, so clearing it here is final.
            PyThreadState_SetAsyncExc(thread_id, nullptr);

            sys.attr("stdout") = saved_out;
            sys.attr("stderr") = saved_err;
        } catch (const py::error_already_set& e) {
            // The setup itself failed — a broken interpreter, not a broken script.
            python_error = e.what();
        }
    }

    // A SCRIPT THE USER STOPPED IS NOT A SCRIPT THAT SUCCEEDED, whatever the
    // exception did on the way out. A `try: ... except: pass` around the whole
    // body would otherwise swallow the interrupt and leave half an ifraz applied.
    if (token.stop_requested()) {
        ok = false;
        if (python_error.empty()) python_error = "Betik iptal edildi.";
    }

    if (!ok) {
        // A failing script leaves NOTHING behind. Half-applied cadastral or zoning
        // edits are never acceptable (CLAUDE.md 1.6, §2.5).
        auto closed = impl_->bus.end_batch();
        if (closed && closed.value().mutated) {
            std::string discarded;
            (void)impl_->bus.undo_stack().undo(impl_->bus.document(), &discarded);
        }

        // The bus's own code and sentence when a command failed; Python's when the
        // body itself did. Reporting every script failure as `ParseError` would
        // tell a user their syntax was wrong when their ada number was.
        if (impl_->has_pending) return impl_->pending;

        return core::err(ErrorCode::ParseError, "Python betiği: " + python_error);
    }

    auto closed = impl_->bus.end_batch();
    if (!closed) return closed.error();

    RunReport report;
    report.label    = std::move(label);
    report.commands = impl_->commands;
    report.ops      = closed.value().ops;
    return report;
}

core::Result<RunReport> PythonRunner::run_file(const std::string& path, std::stop_token token)
{
    if (impl_->sandbox == Sandbox::Safe)
        return core::err(ErrorCode::Unsupported,
                         "Betik dosya erişimi 'güvenli' kum havuzunda kapalıdır. "
                         "Gerekli seviye: 'proje' veya 'tam'.");
    if (impl_->sandbox == Sandbox::Project && !path_within_root(impl_->project_root, path))
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

void install(command::Bus& bus, PythonRunner& runner)
{
    bus.on_run_script = [&runner](const std::string& path) -> core::Status {
        auto r = runner.run_file(path);
        if (!r) return r.error();
        return core::ok();
    };
}

void install(command::Bus& bus, JsonRunner& json, PythonRunner& python)
{
    bus.on_run_script = [&json, &python](const std::string& path) -> core::Status {
        // Folded, not lowercased: `.PY` and `.py` are the same extension and
        // `std::tolower` is banned on this alphabet (CLAUDE.md 5.6). The extension
        // is ASCII, but the rule has no exceptions for the cases that happen to be
        // safe — that is how the unsafe one gets written next to it.
        const std::string ext =
            core::turkish_fold_key(std::filesystem::path(path).extension().string());

        if (ext == core::turkish_fold_key(".py")) {
            auto r = python.run_file(path);
            if (!r) return r.error();
            return core::ok();
        }

        auto r = json.run_file(path);
        if (!r) return r.error();
        return core::ok();
    };
}

} // namespace kentos::script
