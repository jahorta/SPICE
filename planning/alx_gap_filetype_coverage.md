# SPICE Filetype Coverage Gaps vs ALX

## Purpose

This document summarizes filetype coverage for Skies of Arcadia Legends data
formats, using the original EU disc inventory as the baseline and the current
SPICE project layout as the status signal.

The long-term goal is for SPICE to handle every useful game data format that
ALX does not handle, while preserving ALX-compatible coverage where ALX is
already a good reference.

Baseline inventory source:

`D:\SoAGC\2003-03-05-gc-eu-final_Skies_of_Arcadia_Legends`

## Current Status Summary

Treat this as the current planning status, not just the original 2026-06 gap
snapshot.

### Stable or Active SPICE Surfaces

SPICE currently has direct project or tool surfaces for:

- `.sct` via `SpiceSCT` and `SpiceFileParsing`.
- `.mld` via `SpiceMLD` and `SpiceFileParsing`.
- `.gvr` / `.gvm` via `SpiceGvm`, including standalone GVR image IR,
  create/replace, and PNG export paths.
- `.ect` via `SpiceEct`.
- `.mlk` via `SpiceMlk` corpus scanning and Blender IR research export.
- `.mll` via `SpiceMll` parsing, corpus scanning, and safe archive export.
- `.sst` / `.sml` via `SpiceSstSml` read-only parsing and research exports.
- `.bin` via `SpiceBin` indexed-layout probing and corpus scanning.
- `.std` via `SpiceStd` usage inventory scanning.
- AKLZ decompression/recompression via `Compression`.

These are not all equally complete. Several are intentionally research or
read-only surfaces rather than writer/repack contracts.

### Remaining First-Look Targets

These file families still need a dedicated first SPICE investigation surface:

- `.dsp`, `.info`, `.samp`: the sound/audio family. These should be looked at
  together because `.info` and `.samp` appear paired, while `.dsp` is likely
  GameCube DSP ADPCM data.
- `FontData.EU`: custom-looking `AFNT` font data.
- `.tpl`: memory-card icon/banner texture containers.
- `Game.toc` and `AppLoader.ldr`: low-priority GameCube system metadata.
- `field/list2.txt`: likely resource metadata, useful for correlation but not
  a binary parser target.

## Scope and Assumptions

For this analysis, "handled" means there is an end-to-end or active research
tool path that can process files of that type from the disc dump. Low-level
helpers, partial decoders, and embedded parsing are called out separately.

ALX currently appears to cover these GameCube paths and formats:

- `&&systemdata/Start.dol`
- `opening.bnr`
- `&&systemdata/ISO.hdr`
- `battle/first.lmt`
- `battle/ebinit*.dat`
- `battle/ecinit*.dat`
- `field/*_ep.enp`
- `battle/epevent.evp`
- `field/*.sct`
- `bchara/m*.std`
- root `*.sot`
- `field/r*.tec`

ALX also exports/imports gameplay data from those files into CSVs, including
characters, enemies, enemy ships, skills, items, shops, script tasks, treasure
chests, string tables, and related progression tables.

## Disc Dump Extension Inventory

| Extension | Count | Sample paths |
| --- | ---: | --- |
| `.mld` | 1810 | `player.mld`, `battle\btlcursor.mld`, `field\a017a00.mld` |
| `.mlk` | 577 | `bchara\cr001.mlk`, `beff\...` |
| `.std` | 438 | `bchara\common.std`, `bchara\cr001.std` |
| `.dsp` | 392 | `sound\141_l.dsp`, `sound\141_r.dsp` |
| `.sct` | 258 | `field\me002a.sct`, `field\me017a.sct` |
| `.dat` | 245 | `battle\ebinit000.dat`, `battle\ecinit000.dat` |
| `.info` | 180 | `sound\b7000000.info`, `sound\cr000.info` |
| `.samp` | 179 | `sound\b7000000.samp`, `sound\cr000.samp` |
| `.sst` | 136 | `battle\s001.sst` |
| `.sml` | 136 | `battle\s001.sml` |
| `.tec` | 63 | `field\r500a.tec` |
| `.enp` | 51 | `field\a017a_ep.enp` |
| `.ect` | 35 | `field\a017a.ect` |
| `.mll` | 27 | `ending\sr.mll`, `field\...` |
| `.gvr` | 5 | `field\ts000110.gvr` |
| `.bin` | 5 | `battle\HrsBinCW.bin`, `field\wmaparea.BIN` |
| `.sot` | 4 | `english.sot`, `french.sot`, `german.sot`, `spanish.sot` |
| `.tpl` | 2 | `ea_mc_banner.tpl`, `ea_mc_icon.tpl` |
| `.EU` | 1 | `FontData.EU` |
| `.bnr` | 1 | `opening.bnr` |
| `.bat` | 1 | `eudvd.bat` |
| `.txt` | 1 | `field\list2.txt` |
| `.lmt` | 1 | `battle\first.lmt` |
| `.hdr` | 1 | `&&systemdata\ISO.hdr` |
| `.dol` | 1 | `&&systemdata\Start.dol` |
| `.toc` | 1 | `&&systemdata\Game.toc` |
| `.ldr` | 1 | `&&systemdata\AppLoader.ldr` |
| `.evp` | 1 | `battle\epevent.evp` |

