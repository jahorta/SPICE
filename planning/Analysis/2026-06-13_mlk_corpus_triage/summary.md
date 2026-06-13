# MLK Corpus Triage

## Summary

This pass extends the read-only MLK scanner with corpus-level triage reports and
uses the generated US/EU/JP corpus data plus existing Ghidra decompiles to define
the next parser boundary.

The important correction from this pass is that the observed runtime MLK walkers
use the signed halfword at `buffer + 0x04` as their loop bound. First-payload
offset inference is useful evidence for anomalous files, but it should not
silently replace the runtime-observed count until a subtype-specific code path is
found.

## Generated Reports

`--export-mlk-corpus` now writes seven files:

- `mlk_corpus.json`
- `mlk_corpus_files.csv`
- `mlk_corpus_records.csv`
- `mlk_corpus_word12_histogram.csv`
- `mlk_corpus_anomalies.csv`
- `mlk_corpus_raw_word12_by_kind.csv`
- `mlk_corpus_embedded_mld_summary.csv`

The new `tableShape` field has three values:

- `normal`: no record-span errors and no count mismatch evidence.
- `first-payload-count-candidate`: the first payload offset implies a plausible
  alternate count, but the runtime halfword count is still preserved.
- `malformed-record-spans`: the table count matches first-payload offset, but
  one or more records produce invalid payload spans.

## Regional Results

US:

- Files: 574
- `normal`: 569
- `first-payload-count-candidate`: 1
- `malformed-record-spans`: 4
- Embedded MLD rows accepted by entry-list parse: 5730

EU:

- Files: 577
- `normal`: 572
- `first-payload-count-candidate`: 1
- `malformed-record-spans`: 4
- Embedded MLD rows accepted by entry-list parse: 5754

JP:

- Files: 578
- `normal`: 573
- `first-payload-count-candidate`: 1
- `malformed-record-spans`: 4
- Embedded MLD rows accepted by entry-list parse: 5759

The same five anomaly files appear in all three regions:

| path | tableShape | header count | inferred first-payload count | payload span errors | embedded MLD accepted |
| --- | --- | ---: | ---: | ---: | ---: |
| `beff/d2403900.mlk` | `first-payload-count-candidate` | 20992 | 82 | 15795 | 33 |
| `beff/d2900200.mlk` | `malformed-record-spans` | 28 | 28 | 14 | 14 |
| `beff/f2705733.mlk` | `malformed-record-spans` | 10 | 10 | 5 | 5 |
| `beff/f2900200.mlk` | `malformed-record-spans` | 28 | 28 | 14 | 14 |
| `beff/f290986b.mlk` | `malformed-record-spans` | 10 | 10 | 5 | 5 |

## Ghidra Evidence

Existing decompile outputs under
`tools/ghidra/analyses/20260613_initial_mlk_investigation` were revisited.

Relevant findings:

- `FUN_8006e6e4@8006e6e4` initializes the record pointer at `param_1 + 8` and
  loops while `sVar1 < *(short *)(param_1 + 4)`.
- `FUN_8006e4cc@8006e4cc` computes its reverse chunk window from
  `(*(short *)(param_1 + 4) - *param_2) - 1`.
- `FUN_8006e640@8006e640` continues while
  `*param_3 < *(short *)(param_1 + 4)`.
- `FUN_8006e5bc@8006e5bc` duplicate-key validation also derives its loop bound
  from `(int)*(short *)(param_1 + 4)`.
- `FUN_8006befc@8006befc` validates BEFF MLKs with `FUN_8006e5bc`, then uses
  either `FUN_8006e4cc` or `FUN_8006e640` to consume them in chunks.

Conclusion: the runtime evidence currently supports preserving the signed
halfword count as the observed count. The inferred count from first payload
offset should remain a diagnostic candidate for `d2403900.mlk`, not a parser
repair rule.

## rawWord12 Observations

The raw `record + 0x0c` field is still intentionally named `rawWord12`.

US top grouped values in `mlk_corpus_raw_word12_by_kind.csv`:

- BEFF normal MLD records: `16` appears 2839 times.
- BEFF normal MLD records: `4` appears 999 times.
- BEFF normal MLD records: `8` appears 531 times.
- BEFF normal MLD records: `28` appears 312 times.
- BCHARA normal MLD records: `16` appears 139 times.

The `first-payload-count-candidate` row for `d2403900.mlk` produces many
nonsensical `rawWord12` values after the plausible leading records. That is
consistent with scanning past the valid table shape when trusting the header
count literally.

## Embedded MLD Summary

The embedded MLD summary report now records:

- parse status and entry count
- diagnostic, warning, and error counts
- whether a texture archive was present
- object, ground, motion, and texture reference totals
- up to eight sampled function names per embedded MLD payload

US accepted embedded MLD totals:

- Parsed embedded rows: 5730
- Total MLD entries: 11322
- Object references: 780039
- Ground references: 472833
- Motion references: 817363
- Texture references: 119431

## Parser Contract

The next parser should stay read-only and conservative:

- Preserve header words and the signed `buffer + 0x04` count.
- Preserve the first-payload inferred count as evidence, not as replacement.
- Expose `tableShape`.
- Keep malformed records and diagnostics in the parsed result.
- Preserve `rawWord12` until runtime evidence explains it.
- Attach embedded MLD summary metadata without extracting payload binaries by
  default.
- Do not add an MLK writer yet.

Recommended next implementation step: introduce a read-only `MlkFile` model that
wraps the current scanner output and corpus-derived table interpretation, then
add real-file regression tests around the five BEFF anomaly shapes.
