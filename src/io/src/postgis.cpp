// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/io/postgis.hpp"

#include "kentos_cad/core/attribute.hpp"
#include "kentos_cad/core/text.hpp"

// The connection half of this file needs libpqxx; the ENCODING half does not.
// `entity_ewkb` is pure arithmetic over the document's rings, so it is built and
// tested in every configuration — including one where PostGIS is off and the
// store below reports that plainly instead of failing to compile. This is the
// same shape `vector.cpp` uses for KENTOS_HAVE_GDAL.
#if KENTOS_HAVE_POSTGIS
#include <pqxx/pqxx>
#endif

#include <algorithm>
#include <array>
#include <cstring>
#include <optional>
#include <utility>
#include <vector>

namespace kentos::io {
namespace {

using core::ErrorCode;

/// The table a project's bytes are stored in.
///
/// Created on first write so a fresh database needs no setup step. The name is
/// prefixed because this table sits in somebody else's schema beside their own
/// cadastral tables, and a bare `projeler` would be a name collision waiting for
/// the first municipality that already has one.
constexpr const char* kProjectTable = "kentos_projeler";

/// Millimetres to the CRS's own unit.
///
/// THE ONE PLACE `Mm` BECOMES A DOUBLE on this path, and it is an export boundary
/// exactly like DXF or GeoPackage: PostGIS stores coordinates as doubles and no
/// wrapper changes that. Article 2.4 makes `double` a transient local rather than
/// a stored format, and here the stored format belongs to somebody else.
///
/// A TUREF easting is about 4.5e8 millimetres, which a double holds exactly —
/// every integer below 2^53 does — so nothing is lost on the way out.
double to_crs_units(core::Mm value)
{
    return static_cast<double>(value) / 1000.0;
}

/// Appends a little-endian value to a WKB buffer.
template<class T> void put(std::string& wkb, T value)
{
    static_assert(std::is_trivially_copyable_v<T>);
    char bytes[sizeof(T)];
    std::memcpy(bytes, &value, sizeof(T));
    wkb.append(bytes, sizeof(T));
}

#if KENTOS_HAVE_POSTGIS

/// The hex form of a WKB buffer, which is what a COPY into a geometry column
/// takes: PostGIS's own text input for `geometry` is hex EWKB.
std::string to_hex(const std::string& wkb)
{
    static constexpr std::array<char, 16> kDigits{'0', '1', '2', '3', '4', '5', '6', '7',
                                                  '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    std::string out;
    out.reserve(wkb.size() * 2);
    for (const char c : wkb) {
        const auto byte = static_cast<unsigned char>(c);
        out += kDigits[byte >> 4];
        out += kDigits[byte & 0x0F];
    }
    return out;
}

#endif // KENTOS_HAVE_POSTGIS

/// One entity's geometry as EWKB, or empty when it has none worth writing.
///
/// WKB rather than WKT, and the reason is the red line about performance: a
/// five-million-parcel layer written as text is four times the bytes and a parse
/// on the server for every one of them. This is the same encoding PostGIS stores
/// internally, so the server copies it rather than reading it.
///
/// EWKB — PostGIS's extension, which is the plain OGC form with `0x20000000` set
/// on the type word and the SRID written straight after it. Plain WKB carries no
/// SRID, so a `geometry(Geometry, 5254)` column rejects it as SRID 0; the COPY
/// path has no `ST_SetSRID` call to wrap it in, so the geometry has to say what it
/// is by itself. Only the OUTER geometry carries the SRID; the members of a
/// collection do not, which is what PostGIS itself writes.
///
/// WHAT THE FOUR SHAPES ARE AND WHY THE COUNT MATTERS. An entity's rings are a
/// FLAT list, and their roles are what group them: an `Exterior` opens a part and
/// every `Interior` after it is a hole in THAT part. So:
///
///   one exterior            -> POLYGON
///   several exteriors       -> MULTIPOLYGON, one part per exterior
///   one open ring           -> LINESTRING
///   several open rings      -> MULTILINESTRING
///
/// The multipart case is not a curiosity. A parcel cut in two by a road is one
/// parsel with two faces, and writing its second face as a HOLE in its first
/// would hand a municipality a table whose areas are wrong and whose geometry is
/// still valid enough that nothing complains.
std::string encode_ewkb(const core::Document& doc, core::EntityId e, std::int64_t srid)
{
    const core::RingGeometry& geometry = doc.geometry();
    const core::RingSpan span          = geometry.rings_of(doc.entities().slot[e]);
    if (span.count == 0) return {};

    const bool closed = geometry.ring_role[span.first] != core::RingRole::Open;

    // The SRID rides in the type word's high bits; see the note above.
    constexpr std::uint32_t kHasSrid = 0x20000000u;
    constexpr char kLittleEndian     = '\x01'; // every platform this ships on

    /// Writes one ring's vertices. A ring is stored WITHOUT its closing vertex
    /// and a WKB polygon ring requires one, so a closed ring's count is one more
    /// than the document holds and its first point is written again at the end.
    const auto put_ring = [&](std::string& out, std::uint32_t r, bool close_it) {
        const auto xs = geometry.ring_xs(r);
        const auto ys = geometry.ring_ys(r);

        put<std::uint32_t>(out, static_cast<std::uint32_t>(xs.size() + (close_it ? 1 : 0)));
        for (std::size_t v = 0; v < xs.size(); ++v) {
            put<double>(out, to_crs_units(xs[v]));
            put<double>(out, to_crs_units(ys[v]));
        }
        if (close_it) {
            put<double>(out, to_crs_units(xs.front()));
            put<double>(out, to_crs_units(ys.front()));
        }
    };

    std::string wkb;

    if (closed) {
        // Where each part starts. A part opens at an `Exterior` and runs until the
        // next one; everything between is a hole in it.
        std::vector<std::uint32_t> parts;
        for (std::uint32_t r = span.first; r < span.first + span.count; ++r)
            if (geometry.ring_role[r] == core::RingRole::Exterior) parts.push_back(r);

        // No exterior at all is a malformed face, not an empty one. Writing it as
        // a polygon whose first hole is its boundary would be worse than skipping
        // it, and skipping is what an unwritable geometry has always meant here.
        if (parts.empty()) return {};

        /// One part as a bare Polygon body, with no SRID of its own.
        const auto put_polygon = [&](std::string& out, std::size_t part) {
            const std::uint32_t first = parts[part];
            const std::uint32_t last =
                part + 1 < parts.size() ? parts[part + 1] : span.first + span.count;

            put<std::uint32_t>(out, last - first); // rings in this part
            for (std::uint32_t r = first; r < last; ++r)
                put_ring(out, r, /*close_it=*/true);
        };

        if (parts.size() == 1) {
            wkb += kLittleEndian;
            put<std::uint32_t>(wkb, 3u | kHasSrid); // Polygon
            put<std::uint32_t>(wkb, static_cast<std::uint32_t>(srid));
            put_polygon(wkb, 0);
            return wkb;
        }

        wkb += kLittleEndian;
        put<std::uint32_t>(wkb, 6u | kHasSrid); // MultiPolygon
        put<std::uint32_t>(wkb, static_cast<std::uint32_t>(srid));
        put<std::uint32_t>(wkb, static_cast<std::uint32_t>(parts.size()));

        for (std::size_t part = 0; part < parts.size(); ++part) {
            // Each member repeats the byte order and its own type, and does NOT
            // repeat the SRID.
            wkb += kLittleEndian;
            put<std::uint32_t>(wkb, 3u); // Polygon
            put_polygon(wkb, part);
        }
        return wkb;
    }

    // ---- open rings ----
    //
    // A run of two vertices is the smallest line there is; anything shorter is a
    // point that was drawn as a line and is not written.
    std::vector<std::uint32_t> lines;
    for (std::uint32_t r = span.first; r < span.first + span.count; ++r)
        if (geometry.ring_xs(r).size() >= 2) lines.push_back(r);

    if (lines.empty()) return {};

    if (lines.size() == 1) {
        wkb += kLittleEndian;
        put<std::uint32_t>(wkb, 2u | kHasSrid); // LineString
        put<std::uint32_t>(wkb, static_cast<std::uint32_t>(srid));
        put_ring(wkb, lines.front(), /*close_it=*/false);
        return wkb;
    }

    wkb += kLittleEndian;
    put<std::uint32_t>(wkb, 5u | kHasSrid); // MultiLineString
    put<std::uint32_t>(wkb, static_cast<std::uint32_t>(srid));
    put<std::uint32_t>(wkb, static_cast<std::uint32_t>(lines.size()));

    for (const std::uint32_t r : lines) {
        wkb += kLittleEndian;
        put<std::uint32_t>(wkb, 2u); // LineString
        put_ring(wkb, r, /*close_it=*/false);
    }
    return wkb;
}

#if KENTOS_HAVE_POSTGIS

/// A safe SQL identifier built from a layer or column name.
///
/// Turkish-folded to ASCII and reduced to `[a-z0-9_]`, because a PostgreSQL
/// identifier that needs quoting is one every later hand-written query has to
/// remember to quote. `İMAR PLANI` becomes `imar_plani`, which is what a GIS user
/// expects to type.
///
/// The result is never interpolated blind: it is checked against this alphabet and
/// the callers quote it anyway. An identifier cannot be a bound parameter in
/// PostgreSQL, so the defence has to be the alphabet.
std::string as_identifier(std::string_view name)
{
    static constexpr std::array<std::pair<const char*, char>, 12> kFold{{{"ı", 'i'},
                                                                         {"İ", 'i'},
                                                                         {"ş", 's'},
                                                                         {"Ş", 's'},
                                                                         {"ğ", 'g'},
                                                                         {"Ğ", 'g'},
                                                                         {"ü", 'u'},
                                                                         {"Ü", 'u'},
                                                                         {"ö", 'o'},
                                                                         {"Ö", 'o'},
                                                                         {"ç", 'c'},
                                                                         {"Ç", 'c'}}};

    std::string out;
    for (std::size_t i = 0; i < name.size();) {
        bool folded = false;
        for (const auto& [utf8, ascii] : kFold) {
            const std::size_t length = std::strlen(utf8);
            if (name.compare(i, length, utf8) == 0) {
                out += ascii;
                i += length;
                folded = true;
                break;
            }
        }
        if (folded) continue;

        const char c = name[i++];
        if (c >= 'A' && c <= 'Z')
            out += static_cast<char>(c - 'A' + 'a');
        else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
            out += c;
        else
            out += '_';
    }

    // A leading digit is not a legal identifier, and an empty one is not a name.
    if (out.empty()) out = "katman";
    if (out.front() >= '0' && out.front() <= '9') out.insert(out.begin(), 'k');
    return out;
}

/// The PostgreSQL type a declared attribute column maps to.
const char* sql_type(core::AttrType type)
{
    switch (type) {
    case core::AttrType::Int64: return "bigint";
    case core::AttrType::Length: return "double precision"; // stored mm, written in CRS units
    case core::AttrType::Bool: return "boolean";
    case core::AttrType::Text:
    case core::AttrType::CodeRef: return "text";
    }
    return "text";
}

/// The SRID a document's CRS names, or 0 when nothing resolved it.
///
/// Zero is written as SRID 0, which is PostGIS's own "unknown". NOT guessed at:
/// a spatial table labelled with the wrong SRID moves every coordinate in it by
/// kilometres and does so silently, because the numbers still look like Turkish
/// coordinates (model.md R36).
std::int64_t srid_of(const core::Document& doc)
{
    return doc.crs().epsg();
}

#endif // KENTOS_HAVE_POSTGIS

} // namespace

std::string entity_ewkb(const core::Document& doc, core::EntityId entity, std::int64_t srid)
{
    return encode_ewkb(doc, entity, srid);
}

#if !KENTOS_HAVE_POSTGIS

// ---------------------------------------------------------------------------
// Built WITHOUT libpqxx. The type still exists, so nothing downstream needs an
// `#ifdef` around its declaration; every entry point says the same sentence.
// `io::DatabaseService` answers before any of these is reached, so in practice
// this is the belt to that braces.
// ---------------------------------------------------------------------------

struct PostgisStore::Impl
{};

PostgisStore::PostgisStore(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}

PostgisStore::~PostgisStore() = default;

namespace {

core::Error postgis_off()
{
    return core::Error{ErrorCode::Unsupported,
                       "Bu KentOSCad yapısı PostgreSQL desteği olmadan derlenmiş. "
                       "Kaynaktan derliyorsanız KENTOS_WITH_POSTGIS=ON ile yapılandırın."};
}

} // namespace

core::Result<std::unique_ptr<PostgisStore>> PostgisStore::open(const std::string&)
{
    return postgis_off();
}

core::Result<std::vector<PostgisTable>> PostgisStore::tables()
{
    return postgis_off();
}

core::Result<std::size_t> PostgisStore::write_layer(const core::Document&, core::LayerId,
                                                    const std::string&)
{
    return postgis_off();
}

core::Status PostgisStore::write_project(const std::string&, const std::vector<std::byte>&)
{
    return postgis_off();
}

core::Result<std::vector<std::byte>> PostgisStore::read_project(const std::string&)
{
    return postgis_off();
}

core::Result<std::vector<PostgisProject>> PostgisStore::projects()
{
    return postgis_off();
}

core::Result<std::size_t> PostgisStore::delete_project(const std::string&)
{
    return postgis_off();
}

#else

/// The connection, kept out of the header so libpqxx stays inside this file.
struct PostgisStore::Impl
{
    explicit Impl(const std::string& conninfo) : connection(conninfo) {}

