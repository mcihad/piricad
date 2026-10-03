// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/io/source_info.hpp"

#include "piricad/core/text.hpp"
#include "piricad/io/vector.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

#ifdef PIRICAD_HAVE_GDAL
#include <cpl_conv.h>
#include <cpl_error.h>
#include <gdal_priv.h>
#include <ogr_spatialref.h>
#include <ogrsf_frmts.h>
#endif

namespace piricad::io {

#ifdef PIRICAD_HAVE_GDAL

namespace {

/// The drivers this product allows, as GDAL's open call wants them. Rebuilt per call: this runs
/// once per source a person asks about, and a function-local static would be a second copy of the
/// list the importer keeps — both are made from `vector_formats()`, the one allow-list.
char** allowed_drivers()
{
    char** list = nullptr;
    for (const VectorFormat& f : vector_formats())
        list = ::CSLAddString(list, f.driver.c_str());
    return list;
}

std::string reason()
{
    const char* msg = ::CPLGetLastErrorMsg();
    return (msg != nullptr && *msg != '\0') ? std::string(msg) : std::string("ayrıntı yok");
}

bool yes(GDALDriver* driver, const char* key)
{
    const char* value = driver->GetMetadataItem(key);
    return value != nullptr && std::string(value) == "YES";
}

/// `AUTHORITY:CODE` when the system has one, its name otherwise, empty for none.
std::string crs_text(const OGRSpatialReference* srs)
{
    if (srs == nullptr) return {};
    const char* authority = srs->GetAuthorityName(nullptr);
    const char* code      = srs->GetAuthorityCode(nullptr);
    if (authority != nullptr && code != nullptr) return std::string(authority) + ":" + code;
    const char* name = srs->GetName();
    return name != nullptr ? std::string(name) : std::string("(adsız)");
}

std::string unit_text(const OGRSpatialReference* srs)
{
    if (srs == nullptr) return {};
    const char* name = nullptr;
    if (srs->IsGeographic())
        (void)srs->GetAngularUnits(&name);
    else
        (void)srs->GetLinearUnits(&name);
    return name != nullptr ? std::string(name) : std::string{};
}

std::string field_type_name(const OGRFieldDefn& f)
{
    // A boolean is stored in an integer field with a subtype; GDAL's own name for the pair.
    if (f.GetSubType() != OFSTNone) return OGRFieldDefn::GetFieldSubTypeName(f.GetSubType());
    return OGRFieldDefn::GetFieldTypeName(f.GetType());
}

LayerFacts facts_of(OGRLayer& layer)
{
    LayerFacts out;
    out.name = layer.GetName() != nullptr ? layer.GetName() : "";

    const OGRwkbGeometryType type = layer.GetGeomType();
    out.geometry                  = OGRGeometryTypeToName(wkbFlatten(type));
    out.has_z                     = OGR_GT_HasZ(type) != 0;
    out.has_m                     = OGR_GT_HasM(type) != 0;
    out.curved                    = OGR_GT_IsNonLinear(type) != 0;

    const OGRSpatialReference* srs = layer.GetSpatialRef();
    out.crs                        = crs_text(srs);
    out.crs_unit                   = unit_text(srs);

    // The count WITHOUT forcing a scan: a negative answer means counting would read the whole
    // layer.
    out.rows       = static_cast<std::int64_t>(layer.GetFeatureCount(FALSE));
    out.rows_exact = layer.TestCapability(OLCFastFeatureCount) != 0;

    OGREnvelope box;
    if (layer.GetExtent(&box, FALSE) == OGRERR_NONE) {
        out.has_extent = true;
        out.extent[0]  = box.MinX;
        out.extent[1]  = box.MinY;
        out.extent[2]  = box.MaxX;
        out.extent[3]  = box.MaxY;
    }

    if (const char* fid = layer.GetFIDColumn(); fid != nullptr) out.fid_column = fid;
    const OGRFeatureDefn* defn = layer.GetLayerDefn();
    if (defn != nullptr) {
        if (defn->GetGeomFieldCount() > 0) {
            const OGRGeomFieldDefn* geom = defn->GetGeomFieldDefn(0);
            if (geom != nullptr) {
                out.geometry_column   = geom->GetNameRef() != nullptr ? geom->GetNameRef() : "";
                out.geometry_nullable = geom->IsNullable() != 0;
            }
        }
        for (int i = 0; i < defn->GetFieldCount(); ++i) {
            const OGRFieldDefn* f = defn->GetFieldDefn(i);
            if (f == nullptr) continue;
            FieldFacts field;
            field.name      = f->GetNameRef() != nullptr ? f->GetNameRef() : "";
            field.type      = field_type_name(*f);
            field.width     = f->GetWidth();
            field.precision = f->GetPrecision();
            field.nullable  = f->IsNullable() != 0;
            field.unique    = f->IsUnique() != 0;
            if (const char* def = f->GetDefault(); def != nullptr) field.default_value = def;
            field.domain = f->GetDomainName();
            out.fields.push_back(std::move(field));
        }
    }

    out.random_read         = layer.TestCapability(OLCRandomRead) != 0;
    out.fast_spatial_filter = layer.TestCapability(OLCFastSpatialFilter) != 0;
    out.fast_count          = out.rows_exact;
    out.fast_extent         = layer.TestCapability(OLCFastGetExtent) != 0;
    out.transactions        = layer.TestCapability(OLCTransactions) != 0;
    out.curve_geometries    = layer.TestCapability(OLCCurveGeometries) != 0;
    out.measured_geometries = layer.TestCapability(OLCMeasuredGeometries) != 0;
    out.z_geometries        = layer.TestCapability(OLCZGeometries) != 0;
    return out;
}

} // namespace

core::Result<SourceInfo> inspect_source(const std::string& path)
{
    ::GDALAllRegister();
    ::CPLErrorReset();

    char** drivers    = allowed_drivers();
    GDALDataset* data = static_cast<GDALDataset*>(
        ::GDALOpenEx(path.c_str(), GDAL_OF_VECTOR | GDAL_OF_READONLY, drivers, nullptr, nullptr));
    ::CSLDestroy(drivers);
    if (data == nullptr) {
        // WHAT IS ALLOWED, in the message: "izin listesinde olduğunu denetleyin" sends a person to
        // a list they cannot see. The list is the importer's own.
        std::string readable;
        for (const VectorFormat& f : vector_formats())
            if (f.read) readable += (readable.empty() ? "" : ", ") + f.driver;
        return core::err(core::ErrorCode::IoFailure,
                         "'" + path + "' açılamadı: " + reason() +
                             ". Dosyanın var olduğunu denetleyin; PiriCAD'in okuduğu biçimler: " +
                             readable + ".");
    }

    SourceInfo info;
    info.path = path;
    if (GDALDriver* driver = data->GetDriver(); driver != nullptr) {
        info.driver                  = driver->GetDescription();
        const char* longname         = driver->GetMetadataItem(GDAL_DMD_LONGNAME);
        info.driver_name             = longname != nullptr ? longname : "";
        info.driver_can_create       = yes(driver, GDAL_DCAP_CREATE);
        info.driver_can_create_layer = yes(driver, GDAL_DCAP_CREATE_LAYER);
        info.driver_can_delete_layer = yes(driver, GDAL_DCAP_DELETE_LAYER);
    }
    if (const VectorFormat* format = vector_format_by_id(info.driver); format != nullptr) {
        info.piricad_reads  = format->read;
        info.piricad_writes = format->write;
    }
    for (int i = 0; i < data->GetLayerCount(); ++i)
        if (OGRLayer* layer = data->GetLayer(i); layer != nullptr)
            info.layers.push_back(facts_of(*layer));
    ::GDALClose(data);
    return info;
}

#else // ----------------------------------------------------------------------

core::Result<SourceInfo> inspect_source(const std::string&)
{
    return core::err(core::ErrorCode::Unsupported, vector_backend_status());
}

#endif

namespace {

const char* flag(bool on)
{
    return on ? "var" : "yok";
}

std::string typed(const FieldFacts& f)
{
    std::string out = f.name + " " + f.type;
    if (f.width > 0) {
        out += "(" + std::to_string(f.width);
        if (f.precision > 0) out += "," + std::to_string(f.precision);
        out += ")";
    }
    std::string rules;
    auto add = [&rules](const char* text) {
        rules += (rules.empty() ? "" : ", ") + std::string(text);
    };
    if (!f.nullable) add("boş olamaz");
    if (f.unique) add("benzersiz");
    if (!f.default_value.empty()) add(("varsayılan " + f.default_value).c_str());
    if (!f.domain.empty()) add(("alan kuralı " + f.domain).c_str());
    return rules.empty() ? out : out + " [" + rules + "]";
}

std::string number(double v)
{
    std::string out = core::format_general(v, 12);
    std::replace(out.begin(), out.end(), '.', ',');
    return out;
}

std::string geometry_tr(const std::string& english)
{
    // GDAL's own names, in the words a surveyor uses. A type not listed is shown as GDAL gives it
    // rather than guessed at.
    static const std::pair<const char*, const char*> names[] = {
        {"Point", "nokta"},
        {"Multi Point", "çoklu nokta"},
        {"Line String", "çizgi"},
        {"Multi Line String", "çoklu çizgi"},
        {"Polygon", "alan"},
        {"Multi Polygon", "çoklu alan"},
        {"Geometry Collection", "geometri karması"},
        {"Circular String", "daire yayı"},
        {"Compound Curve", "bileşik eğri"},
        {"Curve Polygon", "eğrili alan"},
        {"Multi Curve", "çoklu eğri"},
        {"Multi Surface", "çoklu yüzey"},
        {"None", "geometrisiz (yalnız öznitelik)"},
        {"Unknown (any)", "karışık/bilinmeyen"},
    };
    for (const auto& [en, tr] : names)
        if (english == en) return tr;
    return english;
}

} // namespace

std::string describe(const SourceInfo& info, const std::string& only)
{
    std::string out = "Kaynak: " + info.path + "\n";
    out += "Sürücü: " + info.driver +
           (info.driver_name.empty() ? "" : " (" + info.driver_name + ")") + "\n";
    out += std::string("  PiriCAD okur (İÇEAKTAR): ") + flag(info.piricad_reads) +
           " · yazar (DIŞAAKTAR): " + flag(info.piricad_writes) +
           " · sürücü yeni veri kümesi yazabilir: " + flag(info.driver_can_create) +
           " · katman ekleyebilir: " + flag(info.driver_can_create_layer) +
           " · katman silebilir: " + flag(info.driver_can_delete_layer) + "\n";
    // THE ONE THING A READER MUST NOT ASSUME, said once and plainly: nothing this program does
    // writes back into a source file. What is imported is a copy.
    out += "  PiriCAD kaynağa GERİ YAZMAZ: içe alınan bir kopyadır; değişikliği DIŞAAKTAR yeni bir "
           "dosyaya yazar.\n";

    std::size_t shown = 0;
    for (const LayerFacts& l : info.layers) {
        if (!only.empty() && !core::turkish_iequals(l.name, only)) continue;
        ++shown;
        out += "\n[" + std::to_string(shown) + "] " + l.name + " — " + geometry_tr(l.geometry);
        if (l.has_z) out += " (Z)";
        if (l.has_m) out += " (M)";
        out += "\n";

        out += "  Sistem: " + (l.crs.empty() ? std::string("bildirmiyor") : l.crs);
        if (!l.crs_unit.empty()) out += " (" + l.crs_unit + " sayar)";
        out += "\n";

        out += "  Satır: ";
        if (l.rows < 0)
            out += "bilinmiyor (saymak bütün katmanı okumak demek)";
        else
            out += (l.rows_exact ? "" : "yaklaşık ") + std::to_string(l.rows) +
                   (l.rows_exact ? " (kesin)" : " (tahmin)");
        if (l.has_extent)
            out += " · sınır kutusu " + number(l.extent[0]) + " " + number(l.extent[1]) + " … " +
                   number(l.extent[2]) + " " + number(l.extent[3]);
        out += "\n";

        out += "  Kimlik: " + (l.fid_column.empty() ? std::string("kaynak satır kimliği vermiyor")
                                                    : l.fid_column + " (satır kimliği)");
        if (!l.geometry_column.empty()) out += " · geometri sütunu: " + l.geometry_column;
        if (!l.geometry_nullable) out += " · geometri boş olamaz";
        out += "\n";

        if (!l.fields.empty()) {
            out += "  Alanlar (" + std::to_string(l.fields.size()) + "): ";
            for (std::size_t i = 0; i < l.fields.size(); ++i)
                out += (i == 0 ? "" : "; ") + typed(l.fields[i]);
            out += "\n";
        }

        out += std::string("  Yetenekler: rastgele okuma ") + flag(l.random_read) +
               " · dizinli konum süzgeci " + flag(l.fast_spatial_filter) + " · hızlı sayım " +
               flag(l.fast_count) + " · işlem (transaction) " + flag(l.transactions) + " · eğri " +
               flag(l.curve_geometries) + " · Z " + flag(l.z_geometries) + " · M " +
               flag(l.measured_geometries) + "\n";

        // WHAT PIRICAD DOES WITH IT, which is a different question from what the driver can do.
        std::string loses;
        auto lose = [&loses](const std::string& text) { loses += "\n    - " + text; };
        if (l.has_z) lose("Yükseklik (Z): çizim düzlemseldir; içe alırken atılır ve söylenir.");
        if (l.has_m) lose("Ölçü değeri (M): atılır.");
        if (l.crs.empty())
            lose("Sistem bildirmiyor: GIS biçimlerinde içe alma reddedilir (DXF'te çizimin sistemi "
                 "varsayılır ve uyarılır).");
        else if (!l.crs_unit.empty() && l.crs_unit != "metre")
            lose("Sistem '" + l.crs_unit +
                 "' sayar, metre değil: sayılar olduğu gibi okunamaz; İÇEAKTAR cevir=evet PROJ ile "
                 "çizimin sistemine taşır.");
        const bool constrained =
            std::any_of(l.fields.begin(), l.fields.end(), [](const FieldFacts& f) {
                return !f.nullable || f.unique || !f.default_value.empty() || !f.domain.empty();
            });
        if (constrained)
            lose("Alan kısıtları (boş olamaz, benzersiz, varsayılan, alan kuralı) içe alırken "
                 "taşınmaz; sütunlar serbest açılır.");
        if (l.rows < 0 || !l.rows_exact)
            lose("Satır sayısı bu kaynakta ucuz değil; içe aktarma sayıyı okuma bitince bilir.");
        if (loses.empty())
            out += "  PiriCAD: bu katmanı içe alırken kayıp bildirmiyor.\n";
        else
            out += "  PiriCAD içe alırken:" + loses + "\n";
    }
    if (shown == 0)
        out += only.empty() ? "\nKaynak katman içermiyor.\n"
                            : "\n'" + only + "' adında bir katman yok. Katmanlar: " +
                                  [&info] {
                                      std::string names;
                                      for (const LayerFacts& l : info.layers)
                                          names += (names.empty() ? "" : ", ") + l.name;
                                      return names.empty() ? std::string("(yok)") : names;
                                  }() +
                                  ".\n";
    return out;
}

core::Json to_json(const SourceInfo& info)
{
    core::Json out;
    out.set("kaynak", core::Json::string(info.path));
    out.set("surucu", core::Json::string(info.driver));
    out.set("surucu_adi", core::Json::string(info.driver_name));
    out.set("piricad_okur", core::Json::boolean(info.piricad_reads));
    out.set("piricad_yazar", core::Json::boolean(info.piricad_writes));
    out.set("surucu_olusturur", core::Json::boolean(info.driver_can_create));
    out.set("surucu_katman_ekler", core::Json::boolean(info.driver_can_create_layer));
    out.set("surucu_katman_siler", core::Json::boolean(info.driver_can_delete_layer));
    out.set("kaynaga_geri_yazma", core::Json::boolean(false));
    core::Json layers = core::Json::array({});
    for (const LayerFacts& l : info.layers) {
        core::Json one;
        one.set("ad", core::Json::string(l.name));
        one.set("geometri", core::Json::string(l.geometry));
        one.set("z", core::Json::boolean(l.has_z));
        one.set("m", core::Json::boolean(l.has_m));
        one.set("egri_tipi", core::Json::boolean(l.curved));
        one.set("sistem", core::Json::string(l.crs));
        one.set("sistem_birimi", core::Json::string(l.crs_unit));
        one.set("satir", core::Json::integer(l.rows));
        one.set("satir_kesin", core::Json::boolean(l.rows_exact));
        if (l.has_extent) {
            core::Json box = core::Json::array({});
            for (const double v : l.extent)
                box.push(core::Json::number(v));
            one.set("sinir_kutusu", std::move(box));
        }
        one.set("kimlik_sutunu", core::Json::string(l.fid_column));
        one.set("geometri_sutunu", core::Json::string(l.geometry_column));
        one.set("geometri_bos_olabilir", core::Json::boolean(l.geometry_nullable));
        core::Json fields = core::Json::array({});
        for (const FieldFacts& f : l.fields) {
            core::Json field;
            field.set("ad", core::Json::string(f.name));
            field.set("tur", core::Json::string(f.type));
            field.set("genislik", core::Json::integer(f.width));
            field.set("basamak", core::Json::integer(f.precision));
            field.set("bos_olabilir", core::Json::boolean(f.nullable));
            field.set("benzersiz", core::Json::boolean(f.unique));
            field.set("varsayilan", core::Json::string(f.default_value));
            field.set("alan_kurali", core::Json::string(f.domain));
            fields.push(std::move(field));
        }
        one.set("alanlar", std::move(fields));
        core::Json caps;
        caps.set("rastgele_okuma", core::Json::boolean(l.random_read));
        caps.set("dizinli_konum_suzgeci", core::Json::boolean(l.fast_spatial_filter));
        caps.set("hizli_sayim", core::Json::boolean(l.fast_count));
        caps.set("hizli_sinir_kutusu", core::Json::boolean(l.fast_extent));
        caps.set("islem", core::Json::boolean(l.transactions));
        caps.set("egri", core::Json::boolean(l.curve_geometries));
        caps.set("olcu_m", core::Json::boolean(l.measured_geometries));
        caps.set("yukseklik_z", core::Json::boolean(l.z_geometries));
        one.set("yetenekler", std::move(caps));
        layers.push(std::move(one));
    }
    out.set("katmanlar", std::move(layers));
    return out;
}

} // namespace piricad::io
