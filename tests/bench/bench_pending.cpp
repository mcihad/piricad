// SPDX-License-Identifier: GPL-3.0-or-later
//
// The §10.1 budgets that cannot be measured yet. They are listed rather than
// omitted, because a budget that quietly disappears from the report is a budget
// nobody is accountable for. A pending scenario reports BEKLEMEDE and never PASS.
#include "benchmark.hpp"

PIRICAD_BENCH(open_dwg){bench::Case{
    .id      = "io.dwg_200mb_acilis",
    .title   = "200 MB DWG açılışı",
    .budget  = 3000.0,
    .unit    = "ms",
    .pending = "DWG okuyucusu yok; io.md R13/R14 önce lisansı temizlenmiş 50+ dosyalık kapsam "
               "raporunu istiyor (§9.8, §15)",
}};

PIRICAD_BENCH(first_paint_laz){bench::Case{
    .id      = "io.laz_50m_ilk_goruntu",
    .title   = "50M noktalı LAZ ilk görüntüleme",
    .budget  = 5000.0,
    .unit    = "ms",
    .pending = "LAS/LAZ okuyucusu yok; laz-perf ve nokta bulutu boru hattı gerekiyor (§9.10)",
}};

PIRICAD_BENCH(topology_validation){bench::Case{
    .id      = "domain.topoloji_100k_parsel",
    .title   = "100k parselde topolojik doğrulama",
    .budget  = 2000.0,
    .unit    = "ms",
    .pending = "/src/domain boş; topoloji kural motoru gerekiyor (§2.6, §12)",
}};

PIRICAD_BENCH(cold_start){bench::Case{
    .id      = "uygulama.soguk_acilis",
    .title   = "Uygulama soğuk açılışı",
    .budget  = 2000.0,
    .unit    = "ms",
    .pending = "Qt penceresinin ilk karesine kadar ölçüm gerekiyor; başsız harness ölçemez",
}};

PIRICAD_BENCH(empty_ram){bench::Case{
    .id      = "uygulama.bos_proje_ram",
    .title   = "Boş projede RAM",
    .budget  = 300.0,
    .unit    = "MB",
    .pending = "Qt süreci içinde ölçülmeli; başsız harness yalnız çekirdeği görür",
}};

PIRICAD_BENCH(keystroke){bench::Case{
    .id      = "arayuz.tus_ekran_gecikmesi",
    .title   = "Komut satırı tuş → ekran gecikmesi",
    .budget  = 30.0,
    .unit    = "ms",
    .pending = "Qt olay döngüsü içinde ölçülmeli; komut.ayristir_ve_gonder bunun payıdır",
}};
