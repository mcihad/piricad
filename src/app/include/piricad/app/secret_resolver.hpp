// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — app: the key store, asked from a thread that is allowed to wait.
//
// WHY THIS EXISTS, AND IT IS NOT A CACHE FOR SPEED. `SecretStore::read`
// (secret_store.hpp) is a call into the platform's key store, and all three of
// them are allowed to take as long as they like. On macOS `SecItemCopyMatching`
// enters the Security framework and blocks until the user answers an
// authorisation prompt — with nobody at the keyboard it simply sits there, and a
// headless run of this program was measured inside
// `SecurityServer::ClientSession::decrypt` for over two minutes. libsecret's
// `secret_password_lookup_sync` waits on a D-Bus round trip to a keyring that may
// be locked. `CredReadW` is the quick one only because Windows asks nothing.
//
// That call used to be made from `AiTransport::send`, on the GUI thread, BEFORE
// a single byte of the request had left the machine: the window was frozen for
// the whole of it. `.claude/ai.md` R18 requires every AI call to be cancellable
// and the application to stay usable while one is pending, and P8 bans blocking
// waits on the UI thread outright. So the lookup lives here, on a thread of its
// own, and no GUI-thread code path enters the platform store any more.
//
// WHAT IT KEEPS AND WHERE IT KEEPS IT. One resolved secret per `key_ref`, IN
// MEMORY AND FOR THIS SESSION ONLY. Nothing here is written to a file, a
// `SettingSpec`, a command argument, a `Value`, the journal, an audit record, a
// log line or an error message — CLAUDE.md 5.21 lists those and this class adds
// no new place to that list. The memory is what makes the second turn of a
// conversation touch the key store not at all, and it is why a user who declined
// a keychain prompt once is not asked again on every sentence they type.
//
// AND IT IS ASKED AT A MOMENT A PERSON EXPECTS. An authorisation prompt that
// appears when somebody picks a model from the chat dock's chooser is a prompt
// they asked for; the same prompt arriving three sentences later is one they
// cannot explain. `prime()` is that call. `write()` is better still: the secret
// the user has just typed is remembered without ever asking the operating system
// for it back.
//
// ONE LOOKUP AT A TIME, ON ONE THREAD, AND ON PURPOSE. Each of these calls can
// raise a dialog the user has to answer, and two of those at once is a worse
// interruption than one after the other. The cost is that a lookup nobody ever
// answers holds up the next one — which is a real queue and not a hypothetical,
// so the thing it must not do is TRAP anybody: a turn waiting behind it is still
// cancellable, `ai::Cancellation::cancel()` answers its sink on the spot, and the
// window is responsive throughout. That is R18 met, which is more than a second
// thread would buy.
#pragma once

#include "kentos_cad/app/secret_store.hpp"

#include "kentos_cad/core/result.hpp"

#include <QObject>
#include <QString>

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>

class QThread;

namespace kentos::app {

/// Resolves a `key_ref` to the secret it names without ever blocking the caller.
///
/// EVERY PUBLIC MEMBER IS CALLED ON THE GUI THREAD AND NONE OF THEM WAITS. The
/// bookkeeping below (`known_`, `waiting_`, `asking_`) is therefore unguarded by
/// design: it is touched by the GUI thread alone, and the one thing that crosses
/// a thread boundary is the lookup itself, which carries its `key_ref` out and
/// its answer back and shares nothing else.
class SecretResolver : public QObject
{
    Q_OBJECT

public:
    /// What this session already knows about one `key_ref`.
    ///
    /// `settled == false` MEANS "NOBODY HAS ASKED YET", which is not at all the
    /// same as "there is no such key": a caller that treats the two alike reports
    /// a missing credential for a key sitting in the keychain, and sends the user
    /// to a settings page to re-enter something that is already there. Only
    /// `settled` with an empty `secret` means the lookup was made and found
    /// nothing.
    struct Answer
    {
        bool settled{false};             ///< the lookup has been made this session
        std::optional<QString> secret{}; ///< what it found, when it found anything
    };

    /// What a caller is handed when a lookup comes back — on the GUI thread, so
    /// it may touch widgets, and never before `resolve()` has returned.
    using Ready = std::function<void(std::optional<QString>)>;

    /// Starts the worker thread. Nothing is looked up until somebody asks, so
    /// constructing this costs no prompt and no key-store call.
    explicit SecretResolver(QObject* parent = nullptr);

