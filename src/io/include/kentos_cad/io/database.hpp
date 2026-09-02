// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — io: the database engine, plugged into the command bus.
//
// The same seam `FileService` uses, for the same reason: Article 3.2 makes
// `io -> command` one-way, but the registry that generates the CLI help, the AI
// schema and `docs/komutlar/referans.md` lives in /src/command. So the
// `CommandSpec` and the body of `VERİTABANI` live in
// /src/command/src/commands/database.cpp, and the actual work arrives here
// through `Bus::on_database_request`, which this class installs.
//
// A client that never installs a DatabaseService — or a build configured without
// `KENTOS_WITH_POSTGIS` — gets a clear "Veritabanı motoru bağlı değil." from the
// command rather than a crash. That is deliberate: this header compiles and this
// class constructs in BOTH configurations, so nothing downstream needs an `#ifdef`
// around its existence (build.md's rule about optional dependencies staying
// invisible to their callers).
#pragma once

#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/core/result.hpp"

#include <memory>
#include <string>

namespace kentos::io {

/// Owns one database connection for exactly one `Bus`, and therefore for exactly
/// one document.
///
/// Deliberately not a singleton: two documents open in one process must not share
/// a connection, because they would interleave their transactions and Article 1.6
/// would stop meaning anything.
class DatabaseService
{
public:
    /// Installs `Bus::on_database_request` and clears it on destruction.
    explicit DatabaseService(command::Bus& bus);
    ~DatabaseService();

    /// Non-copyable: it owns the bus hook, and two services would fight over it.
    DatabaseService(const DatabaseService&)            = delete;
    DatabaseService& operator=(const DatabaseService&) = delete;

    /// Whether this build can talk to a database at all. False means PostGIS was
    /// not compiled in, and every operation will say so rather than fail obscurely.
    static bool available() noexcept;

    /// Whether a connection is open right now. For a status bar and for tests.
    bool connected() const noexcept;

    /// The connection string in use, with its password redacted. Never the raw
    /// one: this is read by the GUI and could end up in a screenshot.
    const std::string& target() const noexcept { return target_; }

private:
    /// Taken BY VALUE for the reason `FileService::handle` documents: a coroutine
    /// does not copy its reference parameters into its frame, and this is called
    /// through a `std::function` whose argument is a temporary at every call site.
    command::Task<core::Result<std::string>> handle(command::DatabaseRequest request);

    struct Impl;

    command::Bus& bus_;
    std::unique_ptr<Impl> impl_;
    std::string target_;
};

} // namespace kentos::io
