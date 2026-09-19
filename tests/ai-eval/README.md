# AI evaluation set

`senaryolar.json` holds real Turkish requests paired with the command sequence a
model is expected to produce (kentoscad.md §5.6, §14; `.claude/ai.md` R21 asks for
200–300).

The set answers two questions and does not confuse them.

**1. Is the expected sequence still valid?** Every step is parsed and resolved
against the live command registry by `tests/unit/test_ai_eval.cpp`. A set naming a
command that has since been renamed, or an argument it no longer takes, fails the
build — so an answer key cannot rot while the program moves. It caught five
mistakes in the first sixteen cases on the day they were written.

**2. Do the three clients produce the same thing?** Each scenario runs from the
command line, from a JSON script and through the agent's dispatch, and the
document's content hash and the journal's lines must match across all three. That
is TODOS A-08's acceptance sentence, and it is the half a test can prove. It found
a real defect immediately: the atlas block was not folded into `content_hash`, so
two drawings printing entirely different sheet sets shared a fingerprint.

**What is not measured here, and cannot be:** whether a MODEL produces these
sequences. No test in this program calls a live provider (`.claude/ai.md` P10).
The set defines the right answer; measuring a model against it is a separate
runner with a provider behind it, and its accuracy baseline belongs there.

## Size

The set is a STARTER, sanctioned by CLAUDE.md Article 8.9, whose removal condition
is 200 cases covering §5.6's vocabulary. `scripts/ci-gate-eval.sh` prints the count
and the shortfall on every `make check`, which is the condition Article 8.9
attaches to the deviation: the gap is counted, not assumed.

Closing it is domain work — someone who knows ifraz from tevhit — not engineering.
The terminology it must reach: ifraz, tevhit, ihdas, DOP, TAKS, KAKS, nazım imar
planı, muhdesat, irtifak.

## Adding a case

```json
{
  "id": "kisa-kimlik",
  "istek": "Kullanıcının Türkçe cümlesi.",
  "terimler": ["sınanan", "alan", "sözcükleri"],
  "kurulum": ["Ölçüme girmeyen hazırlık satırları"],
  "komutlar": ["Beklenen komut dizisi, sırasıyla"]
}
```

`kurulum` runs before the measured commands and is excluded from the comparison.
Every line in both lists is a real command line: the test refuses anything else.
