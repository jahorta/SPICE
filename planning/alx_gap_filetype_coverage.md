# SPICE Filetype Coverage Gaps vs ALX

## Purpose

This document summarizes the filetype inventory from the Skies of Arcadia Legends
GameCube EU disc dump at:

`D:\SoAGC\2003-03-05-gc-eu-final_Skies_of_Arcadia_Legends`

The long-term goal is for SPICE to handle every useful game data format that ALX
does not handle, while preserving ALX-compatible coverage where ALX is already a
good reference.

## Scope and Assumptions

For this analysis, "handled" means there is an end-to-end tool path that can
process files of that type from the disc dump. Low-level helpers, partial
decoders, or embedded parsing inside another format are called out separately.

SPICE currently has end-to-end top-level handling for:

- `.sct` via `SpiceSCT` and `SpiceFileParsing`
- `.mld` via `SpiceMLD` and `SpiceFileParsing`
- AKLZ decompression/recompression as a wrapper format for supported inputs

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

## Unsupported by Both SPICE and ALX

These are the file families that should be treated as primary SPICE expansion
targets if the goal is to cover everything ALX cannot.

| Extension | Count | First observed bytes | Initial interpretation |
| --- | ---: | --- | --- |
| `.mlk` | 577 | `AKLZ~?Qd=...` | AKLZ-wrapped character/effect link or archive data. |
| `.dsp` | 392 | Binary DSP header/data | GameCube DSP audio stream/sample data. |
| `.info` | 180 | Binary metadata | Sound metadata paired with `.samp`. |
| `.samp` | 179 | Binary sample payload | Sound sample bank/data paired with `.info`. |
| `.sst` | 136 | `AKLZ~?Qd=...` | AKLZ-wrapped battle data, likely ship/stat/script table adjacent. |
| `.sml` | 136 | `AKLZ~?Qd=...` | AKLZ-wrapped battle model/list data paired with `.sst`. |
| `.ect` | 35 | `AKLZ~?Qd=...` | AKLZ-wrapped field event/control table. |
| `.mll` | 27 | `AKLZ~?Qd=...` | AKLZ-wrapped model/list/archive data. |
| `.gvr` | 5 | `AKLZ~?Qd=...` | Standalone AKLZ-wrapped GVR texture files. |
| `.bin` | 5 | Mixed, some AKLZ | Specific `HrsBin*`, `HRSBin`, and `wmaparea` files not covered by ALX config. |
| `.tpl` | 2 | GameCube TPL header | Memory card banner/icon texture container. |
| `.EU` | 1 | `AFNT...` | Font data. |
| `.toc` | 1 | Binary metadata | GameCube filesystem/table-of-contents metadata. |
| `.ldr` | 1 | `2002/09/05...` | GameCube apploader/system file. |
| `.txt` | 1 | Text | Field list file; may be useful as metadata but not a binary parser target. |

`eudvd.bat` is also not handled, but it appears to be a build/script artifact
rather than a game data format.

## Partial or Borderline Coverage

### `.gvr`

SPICE has low-level GVR/GVM parsing in `SpiceGvm`, including `parseGvrTexture`
and `parseGvmArchive`, and `SpiceMLD` uses texture parsing while processing MLD
contents. The current `SpiceFileParsing` command-line flow does not process
standalone `.gvr` files from the disc dump.

Suggested next step: add a standalone `.gvr` top-level parse/export path that
uses the existing `SpiceGvm` parser after AKLZ decompression.

### AKLZ-Wrapped Unknowns

Most unsupported non-audio assets start with the AKLZ signature. Compression is
therefore not the blocker. The missing work is understanding each decompressed
payload format.

Known AKLZ-wrapped unsupported families from samples:

- `.mlk`
- `.std` in general, although ALX covers selected `bchara/m*.std` animation data
- `.sst`
- `.sml`
- `.ect`
- `.mll`
- `.gvr`
- some `.bin`

## External Handler Leads

This pass did not find public handlers that directly name the Skies-specific
extensions `.mlk`, `.sst`, `.sml`, `.ect`, or `.mll`. The best public leads are
for platform or middleware formats that overlap with the uncovered families.

### Strong Leads

