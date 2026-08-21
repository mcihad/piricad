# AI evaluation set

200–300 real Turkish user requests paired with the expected command sequence
(piricad.md §5.6, §14). Run in CI; a drop below the stored accuracy baseline
breaks the build.

The set exists because a model that scores well on English benchmarks can be weak
on Turkish surveying and planning terminology — ifraz, tevhit, ihdas, DOP, TAKS,
KAKS, nazım imar planı, muhdesat, irtifak. Turkish performance is measured
separately, never inferred.

Empty until Phase 3.
