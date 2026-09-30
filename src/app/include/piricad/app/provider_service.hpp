// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — app: the model profiles, on disk and answerable.
//
// `/src/ai` owns the RECORD — a named endpoint, its dialect, its context window
// and the NAME of its key-store entry (`ai/provider.hpp`) — and deliberately owns
// no file and no socket (`.claude/ai.md` P10). `ai::ProviderProfiles` therefore
// has no `load` and no `save`, which is the one place it departs from
// `io::PrintProfiles`: the application reads the file and hands the text over.
// This service is that application half, in the same shape as `PrintService` and
// for the same reason — `Bus::on_print_request` there,
// `Bus::on_ai_provider_request` here.
//
// FOUR THINGS IT OWNS. The store as it stands; the JSON file under the user's
// configuration directory; the key store the API keys live in, reached through
// `secret_resolver.hpp` so that no caller of `secrets()` can make this thread
// wait (the profile file never holds a key, CLAUDE.md 5.21); and the connection
// PROBE that `YAPAYZEKAMODELİ islem=dene` runs.
//
// THE PROBE ANSWERS LATE, AND THAT IS THE ONLY HONEST SHAPE IT CAN HAVE. A
// command dispatch is synchronous — `Bus::run_to_completion` requires the body to
// finish — while a request to a model endpoint is not, and `.claude/ai.md` R18/P8
// forbid blocking the interface on one: no synchronous HTTP, no waiting on a
// model. So `dene` STARTS the request and says so, and the result — the status,
// the round trip, the refusal — arrives on the transcript through `Bus::echo` when
// the endpoint answers, exactly where the command's own line went. A nested event
// loop would have made the answer immediate by freezing the window, and a probe
// that reported success before the reply arrived would be a probe that lies.
#pragma once

#include "kentos_cad/app/secret_resolver.hpp"

#include "kentos_cad/ai/provider.hpp"
#include "kentos_cad/ai/provider_catalog.hpp"
#include "kentos_cad/ai/transport.hpp"
#include "kentos_cad/command/bus.hpp"

#include <QObject>
#include <QString>
#include <QTimer>

#include <functional>
#include <memory>
#include <string>

namespace kentos::app {

/// Owns the provider profiles, their file, the key store and the probe.
class ProviderService : public QObject
{
    Q_OBJECT

public:
    /// How the owner tells its transport which profile a probe belongs to.
    ///
    /// WHY IT IS NOT DONE HERE. `ai::HttpTransport::send` takes no profile and no
    /// secret — that sans-key signature is what keeps every credential out of
    /// `/src/ai` (`ai/transport.hpp`) — so the concrete transport is told the
    /// profile just before the bytes leave, through `AiTransport::useProfile`.
    /// This service holds the transport through the INTERFACE, so it cannot make
    /// that call itself; the owner passes a binder that can.
    using ProfileBinder = std::function<void(const ai::ProviderProfile&)>;

    /// Installs `Bus::on_ai_provider_request` and loads the profiles from
    /// `ai-modelleri.json` in the user's configuration directory. A missing file
    /// is the built-in set; a file that will not parse is REPORTED by
    /// `announce()` and left untouched on disk. Clears the hook on destruction.
    ///
    /// `transport` may be null — a build or a run with no outbound wire — and
    /// `islem=dene` then says so instead of reporting a test it never made.
    explicit ProviderService(command::Bus& bus, ai::HttpTransport* transport = nullptr,
                             QObject* parent = nullptr);
    ~ProviderService() override;

    ProviderService(const ProviderService&)            = delete;
    ProviderService& operator=(const ProviderService&) = delete;

    /// The profiles as they stand. Read by the settings page, the chat dock and
    /// the transport; written only through the bus.
    const ai::ProviderProfiles& profiles() const noexcept { return profiles_; }

    /// Where the profiles live on disk, for the settings page's note.
    const QString& profilesPath() const noexcept { return path_; }

    /// The shipped vendor catalogue: what this program knows how to reach, and
    /// which models each endpoint serves.
    ///
    /// READ-ONLY AND SHIPPED, unlike `profiles()` which is the user's own list.
    /// It is loaded once from `/data/catalogs/ai/saglayicilar.json`; an absent
    /// or unreadable package leaves it EMPTY and `announce()` says so, because a
    /// stale copy compiled into the binary is exactly what moving it to data was
    /// meant to end (`ai/provider_catalog.hpp`).
    const ai::ProviderCatalog& catalog() const noexcept { return catalog_; }

    /// The key store, for the settings page's secret field and for the
    /// transport. The keys themselves are never held by this service.
    ///
    /// IT IS THE RESOLVER RATHER THAN THE STORE, so that no caller reached
    /// through here can make the GUI thread wait on a platform key store
    /// (secret_resolver.hpp, ai.md R18/P8). `SecretStore::describe` and
    /// `SecretStore::available` are static and still called directly: they ask
    /// the operating system nothing.
    SecretResolver& secrets() noexcept { return secrets_; }

    /// Hands over the outbound transport after construction, with the binder
    /// that names a profile to it. Replaces whatever was there; a null transport
    /// puts the service back into the honest "no wire attached" state.
    void setTransport(ai::HttpTransport* transport, ProfileBinder binder = {});

    /// Says what the CONSTRUCTOR could not — a profile file that would not parse
    /// — once the bus can be heard. The reason this is not in the constructor is
    /// the one `PrintService::announce` gives: the service is built before
    /// `Bus::on_echo` is installed, so a line said there goes nowhere.
    void announce();

signals:
    /// The profile store changed — added, removed, default moved. The settings
    /// page and the chat dock's chooser rebuild from `profiles()`.
    void profilesChanged();

private:
    /// One connection test in flight: the sink that collects the answer and the
    /// handle that cancels it. Defined in the .cpp, because it is the shape of
    /// `ai::StreamSink` and nothing outside needs to know it.
    class Probe;

    command::Task<core::Result<std::string>> handle(command::AiProviderRequest request);

    /// `islem=dene`: checks what can be checked here — the profile exists, the
    /// project's sensitivity permits the endpoint, a credential is findable, a
    /// transport is attached — and starts the request. The line it returns says
    /// the test has STARTED; the outcome is echoed by the probe.
    core::Result<std::string> beginProbe(const ai::ProviderProfile& profile);

    /// Writes the store to `path_`, creating the directory but never a second
    /// file. Called after every accepted change, exactly as `PrintService` does.
    core::Status persist();

    /// Abandons an in-flight probe. A second `dene` replaces the first: one
    /// endpoint is being tested at a time, and a probe left running past its
    /// welcome is a request nobody is waiting for.
    ///
    /// The probe is SHARED rather than owned outright: the transport holds the
    /// sink by reference until the reply ends, so an abandoned probe keeps
    /// itself alive until then (see `Probe::keepAlive` in the .cpp).
    void cancelProbe();

    command::Bus& bus_;
    ai::ProviderCatalog catalog_;
    ai::ProviderProfiles profiles_;
    SecretResolver secrets_;
    ai::HttpTransport* transport_{nullptr};
    ProfileBinder binder_;
    QString path_;
    std::string trouble_; ///< what `announce` has yet to say; empty when all is well

    std::shared_ptr<Probe> probe_;
    QTimer probe_timeout_; ///< an endpoint that never answers still has to be given up on
};

} // namespace kentos::app