| Target | Candidate reference | Why it matters |
| --- | --- | --- |
| `.dsp` | `vgmstream/vgmstream` | vgmstream supports Nintendo/GameCube DSP ADPCM variants and includes reusable header validation and decode logic in `src/meta/ngc_dsp_std.c` and `src/coding/ngc_dsp_decoder.c`. |
| `.gvr` / `.gvm` | `nickworonekin/puyotools` | PuyoTools lists GVM archives and GVR textures as supported formats, and has explicit `GvrFormat` / `GvmFormat` wrappers. |
| `.gvr` / `.gvm` / `.pvr` | `X-Hax/sa_tools` | Sonic Adventure tools include PVM/GVM/XVM texture archive handling and explicitly describe PVM archives used in Dreamcast games and GVM in GameCube ports. This is relevant because Skies moved related Sega/Ninja-era asset concepts from Dreamcast to GameCube. |
| `.tpl` | `soopercool101/BrawlCrate` | BrawlCrate/BrawlLib has TPL resource parsing/rebuilding and Wii/GameCube texture conversion code for I4, I8, IA4, IA8, RGB565, RGB5A3, CI4, CI8, CMPR, and RGBA8. |

Candidate URLs:

- https://github.com/vgmstream/vgmstream
- https://github.com/nickworonekin/puyotools
- https://github.com/X-Hax/sa_tools
- https://github.com/soopercool101/BrawlCrate

### Medium Leads

| Target | Candidate reference | Why it matters |
| --- | --- | --- |
| `.info` / `.samp` | `vgmstream/vgmstream` plus TXTH support | No direct Skies `.info`/`.samp` hit was found, but vgmstream has broad support for companion-file audio layouts and external format descriptions. These should be tested by building a TXTH or local probe against paired `.info`/`.samp` files. |
| `.mlk` / `.mll` | `X-Hax/sa_tools`, `Sa3Dport`, local MLD work | No direct handler found. These may still be model/link/archive relatives around MLD/Ninja assets, so Sonic Adventure/Ninja archive conventions are useful comparison material. |
| `.sst` / `.sml` | local reverse engineering first | No direct handler found. Because they pair by stem in `battle/` and are AKLZ-wrapped, the next step should be decompressed structure clustering and cross-reference from battle code/SCT/resource strings. |
| `.ect` | local reverse engineering first | No direct handler found. It should be investigated with field SCT references and decompressed payload signatures. |
| `FontData.EU` | local reverse engineering first | No direct handler found. The `AFNT` magic makes this look custom enough to treat as a separate font parser task. |
| `Game.toc` / `AppLoader.ldr` | Nintendo/GameCube system docs/tools | These are standard-ish system metadata files, useful for full-disc metadata but lower priority for game-data extraction. |

### Negative Search Results From This Pass

The following searches did not produce usable public handlers:

- `Skies of Arcadia file formats MLK SML SST ECT MLL`
- `Skies of Arcadia Legends .mlk .sml .sst file format`
- `Eternal Arcadia MLK SML SST ECT MLL file format`
- GitHub code searches for `mlk`, `sml`, `sst`, `ect`, and `mll` together with
  Skies/Eternal Arcadia terms

This does not prove no handlers exist. It does mean these formats should be
treated as local reverse-engineering targets until a more specific reference
turns up.

## Recommended Roadmap

### Phase 1: Corpus and Decompression Inventory

Create a repeatable inventory tool that walks a disc dump and emits:

- extension counts
- directory distribution by extension
- AKLZ vs raw count per extension
- decompressed size statistics
- first bytes after decompression
- likely magic tags or string signatures
- pair/group relationships by stem, such as `.info` plus `.samp`

This should become a checked-in tool or test fixture workflow, not a one-off
PowerShell command.

### Phase 2: Standalone Texture Coverage

Add end-to-end support for standalone `.gvr` files:

- detect AKLZ
- decompress
- parse with `SpiceGvm`
- export metadata JSON
- export decoded base texture as a common inspection format

This is the lowest-risk expansion because the low-level parser already exists.

### Phase 3: Audio Families

Investigate the `sound/` formats together:

- `.dsp`
- `.info`
- `.samp`

The goal should initially be metadata extraction and WAV/PCM inspection export,
not immediate re-import. These files are numerous and likely valuable for
content browsing, but they are separated from the current MLD/SCT pipeline.

