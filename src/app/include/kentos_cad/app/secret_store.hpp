// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — app: where a credential actually lives, and it is not in our files.
//
// THE RULE THIS CLASS EXISTS TO MAKE STRUCTURAL. CLAUDE.md 5.21: an API key, a
// bearer token or a database password may never be in a `SettingSpec` value, a
// command argument, a `Value`, a journal line, a `{kind:"meta"}` record, an audit
// record, a log line, a transcript line or an error message. `.claude/ai.md` P11
// says the same of the audit record and the repository. A provider profile
// therefore carries `key_ref` — the NAME of an entry — and the secret it names is
// held by the operating system, which already has a place for exactly this and
// already asks the user before it hands one over.
//
// TWO ROADS, AND BOTH ARE DELIBERATE.
//
//   1. THE SYSTEM KEY STORE, behind `KENTOS_WITH_KEYCHAIN`: the macOS keychain
//      through the Security framework, the Secret Service through libsecret on
//      Linux, the Windows credential store through wincred. All three are system
//      APIs — nothing is added to `/vcpkg.json` and nothing to `/NOTICE`.
//
//   2. AN ENVIRONMENT VARIABLE, always, on every platform and in every build.
//      `key_ref` may simply name one — `DEEPSEEK_API_KEY` — and it is read at
//      USE and never persisted anywhere by this program. That is the honest
//      answer for a headless machine, a container, a CI runner and a Linux box
//      with no Secret Service running, and it is the same shape the PostGIS path
//      already has with `~/.pgpass`: the credential is somewhere the operating
//      system owns, and this program knows only where to ask.
//
// A BUILD WITH NEITHER SAYS SO. With `KENTOS_WITH_KEYCHAIN=OFF` the store reads
// the environment and stores NOTHING, and `write()` refuses with a sentence
// naming the variable to set rather than pretending a key was saved. A store that
// silently forgot a key would send unauthenticated requests and report the
// provider's 401 as if the endpoint were wrong.
#pragma once

#include "kentos_cad/core/result.hpp"

#include <QString>

#include <optional>

namespace kentos::app {

/// Reads, writes and removes one named secret, through whatever the platform has.
///
/// THE NAME IS THE PROFILE'S `key_ref`, unchanged: a profile, the settings page,
/// the transport and this class all use the one string, so a key written under
/// the name the user typed is the key the request finds.
class SecretStore
{
public:
    /// The key store's service (macOS) / label prefix (Linux, Windows) — one
    /// name, so an entry written by one version is found by the next.
    static constexpr const char* kService = "KentOSCad";

    /// Nothing to build: the platform store is opened per call, which is what
    /// all three system APIs are designed for and what keeps a locked keychain a
    /// per-call refusal rather than a constructor that failed hours earlier.
    SecretStore() = default;

    /// The secret `key_ref` names, or nothing when nobody has one.
    ///
    /// THE ORDER IS FIXED AND IT MATTERS: the system key store first, because
    /// that is what the settings page wrote and what the user typed most
    /// recently; then an environment variable of exactly that name; then the
    /// conventional `<KEY_REF>_API_KEY` (upper case, ASCII), so that the shipped
    /// profiles — whose `key_ref` is `openai`, `deepseek`, `anthropic` — find the
    /// variable the vendor's own documentation tells people to set.
    ///
    /// An empty `key_ref` is a profile that needs no credential (a local Ollama
    /// or llama.cpp) and returns nothing without asking anybody.
    std::optional<QString> read(const QString& key_ref) const;

    /// Writes `secret` under `key_ref`, replacing whatever was there.
    ///
    /// Refuses in a build with no key store, naming the environment variable to
    /// set instead — see the note at the top of this file. The secret is never
    /// echoed back, not even on the failure path: the error names the store and
    /// the entry, never the value.
    core::Status write(const QString& key_ref, const QString& secret);

    /// Removes the entry `key_ref` names. Succeeds when there was nothing to
    /// remove: "the key is not there" is the state the caller asked for, and an
    /// error would make a settings page report a failure for a key the user had
    /// never saved. An environment variable is NOT touched — this program does
    /// not own the caller's environment.
    core::Status erase(const QString& key_ref);

    /// Whether this build can STORE a secret. False means reads still work
    /// through the environment and writes refuse.
    static bool available() noexcept;

    /// Which backend is in use, in Turkish, for the settings page's note and for
    /// `make doctor`: the platform store's name, or the sentence that says only
    /// the environment road is open.
    static QString describe();
};

} // namespace kentos::app