    pqxx::connection connection;
};

PostgisStore::PostgisStore(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}

PostgisStore::~PostgisStore() = default;

core::Result<std::unique_ptr<PostgisStore>> PostgisStore::open(const std::string& conninfo)
{
    if (conninfo.empty())
        return core::err(ErrorCode::InvalidArgument,
                         "Veritabanı bağlantı dizesi boş. Örnek: "
                         "host=localhost dbname=postgres user=postgres password=...");

    try {
        auto impl = std::make_unique<Impl>(conninfo);

        std::unique_ptr<PostgisStore> store(new PostgisStore(std::move(impl)));

        pqxx::work tx(store->impl_->connection);
        store->server_ = tx.query_value<std::string>("select version()");

        // A database without the extension is a database this program cannot use,
        // and saying so now is better than failing on the first geometry column.
        const auto found = tx.query01<std::string>(
            "select extversion from pg_extension where extname = 'postgis'");
        if (!found)
            return core::err(ErrorCode::ValidationFailed,
                             "Bu veritabanında PostGIS eklentisi yok. Kurmak için: "
                             "CREATE EXTENSION postgis;");
        store->postgis_ = std::get<0>(*found);
        tx.commit();

        return store;
    } catch (const std::exception& e) {
        return core::err(ErrorCode::NotFound,
                         std::string("Veritabanına bağlanılamadı: ") + e.what());
    }
}

core::Result<std::vector<PostgisTable>> PostgisStore::tables()
{
    try {
        pqxx::work tx(impl_->connection);

        std::vector<PostgisTable> out;
        // From `geometry_columns`, which is PostGIS's own view of what is spatial.
        // Listing every table would list the sequences and the catalogue too.
        for (const auto& row : tx.query<std::string, std::string, std::string, int, double>(
                 "select g.f_table_schema, g.f_table_name, g.f_geometry_column, g.srid, "
                 "       coalesce(c.reltuples, 0)::float8 "
                 "  from geometry_columns g "
                 "  left join pg_class c on c.relname = g.f_table_name "
                 " order by g.f_table_schema, g.f_table_name")) {
            PostgisTable table;
            table.schema   = std::get<0>(row);
            table.name     = std::get<1>(row);
            table.geometry = std::get<2>(row);
            table.srid     = std::get<3>(row);
            table.rows     = static_cast<std::int64_t>(std::get<4>(row));
            out.push_back(std::move(table));
        }
        tx.commit();
        return out;
    } catch (const std::exception& e) {
        return core::err(ErrorCode::Internal, std::string("Tablolar okunamadı: ") + e.what());
    }
}

core::Result<std::size_t> PostgisStore::write_layer(const core::Document& doc, core::LayerId layer,
                                                    const std::string& table)
{
    const core::Layer* record = doc.layer(layer);
    if (record == nullptr)
        return core::err(ErrorCode::NotFound, "Katman bulunamadı: " + std::to_string(layer) + ".");

    const std::string name  = as_identifier(table.empty() ? record->name : table);
    const std::int64_t srid = srid_of(doc);

    try {
        pqxx::work tx(impl_->connection);

        // ONE transaction for the drop, the create and every row. A write that
        // failed half way would leave a municipality's table holding part of a
        // drawing, which Article 1.6 refuses.
        tx.exec("drop table if exists " + tx.quote_name(name));

        std::string create = "create table " + tx.quote_name(name) +
                             " (kimlik bigint primary key, geom geometry(Geometry, " +
                             std::to_string(srid) + ")";

        // One column per DECLARED attribute, in declaration order, so the table a
        // GIS user opens has the drawing's own schema rather than a blob.
        const core::AttrTable& attributes = doc.attributes();

        std::vector<core::AttrId> sources;
        std::vector<std::string> headings{"kimlik", "geom"};

        for (std::size_t i = 0; i < attributes.columns(); ++i) {
            const auto id                  = static_cast<core::AttrId>(i);
            const core::AttrColumn* column = attributes.column(id);
            if (column == nullptr) continue;

            const core::AttrSpec& spec = column->spec();
            const std::string heading  = as_identifier(spec.id);

            // A clash with the two columns this table always has, or with an
            // earlier attribute that folded to the same ASCII identifier. The
            // server would refuse the CREATE anyway; saying it here names the
            // attribute the user has to rename instead of the SQL that broke.
            if (std::find(headings.begin(), headings.end(), heading) != headings.end())
                return core::err(ErrorCode::ValidationFailed,
                                 "'" + spec.id + "' özniteliği tabloda '" + heading +
                                     "' sütunu olurdu; bu ad zaten kullanılıyor. Özniteliği "
                                     "yeniden adlandırın.");

            sources.push_back(id);
            headings.push_back(heading);
            create += ", " + tx.quote_name(heading) + " " + sql_type(spec.type);
        }
        create += ")";
        tx.exec(create);

        // COPY, NOT `insert`. This is the performance red line showing up in a
        // place it is easy to miss: a per-row `insert` is a round trip and a plan
        // lookup for every parcel, and on a cadastral layer that is the difference
        // between seconds and a coffee break. `stream_to` is libpqxx's COPY writer,
        // it lives inside the transaction above, and a rejected row aborts the
        // whole stream — which is the behaviour Article 1.6 wants anyway.
        std::size_t written  = 0;
        const auto& entities = doc.entities();

        {
            pqxx::stream_to stream = pqxx::stream_to::table(tx, {name}, headings);

            // One row buffer, refilled rather than rebuilt: the strings keep their
            // capacity across five million entities instead of allocating per cell.
            std::vector<std::optional<std::string>> row(headings.size());

            for (core::EntityId e = 0; e < entities.size(); ++e) {
                if (!entities.alive(e) || entities.layer[e] != layer) continue;

                const std::string wkb = entity_ewkb(doc, e, srid);
                if (wkb.empty()) continue;

                // The PERSISTENT key, not the slot. A slot is an allocation detail
                // and reusing one across a save would make the database point at a
                // different parcel than the drawing does (model.md R1-R5).
                row[0] = pqxx::to_string(static_cast<std::int64_t>(core::raw(entities.key[e])));
                row[1] = to_hex(wkb);

                for (std::size_t i = 0; i < sources.size(); ++i) {
                    std::optional<std::string>& cell = row[i + 2];

                    auto value = doc.attribute(sources[i], e);
                    if (!value) return value.error();

                    // An ABSENT cell is written as SQL null and not as a zero: an
                    // unmeasured frontage and a zero frontage are different facts
                    // about a parcel, and a database is where that difference gets
                    // queried.
                    if (!value.value().present) {
                        cell.reset();
                        continue;
                    }

                    switch (value.value().type) {
                    case core::AttrType::Int64: cell = pqxx::to_string(value.value().number); break;
                    case core::AttrType::Length:
                        cell = pqxx::to_string(to_crs_units(value.value().number));
                        break;
                    case core::AttrType::Bool:
                        cell = value.value().number != 0 ? "true" : "false";
                        break;
                    case core::AttrType::Text:
                    case core::AttrType::CodeRef: cell = value.value().text; break;
                    }
                }

                stream.write_row(row);
                ++written;
            }

            // Flushes and ends the COPY. Called explicitly rather than left to the
            // destructor, because a failure here has to be an exception this
            // function catches and not one thrown while unwinding.
            stream.complete();
        }

        // The index a GIS reader will want on the first pan. Building it after the
        // rows rather than before is the cheaper order and the one every bulk load
        // uses.
        tx.exec("create index on " + tx.quote_name(name) + " using gist (geom)");
        tx.commit();
        return written;
    } catch (const std::exception& e) {
        return core::err(ErrorCode::Internal, "'" + name + "' tablosuna yazılamadı: " + e.what());
    }
}

core::Status PostgisStore::write_project(const std::string& name,
                                         const std::vector<std::byte>& bytes)
{
    if (name.empty()) return core::err(ErrorCode::InvalidArgument, "Proje adı boş olamaz.");
    if (bytes.empty()) return core::err(ErrorCode::InvalidArgument, "Kaydedilecek proje boş.");

    try {
        pqxx::work tx(impl_->connection);

        tx.exec(std::string("create table if not exists ") + kProjectTable +
                " (ad text primary key, guncelleme timestamptz not null default now(), "
                " veri bytea not null)");

        pqxx::params values;
        values.append(name);
        values.append(pqxx::binary_cast(bytes));

        tx.exec_params(std::string("insert into ") + kProjectTable +
                           " (ad, veri) values ($1, $2) "
                           " on conflict (ad) do update set veri = excluded.veri, "
                           " guncelleme = now()",
                       values);
        tx.commit();
        return core::ok();
    } catch (const std::exception& e) {
        return core::err(ErrorCode::Internal,
                         "Proje veritabanına yazılamadı: " + std::string(e.what()));
    }
}

core::Result<std::vector<std::byte>> PostgisStore::read_project(const std::string& name)
{
    try {
        pqxx::work tx(impl_->connection);

        pqxx::params values;
        values.append(name);

        const auto row = tx.query01<pqxx::bytes>(
            std::string("select veri from ") + kProjectTable + " where ad = $1", values);
        if (!row)
            return core::err(ErrorCode::NotFound,
                             "Veritabanında böyle bir proje yok: '" + name + "'.");

        const pqxx::bytes& blob = std::get<0>(*row);
        std::vector<std::byte> out(blob.begin(), blob.end());
        tx.commit();
        return out;
    } catch (const std::exception& e) {
        return core::err(ErrorCode::Internal,
                         "Proje veritabanından okunamadı: " + std::string(e.what()));
    }
}

core::Result<std::size_t> PostgisStore::delete_project(const std::string& name)
{
    try {
        pqxx::work tx(impl_->connection);

        // `to_regclass` returns null rather than raising when the table is not
        // there, so a database that has never stored a project answers "nothing by
        // that name" instead of an error about a missing relation.
        const auto exists = tx.query_value<bool>(std::string("select to_regclass('") +
                                                 kProjectTable + "') is not null");
        if (!exists) return std::size_t{0};

        const pqxx::result done =
            tx.exec_params(std::string("delete from ") + kProjectTable + " where ad = $1", name);
        tx.commit();
        return static_cast<std::size_t>(done.affected_rows());
    } catch (const std::exception& e) {
        return core::err(ErrorCode::Internal,
                         "Proje veritabanından silinemedi: " + std::string(e.what()));
    }
}

core::Result<std::vector<PostgisProject>> PostgisStore::projects()
{
    try {
        pqxx::work tx(impl_->connection);

        // The catalogue may not exist yet on a database nobody has saved into, and
        // that is not an error: it is an empty list.
        const auto exists = tx.query_value<bool>(std::string("select to_regclass('") +
                                                 kProjectTable + "') is not null");
        if (!exists) {
            tx.commit();
            return std::vector<PostgisProject>{};
        }

        std::vector<PostgisProject> out;

        // Turkish date order and the CLIENT's clock, not UTC. `guncelleme` is a
        // `timestamptz`, so `to_char` renders it in the connection's own TimeZone
        // — which libpq sets from the caller's environment — and a Turkish user
        // reads the hour they saved at rather than one three hours earlier.
        for (const auto& row : tx.query<std::string, std::string, std::int64_t>(
                 std::string("select ad, to_char(guncelleme, 'DD.MM.YYYY HH24:MI'), "
                             "       octet_length(veri) from ") +
                 kProjectTable + " order by guncelleme desc")) {
            PostgisProject project;
            project.name    = std::get<0>(row);
            project.updated = std::get<1>(row);
            project.bytes   = std::get<2>(row);
            out.push_back(std::move(project));
        }
        tx.commit();
        return out;
    } catch (const std::exception& e) {
        return core::err(ErrorCode::Internal, "Projeler listelenemedi: " + std::string(e.what()));
    }
}

#endif // KENTOS_HAVE_POSTGIS

} // namespace kentos::io
