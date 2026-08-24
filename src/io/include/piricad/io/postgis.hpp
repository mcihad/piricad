// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — io: the PostGIS store.
//
// CLAUDE.md Article 2.9: **PostGIS is a first-class store, not an export target.**
// Turkish municipalities and TKGM run their corporate data on it, and a program
// that can only read a dump cannot sit inside that workflow.
//
// TWO THINGS ARE SAVED AND THEY ARE NOT THE SAME THING.
//
//   A LAYER becomes a real table — one row per entity, a `geometry` column in the
//   document's own SRID, and one column per declared attribute. QGIS, ogr2ogr and
//   a `SELECT` all read it, because it is an ordinary spatial table and not a
//   private encoding. That is what "save it as a GIS layer" has to mean.
//
//   A PROJECT is stored WHOLE, as the bytes of its `.pcad` file. A drawing is more
//   than its geometry: it carries its style table, its embedded gösterim pictures,
//   its layer tree, its settings and its CRS, and decomposing all of that into
//   tables would be inventing a second file format whose round trip nobody
//   checks. The native format already round-trips exactly and is checked by the
//   fingerprint on every open.
//
// WHY libpqxx AND NOT GDAL'S PG DRIVER. Article 2.9 states it: the driver cannot
// express a transaction that spans a command, and Article 1.6 requires an ifraz to
// roll back whole. A half-written parcelation in a municipality's live database is
// exactly the failure this project is built to make impossible.
//
// WHY NOT A HAND-ROLLED libpq WRAPPER (Article 2.7): what libpqxx does is escaping,
// binary parameters, notice handling and transaction lifetime — every one of them
// a place a hand-rolled version leaks a connection or lets a quote through.
#pragma once

#include "piricad/core/document.hpp"
#include "piricad/core/result.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace piricad::io {

/// One entity's geometry as EWKB in `srid`, or empty when it has none to write.
///
/// PUBLIC, and not because a caller outside this module wants it. This is the
/// piece that HAS to be exactly right — a parcel written with the wrong shape is
/// a wrong area on a legal document — and checking it must not require a running
/// PostgreSQL. It is pure arithmetic over the document's rings, so it compiles
/// and is tested in EVERY build, including one configured without PostGIS.
///
/// EWKB is PostGIS's extension of OGC WKB: the type word carries `0x20000000`
/// and the SRID follows it, because a `geometry(Geometry, 5254)` column rejects
/// a plain WKB geometry as SRID 0 and the COPY path has no `ST_SetSRID` to wrap
/// it in.
///
/// The shape follows the ring roles, and the multipart case is the one that
/// matters: an entity with two `Exterior` rings is a parcel with two FACES — one
/// cut in two by a road — and it becomes a MULTIPOLYGON. Folding its second face
/// in as a hole would hand a municipality a table whose areas are wrong and whose
/// geometry still passes every check that does not know what it should have been.
std::string entity_ewkb(const core::Document& doc, core::EntityId entity, std::int64_t srid);

/// One table the store found, as a person needs to see it.
struct PostgisTable
{
    std::string schema;   ///< `public`, or whatever the connection uses
    std::string name;     ///< the table
    std::string geometry; ///< the geometry column's name, empty when it has none
    std::int64_t srid{0}; ///< the SRID the geometry column declares
    std::int64_t rows{0}; ///< an estimate, from the planner rather than a COUNT
};

/// One stored project, as the catalogue table records it.
struct PostgisProject
{
    std::string name;      ///< what it was saved under
    std::string updated;   ///< ISO-8601, as the server formatted it
    std::int64_t bytes{0}; ///< the size of the stored `.pcad`
};

/// A live connection to a PostGIS database.
///
/// Owns the connection for as long as it lives and closes it on destruction. NOT
/// copyable and NOT shared: two documents open at once must not be able to write
/// through one connection and interleave their transactions.
class PostgisStore
{
public:
    /// Opens a connection.
    ///
    /// `conninfo` is a libpq connection string — `host=... dbname=... user=...` or
    /// a `postgresql://` URI. Every failure comes back as an `Error` carrying what
    /// the server said, because a connection that fails silently is a connection
    /// the user will blame the drawing for.
    static core::Result<std::unique_ptr<PostgisStore>> open(const std::string& conninfo);

    ~PostgisStore();

    PostgisStore(const PostgisStore&)            = delete;
    PostgisStore& operator=(const PostgisStore&) = delete;

    /// What the server says it is, for a transcript line the user can check.
    const std::string& server_version() const noexcept { return server_; }

    /// The PostGIS version, or empty when the extension is not installed.
    const std::string& postgis_version() const noexcept { return postgis_; }

    /// Spatial tables visible on this connection, from `geometry_columns`.
    core::Result<std::vector<PostgisTable>> tables();

    /// Writes one layer as a spatial table, replacing what is there.
    ///
    /// REPLACES rather than appends, and says so in its name: appending would make
    /// running the command twice produce two copies of every parcel, and a
    /// cadastral table with doubled rows is worse than no table.
    ///
    /// One transaction for the whole layer (Article 1.6): a write that failed
    /// half way would leave a municipality's table holding part of a drawing.
    core::Result<std::size_t> write_layer(const core::Document& doc, core::LayerId layer,
                                          const std::string& table);

    /// Stores a project's bytes under a name, replacing an earlier one.
    ///
    /// `bytes` is a complete `.pcad` file. The catalogue table is created on first
    /// use, so a fresh database needs no setup step.
    core::Status write_project(const std::string& name, const std::vector<std::byte>& bytes);

    /// Reads a stored project's bytes back.
    core::Result<std::vector<std::byte>> read_project(const std::string& name);

    /// Every stored project, newest first.
    core::Result<std::vector<PostgisProject>> projects();

    /// Removes one stored project.
    ///
    /// Touches ONLY PiriCAD's own catalogue table. There is deliberately no verb
    /// anywhere in this class that drops an arbitrary table: a program holding a
    /// municipality's live connection must not be one typo away from deleting
    /// their cadastre, and a GIS user already has tools for that.
    ///
    /// Reports how many rows went, so a caller can tell "deleted" from "there was
    /// nothing by that name" instead of guessing.
    core::Result<std::size_t> delete_project(const std::string& name);

private:
    struct Impl;

    explicit PostgisStore(std::unique_ptr<Impl> impl);

    std::unique_ptr<Impl> impl_;
    std::string server_;
    std::string postgis_;
};

} // namespace piricad::io
