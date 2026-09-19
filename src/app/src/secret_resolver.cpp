// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/app/secret_resolver.hpp"

#include <QThread>

#include <mutex>
#include <utility>
#include <vector>

namespace kentos::app {
namespace {

/// How long the destructor gives a lookup to come back before it stops waiting.
///
/// Long enough for a keyring that is merely slow, short enough that quitting the
/// program never feels like it hung. A lookup still sitting on an unanswered
/// authorisation prompt is not waited for at all — see the destructor.
constexpr int kQuitWaitMs = 1500;

} // namespace

/// The handshake between a lookup and the object that asked for it.
struct SecretResolver::Mailbox
{
    std::mutex lock;                ///< held across the hop back, and by the destructor
    SecretResolver* owner{nullptr}; ///< cleared when the resolver goes away
};

SecretResolver::SecretResolver(QObject* parent) : QObject(parent), box_(std::make_shared<Mailbox>())
{
    box_->owner = this;

    // THE THREAD OWNS ITSELF, and that is what makes the destructor's bounded
    // wait safe. If a lookup is still inside the platform call when this object
    // goes away, nothing here can join it; instead the thread finishes on its own
    // whenever the call returns, and the two objects delete themselves then. A
    // process that exits before that happens leaks two objects, which is the
    // right way round for the trade.
    thread_ = new QThread;
    thread_->setObjectName(QStringLiteral("kentos-keystore"));
    worker_ = new QObject;
    worker_->moveToThread(thread_);
    connect(thread_, &QThread::finished, worker_, &QObject::deleteLater);
    connect(thread_, &QThread::finished, thread_, &QObject::deleteLater);
    thread_->start();
}

SecretResolver::~SecretResolver()
{
    {
        const std::lock_guard<std::mutex> held(box_->lock);
        box_->owner = nullptr;
    }

    // NOBODY IS ANSWERED FROM HERE ON. A caller still waiting is waiting on a
    // turn that is being torn down with the rest of the window; calling back into
    // it would be calling into half-destroyed widgets.
    waiting_.clear();

    thread_->quit();
    thread_->wait(kQuitWaitMs);
}

SecretResolver::Answer SecretResolver::known(const QString& key_ref) const
{
    const auto settled = known_.find(key_ref);
    if (settled == known_.end()) return {};
    return {true, settled->second};
}

void SecretResolver::resolve(const QString& key_ref, Ready ready)
{
    // A LOCAL MODEL HAS NO KEY, and that is the common case rather than an edge
    // one: Ollama and llama.cpp need no credential, so their profiles carry no
    // `key_ref` and nothing is looked up for them.
    if (key_ref.isEmpty()) {
        if (ready) ready(std::nullopt);
        return;
    }

    const auto settled = known_.find(key_ref);
    if (settled != known_.end()) {
        if (ready) ready(settled->second);
        return;
    }

    if (ready) waiting_.emplace(key_ref, std::move(ready));

    // A SECOND ASKER JOINS THE FIRST. Two `SecItemCopyMatching` calls for one
    // entry are two authorisation prompts for one question, and the second one
    // arrives while the user is still reading the first.
    if (!asking_.insert(key_ref).second) return;

    QMetaObject::invokeMethod(
        worker_,
        [box = box_, key_ref] {
            // THE ONE CALL IN THIS PROGRAM THAT IS ALLOWED TO WAIT. Everything
            // this header says about blocking is about this line.
            const SecretStore store;
            std::optional<QString> found = store.read(key_ref);

            const std::lock_guard<std::mutex> held(box->lock);
            if (box->owner == nullptr) return; // the window closed while we waited
            QMetaObject::invokeMethod(
                box->owner,
                [owner = box->owner, key_ref, found] { owner->deliver(key_ref, found); },
                Qt::QueuedConnection);
        },
        Qt::QueuedConnection);
}

void SecretResolver::deliver(const QString& key_ref, std::optional<QString> secret)
{
    asking_.erase(key_ref);

    // A LOOKUP THAT COMES BACK LATE DOES NOT OVERWRITE A NEWER ANSWER. `write()`
    // settles a key the moment the user saves one, and a read that was already on
    // its way when they pressed the button is older than what they typed.
    const auto settled                  = known_.emplace(key_ref, std::move(secret)).first;
    const std::optional<QString> answer = settled->second;

    // TAKEN OUT OF THE MAP BEFORE ANYBODY IS CALLED. A callback is free to ask
    // for another key, and answering out of a container that is being walked is
    // how that freedom becomes a crash.
    std::vector<Ready> answering;
    const auto range = waiting_.equal_range(key_ref);
    for (auto it = range.first; it != range.second; ++it)
        answering.push_back(std::move(it->second));
    waiting_.erase(range.first, range.second);

    for (const Ready& ready : answering)
        if (ready) ready(answer);
}

core::Status SecretResolver::write(const QString& key_ref, const QString& secret)
{
    const core::Status wrote = store_.write(key_ref, secret);
    if (!wrote) return wrote;

    // AN EMPTY SECRET IS AN ERASE — `SecretStore::write` says so itself — so what
    // is known is dropped rather than recorded, for the reason `erase()` gives.
    if (secret.isEmpty())
        known_.erase(key_ref);
    else
        known_.insert_or_assign(key_ref, secret);
    return wrote;
}

core::Status SecretResolver::erase(const QString& key_ref)
{
    const core::Status gone = store_.erase(key_ref);
    if (!gone) return gone;
    known_.erase(key_ref);
    return gone;
}

} // namespace kentos::app