`eudvd.bat` appears to be a build/script artifact rather than a game data
format.

## Active But Incomplete SPICE Areas

### `.bin`

`SpiceBin` exists and currently probes the HRSBin-style indexed UI/render layout
family. The `.bin` extension is not one proven single format. Known loose and
embedded files include indexed UI layouts and the separate `field/wmaparea.BIN`
world-map table family.

Current next work:

- Finish per-entry schema work for `field/wmaparea.BIN`.
- Resolve whether loose `field/HRSBin.bin` is unused/leftover or loaded through
  a constructed path.
- Keep `Docs/BinFileLayout.md` and `Docs/BinFileRecords.md` as the durable
  shared status.

### `.sst` / `.sml`

`SpiceSstSml` exists and is the active read-only battle-stage parser/research
surface. The paired battle files now have documented top-level structure,
command blocks, command-map exports, embedded SML MLD extraction, and combined
Blender IR export with SST placement overlays.

Current next work:

- Continue runtime/visual naming of command semantics.
- Keep exporter/repack behavior out of scope until command effects and runtime
  links are better proven.

### `.mlk`

`SpiceMlk` exists and handles corpus scanning plus combined Blender IR research
exports for MLK-contained model/effect resources.

Current next work:

- Keep `rawWord12` unresolved until runtime evidence identifies it.
- Classify BEFF anomalies without forcing them into the normal record-table
  shape.
- Do not introduce a writer/repack contract yet.

### `.mll`

`SpiceMll` exists and covers the supported table-shaped container layout,
payload classification, corpus scanning, and safe archive export.

Current next work:

- Continue member payload classification, especially MLD-like payloads, indexed
  BIN members, and texture-bank relationships.
- Keep stronger payload semantics in member-specific projects instead of making
  MLL own every embedded format.

### `.ect`

`SpiceEct` exists and parses flat and indexed encounter-table layouts with AKLZ
handling. It should not be counted as a first-look gap anymore.

Current next work:

- Keep regional validation current.
- Add wider tool integration only when encounter-table workflows need it.

### `.gvr`

`SpiceGvm` now covers standalone GVR image IR, PNG export, and GVR
create/replace paths. It should not be counted as a first-look gap anymore.

Current next work:

- Treat new work as format-polish or image-encoding validation rather than a
  first investigation.

### `.std`

`SpiceStd` exists as the initial `.std` usage inventory surface. It scans real
`.std` files without claiming a full schema, records AKLZ status, decoded size,
header fingerprints, directory/name buckets, and the ALX-covered
`bchara/M*.std` family.

The initial EU baseline scan produced:

| Usage bucket | Files | AKLZ | Decode errors | ALX-covered pattern |
| --- | ---: | ---: | ---: | ---: |
| `bchara_m_family` | 386 | 386 | 0 | 386 |
| `bchara_common` | 1 | 1 | 0 | 0 |
| `bchara_damage` | 1 | 1 | 0 | 0 |
| `bchara_character_resource` | 44 | 44 | 0 | 0 |
| `other_directory` | 6 | 6 | 0 | 0 |

Current next work:

- Use the output to decide which `.std` subgroup deserves the first parser.
- Add finer prefix grouping for the broad `bchara_m_family` bucket if the
  follow-up investigation needs MA/MB/etc. counts.
- Correlate `bchara_character_resource` and the six `other_directory` files
  against runtime loaders.

## Remaining First-Look Families

