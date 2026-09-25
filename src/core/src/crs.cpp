// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/core/crs.hpp"

namespace kentos::core {

std::string crs_unit_problem(const Crs& crs)
{
    if (crs.holds_metres()) return {};

    const std::string named = "'" + crs.id() + "'";
    const std::string unit =
        crs.unit_name().empty() ? std::string("bilinmeyen bir birim") : "'" + crs.unit_name() + "'";

    // WHAT GOES WRONG, in ground terms, because "the unit is not metres" says
    // nothing to someone who has never had to think about it. A millidegree is
    // a hundred metres; a foot read as a metre is a third too long.
    std::string said;
    if (crs.unit() == CrsUnit::Degree)
        said = named +
               " coğrafi bir koordinat sistemi: koordinatlarını derece olarak sayar. "
               "KentOSCad çizim koordinatlarını metre olarak, milimetre çözünürlükte saklar; "
               "bir dereceyi metre saymak her köşeyi yüz metrelik bir ızgaraya oturtur.";
    else
        said = named + " koordinatlarını " + unit +
               " birimiyle sayar. KentOSCad çizim koordinatlarını metre olarak saklar; başka "
               "bir birimdeki sayıyı metre saymak her uzunluğu ve her alanı yanlış ölçekler.";

    return said;
}

std::string crs_metric_hint()
{
    // The way out, named rather than implied. The zone list is DATA
    // (/data/crs/tm3-dilimleri.json) and this line does not read it: it names
    // the range a user types, which is the EPSG registry's, not a regulation's.
    return "Metre birimli bir izdüşüm sistemi seçin — Türkiye için TUREF/TM27 … TUREF/TM45 "
           "(EPSG:5253–5259).";
}

} // namespace kentos::core