    /// Stops the worker and drops every callback still waiting for an answer.
    ///
    /// A lookup that is still inside the platform call is given a bounded moment
    /// and then LEFT: waiting for an unanswered authorisation prompt would turn a
    /// freeze in the middle of the session into a freeze on the way out of it,
    /// which is the same defect wearing a hat. The thread and its worker delete
    /// themselves when the call finally returns.
    ~SecretResolver() override;

    SecretResolver(const SecretResolver&)            = delete;
    SecretResolver& operator=(const SecretResolver&) = delete;

    /// What is known about `key_ref` right now, without asking anybody and
    /// without waiting. This is what a caller on a path that cannot wait — a
    /// command body, a request about to be sent — tests before it decides.
    Answer known(const QString& key_ref) const;

    /// Looks `key_ref` up off this thread and calls `ready` with the result.
    ///
    /// Answers from memory when the lookup has already been made, in which case
    /// `ready` runs before this returns. Otherwise `ready` is kept until the
    /// answer arrives; a second caller asking for the same `key_ref` JOINS the
    /// first rather than starting a second lookup, because two lookups of one
    /// entry are two authorisation prompts for one question. An empty `key_ref`
    /// is a profile that needs no credential and is answered with nothing.
    void resolve(const QString& key_ref, Ready ready);

    /// Starts the lookup and has nothing to say about the outcome — the call for
    /// a moment the user initiated, so that the prompt lands there rather than
    /// in the middle of their next sentence.
    void prime(const QString& key_ref) { resolve(key_ref, Ready()); }

    /// Writes `secret` under `key_ref` through `SecretStore::write`, and on
    /// success remembers it for this session.
    ///
    /// REMEMBERING IS THE POINT, not an optimisation: the user has just typed the
    /// key, so reading it straight back out of the operating system would be a
    /// prompt asked for an answer we are holding.
    core::Status write(const QString& key_ref, const QString& secret);

    /// Removes the entry `key_ref` names through `SecretStore::erase`, and drops
    /// what this session knew about it.
    ///
    /// DROPS RATHER THAN RECORDS "NOTHING": `SecretStore::erase` removes the
    /// platform entry and deliberately leaves the environment alone, so a
    /// `key_ref` that also names an exported variable still resolves — and an
    /// entry recorded as absent here would hide that variable for the rest of the
    /// session.
    core::Status erase(const QString& key_ref);

private:
    /// What the lookup and this object share, and the only thing they do.
    ///
    /// The lookup runs on another thread and must hand its answer back to an
    /// object that may be gone by then. It therefore holds this by `shared_ptr`
    /// and takes the lock before it looks at `owner`, while the destructor clears
    /// `owner` under the same lock — so the hop back either happens against a
    /// live object or does not happen at all. Defined in the .cpp; nothing
    /// outside needs its shape.
    struct Mailbox;

    /// Records one lookup's answer and hands it to everybody waiting on it.
    /// Runs on the GUI thread, posted by the worker.
    void deliver(const QString& key_ref, std::optional<QString> secret);

    /// The synchronous platform door, used here only by `write` and `erase` —
    /// both of which happen because a person pressed a button saying so, and
    /// both of which the user expects to be asked about. The lookup builds its
    /// own, so that nothing in this class depends on `SecretStore` being
    /// stateless.
    SecretStore store_;

    /// Shared with every lookup in flight; see `Mailbox`.
    std::shared_ptr<Mailbox> box_;

    /// The lookup thread and the object that lives on it. Neither is owned: both
    /// delete themselves once the thread finishes, which is what lets the
    /// destructor stop waiting without leaving a dangling pointer behind.
    QThread* thread_{nullptr};
    QObject* worker_{nullptr}; ///< the hop ONTO the lookup thread

    /// Every `key_ref` this session has an answer for, and what it was. Presence
    /// in the map IS `Answer::settled`; an entry holding nothing means the lookup
    /// was made and found no key.
    std::map<QString, std::optional<QString>> known_;

    /// Callers waiting on a lookup that has not come back yet.
    std::multimap<QString, Ready> waiting_;

    /// The lookups that are out, so a second asker joins the first instead of
    /// raising a second prompt.
    std::set<QString> asking_;
};

} // namespace kentos::app
