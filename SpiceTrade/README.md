# SpiceTrade

`SpiceTrade` is a static library for controlled interchange with external
game-data tools. Its initial compatibility target is the CSV dialect emitted
and consumed by ALX 5.0.0.

The library owns CSV interchange: ordered UTF-8 transport, typed editable
records for admitted tables, exact ALX 5.0.0 schemas, diagnostics, and change
tracking. It does not own display models, domain joins, or binary game formats.

The initial whitelist contains exactly:

- `enemy.csv`
- `enemyencounter.csv`
- `enemyevent.csv`

`EnemyCsvCodec`, `EnemyEncounterCsvCodec`, and `EnemyEventCsvCodec` parse those
files into ordered editable tables and serialize them back to CSV. Records can
be inserted, removed, or reordered; duplicate IDs are preserved. Bracketed ALX
reference/display columns are retained as strings so a read/edit/write cycle
does not discard them.

The codecs recognize the exact ALX 5.0.0 Japanese, US, and EU header layouts.
Locale and CSV formatting belong to read/write metadata, not to the semantic
table models. Validation is limited to whether values can be represented by
the ALX schema; it does not impose gameplay ranges or require references to
resolve across tables.

`AlxWorkspaceReader` loads any nonempty requested subset of the three tables
and fails the requested import as a unit. `AlxWorkspaceWriter` writes only
tables whose typed models changed, using their canonical filenames. The three
tables remain independently loadable and editable.

Other CSV families remain excluded until they are explicitly whitelisted as
gaps for which SPICE does not plan a native editor. CSVs representing formats
with native SPICE ownership remain with those projects. Script-task CSVs are
out of scope because SALSA owns that editing workflow.

ALX's GPLv3 Ruby source may be used as a behavioral reference and external
compatibility oracle, but its implementation is not copied or line-translated.
The user-provided `Alx v5.0.0 corpuses` directory contains reference exports
for all final Dreamcast and GameCube profiles. Tests require those files and
verify semantic typed round trips across all three whitelisted tables.
