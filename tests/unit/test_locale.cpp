// SPDX-License-Identifier: GPL-3.0-or-later
//
// A FIGURE IS THE SAME FIGURE ON EVERY MACHINE (§7.3, CLAUDE.md 5.11).
//
// `std::strtod` and `snprintf("%g")` read and write the decimal separator of the process's NUMERIC
// locale, and Qt puts the user's `LC_NUMERIC` there when the program starts. On a Turkish machine
// that is `tr_TR`, whose separator is a comma: `strtod("12.5")` stopped at the point and answered
// 12, so a typed `@12.5<50` was twelve metres, an easting `485320.150` was 485320, and the DXF
// writer's `%g` would have put `12,5` into a file every other program reads with a point. It was
// found by a probe that typed `12.5` and measured 12.0004 m.
//
// These cases run the parser and the DXF writer UNDER that locale — the app resets it at start-up
// (`main.cpp`), and the libraries do not rely on that.
#include "piricad_test.hpp"

#include "piricad/command/bus.hpp"
#include "piricad/command/parser.hpp"
#include "piricad/command/registry.hpp"
#include "piricad/core/text.hpp"
#include "piricad/io/service.hpp"

#include <clocale>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

using namespace piricad;
namespace fs = std::filesystem;

namespace {

/// The numeric category set to the hostile locale for the length of a case, and put back after it.
class TurkishNumbers
{
public:
    TurkishNumbers()
    {
        const char* now = std::setlocale(LC_NUMERIC, nullptr);
        if (now != nullptr) before_ = now;
        for (const char* name : {"tr_TR.UTF-8", "tr_TR.utf8", "tr_TR"})
            if (std::setlocale(LC_NUMERIC, name) != nullptr) {
                const char* point = std::localeconv()->decimal_point;
                active_           = point != nullptr && point[0] == ',';
                if (active_) return;
            }
        std::setlocale(LC_NUMERIC, before_.c_str());
    }

    ~TurkishNumbers() { std::setlocale(LC_NUMERIC, before_.c_str()); }

    TurkishNumbers(const TurkishNumbers&)            = delete;
    TurkishNumbers& operator=(const TurkishNumbers&) = delete;

    /// Whether the machine has the locale: the cases are PENDING, never passing, where it has not.
    bool active() const noexcept { return active_; }

private:
    std::string before_;
    bool active_{false};
};

} // namespace

TEST_CASE("YEREL: Türkçe sayı yerel ayarında bile nokta ondalık noktadır")
{
    TurkishNumbers tr;
    if (!tr.active()) PENDING("tr_TR yerel ayarı yüklü değil; virgüllü ondalık denenemiyor.");

    // The hostile condition is real: the C library itself reads the point as the end of the number.
    CHECK(std::strtod("12.5", nullptr) == 12.0);

    CHECK(core::parse_decimal("12.5") == 12.5);
    CHECK(core::parse_decimal("0.001") == 0.001);
    CHECK(core::parse_decimal("1e3") == 1000.0);
    CHECK(core::parse_decimal("-7.25") == -7.25);
    CHECK_FALSE(core::parse_decimal("12.5x").has_value()); ///< the whole text is the number
    CHECK_FALSE(core::parse_decimal("").has_value());
    CHECK_FALSE(core::parse_decimal(".").has_value());
    CHECK_FALSE(core::parse_decimal("12,5").has_value()); ///< a comma is not this language's point

    // The grammar, through the one parser (CLAUDE.md 5.11): a unit, a prompt's number, a polar
    // point and a TUREF coordinate.
    auto metres = command::evaluate_quantity("1250 cm", 0);
    REQUIRE(metres.ok());
    CHECK(metres.value().value == 12.5);
    auto answer = command::evaluate_answer("12.5", 0);
    REQUIRE(answer.ok());
    CHECK(answer.value() == 12.5);

    command::ResolveContext ctx;
    ctx.convention = core::AngleConvention{core::AngleUnit::Grad, core::AngleRule::Semt};
    auto polar     = command::parse_point("@12.5<100", core::Point2{0, 0}, ctx);
    REQUIRE(polar.ok());
    CHECK(polar.value() == core::Point2{12'500, 0});
    auto easting = command::parse_point("485320.150,4310220.400", core::Point2{0, 0}, ctx);
    REQUIRE(easting.ok());
    CHECK(easting.value() == core::Point2{485'320'150, 4'310'220'400});
}

TEST_CASE("YEREL: yazılan sayı da nokta ile yazılır")
{
    TurkishNumbers tr;
    if (!tr.active()) PENDING("tr_TR yerel ayarı yüklü değil; virgüllü ondalık denenemiyor.");

    CHECK(core::format_general(12.5, 15) == "12.5");
    CHECK(core::format_general(485320.15, 15) == "485320.15");
    CHECK(core::format_general(0.1, 15) == "0.1");
    CHECK(core::format_general(-3.0, 15) == "-3");
    CHECK(core::format_general(1.0e-7, 6) == "1e-07");
}

TEST_CASE("YEREL: DXF dosyasında koordinatlar nokta ile yazılır")
{
    if (!io::vector_backend_available()) PENDING("PIRICAD_WITH_GDAL=OFF; DXF yazılamıyor.");
    TurkishNumbers tr;
    if (!tr.active()) PENDING("tr_TR yerel ayarı yüklü değil; virgüllü ondalık denenemiyor.");

    const fs::path dir = fs::temp_directory_path() / "piricad-yerel-dxf";
    fs::create_directories(dir);
    const std::string path = (dir / "cizim.dxf").string();

    core::Document doc;
    command::Registry reg;
    command::Journal journal;
    command::UndoStack undo;
    command::Bus bus{doc, reg, journal, undo};
    io::FileService files{bus};
    command::register_builtin_commands(reg);
    bus.on_echo = [](std::string_view) {};

    REQUIRE(bus.execute_line("AYAR core.crs.id EPSG:5254", command::Origin::Test).ok());
    REQUIRE(bus.execute_line("ÇİZGİ 485320.150,4310220.400 485370.250,4310250.450",
                             command::Origin::Test)
                .ok());
    auto written = bus.execute_line("DIŞAAKTAR \"" + path + "\"", command::Origin::Test);
    REQUIRE_MESSAGE(written.ok(), (written.ok() ? "" : written.error().message));

    std::ifstream in(path);
    std::stringstream text;
    text << in.rdbuf();
    const std::string dxf = text.str();
    CHECK_MESSAGE(dxf.find("485320.15") != std::string::npos, "koordinat noktayla yazılmadı");
    CHECK_MESSAGE(dxf.find("4310250.45") != std::string::npos,
                  "ikinci koordinat noktayla yazılmadı");
    // No numeric group carries a comma: every value line of the file is a number, a name or a
    // string, and a coordinate with a comma is exactly the failure.
    CHECK(dxf.find("485320,15") == std::string::npos);
    CHECK(dxf.find("4310250,45") == std::string::npos);

    fs::remove_all(dir);
}