### Phase 4: Battle Data Families Not Covered by ALX

Investigate:

- `.sst`
- `.sml`
- `HrsBin*.bin`

These are likely high-value because they live under `battle/` and are not part
of ALX's current known `first.lmt`, `ebinit/ecinit`, `epevent`, or enemy-task
coverage.

### Phase 5: Field Event and Metadata Adjacent Formats

Investigate:

- `.ect`
- `field\HRSBin.bin`
- `field\hrs_bend.bin`
- `field\wmaparea.BIN`
- `field\list2.txt`

This phase should cross-reference SCT strings and opcodes that name related
field resources.

### Phase 6: Model/Animation Linkage Families

Investigate:

- `.mlk`
- `.mll`
- any `.std` files outside ALX's supported animation interpretation

This should be coordinated with `SpiceMLD` and `Sa3Dport` work because these
formats may describe animation/linking/lookup relationships around model assets.

### Phase 7: UI/System Containers

Investigate lower-priority one-offs:

- `.tpl` memory-card icon/banner textures
- `FontData.EU`
- `Game.toc`
- `AppLoader.ldr`

These are useful for full-disc completeness but are probably less important
than gameplay, field, battle, audio, and model asset formats.

## Open Questions

- Are `.mlk` and `.mll` container/link formats around `.mld`, or independent
  model data families?
- Do `.sst` and `.sml` pair one-to-one by stem, and what game systems consume
  them?
- Are `.info` and `.samp` enough to reconstruct playable audio, or do they
  require separate lookup tables?
- Does `field\list2.txt` define a resource index useful for resolving `.ect`,
  `.sct`, `.mld`, and `.mll` relationships?
- Which unsupported formats are referenced directly by SCT opcode operands?

## Immediate Next Actions

1. Add a local inventory command under `tools/` that produces a JSON report for
   a disc dump.
2. Add a standalone `.gvr` parser/export path using existing `SpiceGvm` code.
3. Decompress representative files for `.mlk`, `.sst`, `.sml`, `.ect`, `.mll`,
   and unsupported `.bin` files, then record payload signatures and candidate
   structures.
4. Cross-reference SCT string/resource references against unsupported file
   stems to prioritize formats by gameplay reachability.

## Generated Inventory Workflow

The first implementation should use the dependency-free Python tool at
`tools/disc_inventory/spice_disc_inventory.py`:

```powershell
python tools\disc_inventory\spice_disc_inventory.py `
  --disc-root D:\SoAGC\2003-03-05-gc-eu-final_Skies_of_Arcadia_Legends `
  --out $env:TEMP\spice_disc_inventory
```

The generated outputs are analysis artifacts and should remain untracked unless
explicitly requested:

- `disc_inventory.json`
- `unsupported_payload_signatures.json`
- `sct_resource_references.json`
- `unsupported_format_analysis.json`
- `unsupported_priority_report.md`

The inventory workflow performs AKLZ decompression in memory for bounded
payload previews and SCT resource-reference matching. It does not write raw or
decompressed game payloads to disk.

### Enhanced Inventory Result

The enhanced Python inventory run against the EU final dump produced these
target-selection signals:

- `.sst` plus `.sml` form 136 one-to-one `battle/` stem pairs. Both families
  are AKLZ-wrapped in 135 of 136 files and each file has a distinct
  decompressed header prefix. The pairing makes this the best first real parser
  target because each parsed `.sst` can be validated against its `.sml`
  companion by stem.
- `.mlk` remains the largest unsupported family at 577 AKLZ-wrapped files, but
  its decompressed headers cluster into 363 prefixes. It exposes many Ninja-like
  strings such as `.nj`, `NJCM`, `NJTL`, and `POF0`, so it is likely important
  but less bounded as a first parser than the paired battle files.
- `.gvr` is the easiest standalone quick win: all 5 files share one
  decompressed header cluster and expose `GCIX` plus `GVRT`, matching the
  existing low-level `SpiceGvm` parser surface.
- SCT heuristic string extraction still produced zero direct references to
  unsupported file names. Deeper SCT semantic operand parsing is needed before
  SCT reachability can drive target ranking.