### Audio: `.dsp`, `.info`, `.samp`

The sound formats are the largest untouched family after `.std`.

Initial SPICE milestone:

- Investigate `.dsp`, `.info`, and `.samp` together.
- Start with metadata extraction and WAV/PCM inspection export, not re-import.
- Use `vgmstream` as the first reference lead for GameCube DSP and companion
  audio layouts.

### `FontData.EU`

The `AFNT` magic suggests a custom font format.

Initial SPICE milestone:

- Create a small font-specific probe only after higher-value gameplay and asset
  formats are covered.
- Prioritize glyph table discovery, dimensions, encoding, and texture/bitmap
  storage.

### `.tpl`

The two `.tpl` files are memory-card banner/icon texture containers.

Initial SPICE milestone:

- Treat as a small texture-container task.
- Use GameCube/Wii TPL references such as BrawlLib/BrawlCrate for format
  comparison.

### `Game.toc` / `AppLoader.ldr`

These are system metadata files and lower priority for game-data extraction.

Initial SPICE milestone:

- Only investigate when full-disc metadata export needs them.

### `field/list2.txt`

This is probably a resource index or metadata list rather than a binary parser
target.

Initial SPICE milestone:

- Correlate it against SCT, ECT, MLD, MLL, and field resource names before
  treating it as its own format.

## Updated Roadmap

### Phase 1: `.std` Usage Inventory

Use `SpiceStd` to drive the conservative usage inventory for real `.std` files.
The first output should help answer:

- Which name/path groups exist?
- Which are ALX-covered `bchara/m*.std` files?
- Which groups are AKLZ-wrapped?
- Which decoded header clusters repeat?
- Which groups should be parser targets?

### Phase 2: Audio Family Probe

Investigate `.dsp`, `.info`, and `.samp` as a single sound-system family.

### Phase 3: Finish Active Research Surfaces

Continue the already-open work for `.bin`, `.sst/.sml`, `.mlk`, and `.mll`.

### Phase 4: UI/System One-Offs

Investigate `.tpl`, `FontData.EU`, `Game.toc`, `AppLoader.ldr`, and
`field/list2.txt` after the higher-value gameplay, battle, model, UI-layout, and
audio surfaces are in better shape.

## External Handler Leads

| Target | Candidate reference | Why it matters |
| --- | --- | --- |
| `.dsp` | `vgmstream/vgmstream` | vgmstream supports Nintendo/GameCube DSP ADPCM variants and includes reusable header validation and decode logic. |
| `.info` / `.samp` | `vgmstream/vgmstream` plus TXTH support | No direct Skies handler is known, but vgmstream has broad companion-file audio support. |
| `.tpl` | `soopercool101/BrawlCrate` | BrawlLib has TPL parsing/rebuilding and GameCube/Wii texture conversion code. |
| `.std` | ALX plus local Ghidra/runtime evidence | ALX's selected `bchara/m*.std` handling is the best starting reference, but broader `.std` usage needs local evidence. |

Candidate URLs:

- https://github.com/vgmstream/vgmstream
- https://github.com/soopercool101/BrawlCrate

## Open Questions

- Which `.std` filename groups are actually consumed by distinct runtime
  handlers?
- Are non-`m*.std` files animation data, character metadata, effect linkage, or
  separate table formats?
- Are `.info` and `.samp` enough to reconstruct playable audio, or do they
  require separate lookup tables?
- Does `field/list2.txt` define a resource index useful for resolving `.ect`,
  `.sct`, `.mld`, and `.mll` relationships?
- Which remaining unsupported formats are referenced directly by SCT opcode
  operands or by field/battle resource loader tables?

## Inventory Workflow

The dependency-free Python inventory tool is still useful for broad disc
counts:

```powershell
python tools\disc_inventory\spice_disc_inventory.py `
  --disc-root D:\SoAGC\2003-03-05-gc-eu-final_Skies_of_Arcadia_Legends `
  --out $env:TEMP\spice_disc_inventory
```

Generated outputs are analysis artifacts and should remain untracked unless
explicitly requested:

- `disc_inventory.json`
- `unsupported_payload_signatures.json`
- `sct_resource_references.json`
- `unsupported_format_analysis.json`
- `unsupported_priority_report.md`

The tool's historical "unsupported" naming predates several current SPICE
projects. Use this document's status sections when deciding what is still
unstarted.
