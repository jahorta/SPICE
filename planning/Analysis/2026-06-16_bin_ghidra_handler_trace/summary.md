# BIN Ghidra Handler Trace

Date: 2026-06-16

## Question

Do loose HRSBin-style `.bin` files and MLL-embedded `.bin` members use the same indexed-table initializer and downstream runtime object layout?

## Answer

Yes for the indexed HRSBin-style family, with three entry points:

- `FUN_801d6998(object, payload)` initializes the indexed object from an already-loaded payload. MLL callers use this after extracting a member payload with `FUN_801e4984`.
- `FUN_801d6b58(object, path)` is the loose-file wrapper. It calls `loadFileFromPath(path)`, stores the returned payload in object slot `+0x10`, then runs the same count/offset-table setup and record fixups as `FUN_801d6998`.
- `FUN_801d6a74(object, cacheIndex)` is the battle-cache wrapper. It calls `FUN_8022aca8(cacheIndex)`, stores the returned battle-cache payload in object slot `+0x10`, then runs the same count/offset-table setup and record fixups as `FUN_801d6998`.

This proves the loose `field/hrs_bend.bin` path, battle-cache `HrsBinCW`/`HrsBinPCWin` paths, and MLL-embedded indexed `.bin` members share the same in-memory 0x14 object layout and same top-level record fixup behavior. Exporters should model one indexed layout family with separate load provenance, not separate loose-vs-MLL-vs-battle-cache schemas.

`field/wmaparea.BIN` is not part of this family. Its DOL callers use `loadFileFromPath` directly and either copy `0x7e0` bytes into a world-map state object or store the raw pointer for later area logic. It does not call `FUN_801d6998` or `FUN_801d6b58`, matching the corpus probe negative result.

## Runtime Object Layout

The shared indexed object is 0x14 bytes:

| Offset | Runtime field | Evidence |
| --- | --- | --- |
| `+0x00` | `recordBase` | Set to `payload + (count + 1) * 4`. |
| `+0x04` | `recordCount` | Loaded from payload `+0x00`. |
| `+0x08` | `recordOffsets` | Set to `payload + 4`. |
| `+0x0c` | `recordScratch` | Allocated as `count * 4` and zeroed. |
| `+0x10` | `payloadBase` | Set from caller payload in `FUN_801d6998`; set from `loadFileFromPath(path)` in `FUN_801d6b58`. |

For each top-level record, both initializers:

- Calculate `record = recordBase + recordOffsets[i]`.
- Overwrite record `+0x00` with `recordBase`.
- Rebase record `+0x04` by adding `payloadBase`.

`FUN_801d68dc(object)` is the matching free routine for loose loaded objects: it clears `+0x08`, frees `+0x0c`, frees `+0x10`, and clears those slots.

## Call-Path Findings

Direct xrefs to `FUN_801d6998`: 19 calls from 8 caller functions.

MLL member callers observed:

| Caller | Path/source | Member indices passed to `FUN_801d6998` |
| --- | --- | --- |
| `FUN_800c2f4c` | `/field/HrsBin_Hakken.mll` | 0, 1 |
| `GeneratedListUi::LoadFieldLnTestMll_800c3348` | `/field/ln_test.mll` | 0 |
| `FUN_800edb70` | `/field/hrs_wmap.mll` | 0, 1, 2 |
| `FUN_8012ab84` | `/field/HrsBin_sbp.mll` | 0, 1, 2, 3 |
| `FUN_801866b4` | `/field/wanted.mll` | 0, 1 |
| `FUN_801926b8` | path loaded earlier from `/field/HrsBin_Status.mll` | 0, 1, 2, 3 |
| `FUN_801bd644` | `/field/wanted.mll` | 0, 1 |
| `FUN_801c5228` | parameterized MLL path | 0, 1, 2, 3 |

Loose indexed caller observed:

| Caller | Path | Handler |
| --- | --- | --- |
| `FUN_800e3594` | `/field/HRS_BEND.BIN` | `FUN_801d6b58(state + 0x15f8, path)` |

Other loose strings:

- `/battle/hrsbincw.bin` and `/battle/hrsbinpcwin.bin` are traced through the
  battle preload/cache path, summarized below.
- No direct DOL string xref for `field/HRSBin.bin` was found in this string
  scan. The file exists in the US `field` directory and probes positive after
  AKLZ decode, but this pass did not identify a runtime loader path for it.

`wmaparea.BIN` callers:

| Caller | Path | Behavior |
| --- | --- | --- |
| `FUN_800ee51c` | `/field/wmaparea.bin` | Loads file, copies `0x7e0` bytes into `DAT_80346e14 + 0x47c`, frees loaded file. |
| `FUN_801a686c` | `/field/wmaparea.BIN` | Loads file, stores pointer at `*(param_1 + 0x24) + 0x204`, then computes/stores a short via `FUN_800ed8c4`. |

## Follow-up Trace: Three Loose Indexed Candidates

The follow-up traced `battle/HrsBinCW.bin`, `battle/HrsBinPCWin.bin`, and
`field/HRSBin.bin`.

### Battle HrsBinCW and HrsBinPCWin

The battle files are entries 1 and 2 in the 16-entry battle preload table rooted
at `PTR_s__battle_command_mld_802f91f8`:

| Index | Path |
| ---: | --- |
| 0 | `/battle/command.mld` |
| 1 | `/battle/hrsbincw.bin` |
| 2 | `/battle/hrsbinpcwin.bin` |
| 3 | `/battle/pcwindow.mld` |
| 4 | `/battle/btlcursor.mld` |
| 5 | `/bchara/damage.std` |
| 6 | `/bchara/common.std` |
| 7 | `/bchara/jouchu.mlk` |
| 8 | `/bchara/ma000.mld` |
| 9 | `/bchara/ma000.std` |
| 10 | `/bchara/ma001.mld` |
| 11 | `/bchara/ma001.std` |
| 12 | `/bchara/ma002.mld` |
| 13 | `/bchara/ma002.std` |
| 14 | `/bchara/ma003.mld` |
| 15 | `/bchara/ma003.std` |

`FUN_8022a854` walks this table. For each table entry it calls
`FUN_801cc67c(path)` and `FUN_801cc600(path, &DAT_80347868)`, stores the loaded
file size and file buffer in the per-entry cache state at
`DAT_803165c8 + index * 0x20`, copies the buffer to ARAM with
`ARQPostRequest`, frees the temporary file buffer, closes the file, and advances
to the next table entry until all 16 entries are cached.

`FUN_8022abec(name)` lowercases the requested name and scans those same 16 table
paths for a substring match. It returns the matching table index or
`0xffffffff`.

`FUN_8022aca8(index)` validates the battle cache state, allocates a RAM buffer
of the cached entry size, DMA-copies the cached ARAM entry back into that buffer,
and returns the RAM pointer.

Two battle resource request/load paths call that resolver:

| Caller | Evidence |
| --- | --- |
| `FUN_8006c124` | Calls `FUN_8022abec(requestName)`, then `FUN_8022aca8(index)` for cache hits. Later state 6 treats the returned buffer as a container: count at `+0x04`, 0x10-byte entries starting at `+0x08`, rebase entry `+0x04`, call `FUN_8006e244(entry, 0)`, then register the allocation span with `FUN_801e150c`. |
| `FUN_8006e00c` | Calls `FUN_8022abec(requestName)`, then `FUN_8022aca8(index)` for cache hits; if no cache entry is found it falls back to the normal file-load path. |

This proves the two battle `.bin` files are loaded and served through the battle
cache path. The separate battle consumer trace then showed the missing bridge:
`FUN_801d67dc` allocates two 0x14 indexed objects and calls
`FUN_801d6a74(DAT_80347628, 1)` for `battle/HrsBinCW.bin` and
`FUN_801d6a74(DAT_8034762c, 2)` for `battle/HrsBinPCWin.bin`. `FUN_801d6a74`
does the same count/offset-table setup and record fixups as `FUN_801d6998`.

The same setup function loads the likely companion texture/model resources:

| Global | Cache index | Path | Role |
| --- | ---: | --- | --- |
| `DAT_8034760c` | 0 | `/battle/command.mld` | Loaded immediately before `HrsBinCW`; likely the main command-window texture/model companion. |
| `DAT_80347628` | 1 | `/battle/hrsbincw.bin` | Indexed object for broad battle command/window UI layout. |
| `DAT_8034762c` | 2 | `/battle/hrsbinpcwin.bin` | Indexed object for post-battle PC result/status window UI layout. |
| `DAT_80347610` | 3 | `/battle/pcwindow.mld` | Loaded immediately after `HrsBinPCWin`; likely the PC result-window texture/model companion. |

### Field HRSBin.bin

`field/HRSBin.bin` exists in the US field directory and the corpus scan reports
it as AKLZ-wrapped and indexed-probe-positive:

| File | Raw | Decoded | AKLZ | Indexed probe |
| --- | ---: | ---: | --- | --- |
| `field/HRSBin.bin` | 1066 | 3876 | yes | yes |

The Ghidra decompile-term search found HRSBin string references only for MLL
paths such as `HrsBin_Hakken.mll`, `HrsBin_sbp.mll`, and
`HrsBin_Status.mll`. The direct DOL string search did not identify
`/field/HRSBin.bin`, and no runtime path to a loader was proven in this pass.

Current interpretation: `field/HRSBin.bin` is a real loose indexed candidate in
the corpus, but it may be unused/leftover, loaded by a constructed path, or
referenced indirectly through a path source that does not appear as a direct DOL
literal.

## Usage Hypotheses

The known `.bin` files may all be UI/menu-adjacent resources, even though the
binary families are not all the same.

| File or family | Hypothesized purpose |
| --- | --- |
| HRSBin-style indexed layout files | UI/render layout elements. |
| `field/HRSBin.bin` | Field status-menu UI elements used while moving around in-game. Runtime path not yet proven. |
| `battle/HrsBinCW.bin` | Confirmed battle command/menu/window UI layout: command windows, list rows, category/weapon/item/spell icons, cursor/arrow pieces, and party selection/status panels. Exact record names remain provisional. |
| `battle/HrsBinPCWin.bin` | Confirmed post-battle player-character result/status/level-up window layout: base panel, character-specific pieces, stat rows, gauge/bar records, status/element markers, and small status icons. Exact record names remain provisional. |
| `field/wmaparea.BIN` | World-map menu/area table, likely tied to the world-map UI accessible from field flow. Separate binary family from HRSBin-style indexed files. |

## Texture Trend Follow-up

A broader scan of the known loose `.bin` files and likely companion assets found
a consistent pattern:

- Decoded HRSBin-style loose `.bin` files contain no valid embedded GCIX/GVRT
  texture chunks.
- Their fixed-data tables contain texture-index-like first words and normalized
  float UV/source-rectangle fields. All fixed-data entries checked in
  `battle/HrsBinCW.bin`, `battle/HrsBinPCWin.bin`, `field/HRSBin.bin`, and
  `field/hrs_bend.bin` were UV-like.
- Nearby MLD/MLL assets contain the actual parsed GVR texture chunks and visual
  UI art.

Loose `.bin` trend:

| File | Embedded GVR chunks in decoded `.bin` | Fixed-data entries | UV-like fixed entries | Element records |
| --- | ---: | ---: | ---: | ---: |
| `battle/HrsBinCW.bin` | 0 | 301 | 301 | 239 |
| `battle/HrsBinPCWin.bin` | 0 | 63 | 63 | 60 |
| `field/HRSBin.bin` | 0 | 40 | 40 | 38 |
| `field/hrs_bend.bin` | 0 | 228 | 228 | 210 |
| `field/wmaparea.BIN` | 0 | n/a | n/a | n/a |

Likely companion texture assets:

| Asset | Parsed GVR chunks | Formats | Dimensions observed | Notes |
| --- | ---: | --- | --- | --- |
| `battle/command.mld` | 31 | CMPR, RGB5A3 | 16x16, 32x32, 64x64, 128x128 | Battle command labels, icons, numbers, windows, and menu pieces. |
| `battle/PCWindow.mld` | 23 | CMPR, RGB5A3 | 32x32, 64x64, 128x128 | Player names, portraits, HP/MP text, arrows, status icons, and window pieces. |
| `battle/btlcursor.mld` | 3 | RGB5A3 | 32x32 | Battle cursor/effect textures. |
| `field/HRSBin.mld` | 17 | CMPR, RGB5A3 | 32x32, 64x64, 128x128 | Field status/menu icons, portraits, arrows, text, and symbols. |
| `field/HrsBin_Hakken.mll` | 7 | CMPR, RGB5A3 | 64x64, 128x128, 256x256 | Discovery/status-style UI art. |
| `field/HrsBin_sbp.mll` | 57 | CMPR, RGB5A3 | 16x16, 32x32, 64x64, 128x128 | Battle/status-style UI art and window pieces. |
| `field/HrsBin_Status.mll` | 22 | CMPR | 64x64 | Character portrait/status art. |

Current interpretation: HRSBin-style `.bin` files are probably UI draw/layout
control data that reference companion texture slots and UV rectangles. The
actual sprite pixels live in companion MLD/MLL GVR texture chunks. `wmaparea.BIN`
still looks separate: it has no embedded GVR chunks and does not match the
indexed HRSBin-style probe.

## Runtime Value Usage Follow-up

A deeper read-only Ghidra search traced how initialized indexed `.bin` payload
values are consumed by rendering helpers.

### Record lookup

`FUN_801d68c4(object, index)` is a pure top-level record lookup:

- It returns `object.recordBase + object.recordOffsets[index]`.
- This makes the initialized object a small indexable record table. Callers
  select a record by semantic UI state, menu item, counter, party slot, or
  animation step, then pass that record into render helpers.

`FUN_801d68c4` is heavily reused: the range scan found 1225 call sites into this
lookup helper. This is why direct path tracing from one `.bin` file to one menu
screen is noisy: the same record-table lookup idiom is used across many UI and
effect systems.

### Descriptor conversion path

`FUN_801d61f4(record)` and `FUN_801d6398(record)` convert each record element
into 0x14-byte transient render descriptors:

- `record +0x08` is the element count.
- `record +0x04` points to the 0x34-byte element table.
- `record +0x00` points to the 0x14-byte fixed-data table.
- Each element `+0x00` selects `fixedDataTable + fixedDataIndex * 0x14`.
- Element `+0x04/+0x08/+0x0c/+0x10` are used as local rectangle coordinates.
  The converter stores width, height, and half extents as shorts.
- Fixed-data `+0x04/+0x08/+0x0c/+0x10` are scaled into source/UV-like short
  values.
- Fixed-data `+0x00` is copied into the descriptor as the texture/material
  selector.

Render wrappers such as `FUN_801d538c`, `FUN_801d56c8`, `FUN_801d5a50`,
`FUN_801d5dd8`, and `FUN_801d81c0` call these converters, allocate or reuse
transient 0x20-byte draw queue entries from `DAT_80347608`, scale
`element +0x14` color channels through `FUN_801d6528`/`FUN_801d65b4`, then pass
the descriptor to `FUN_80005d98`. `FUN_80005d98` emits positions, UVs, color,
texture selection, and a 4-vertex submit through `FUN_80291064`.

### Direct quad submit path

The `801c4xxx` helper family is the clearest direct use of the record schema.
Representative function `FUN_801c4074`:

- Iterates `record.elementCount`.
- Reads the element from `record.elementTablePointer + i * 0x34`.
- Uses `element.fixedDataIndex` to select a fixed-data record.
- Scales `element +0x14` packed color with `FUN_801d65b4`.
- Calls `FUN_801c4f24` to build a 4-vertex buffer:
  - Vertex positions are `baseX/baseY + element.left/top/right/bottom`.
  - UVs are copied from fixed-data `+0x04/+0x08/+0x0c/+0x10` by `FUN_801c5030`.
- Binds a companion texture through
  `FUN_80297dc8(**(*(textureList + fixedData.textureIndex * 0x0c + 8) + 0x0c))`.
- Submits the quad with `FUN_80291064(vertexBuffer, 4, flags)`.

This confirms that fixed-data `+0x00` is not arbitrary: in this path it is a
texture/material entry index into an MLD-like texture table supplied by the
caller. It also confirms the element coordinate fields and fixed-data UV fields
are directly used for quad rendering.

### Color interpretation

`FUN_801d65b4(alphaScale, colorScale, packedColor, outBytes)` and
`FUN_801d6528(alphaScale, colorScale, packedColor, outFloats)` unpack the four
bytes of `packedColor` as signed byte channels. The first byte is scaled by the
second scale argument in `FUN_801d65b4`, while the remaining three bytes are
scaled by the first scale argument. The downstream submitters rotate or pack the
resulting channels before sending them to the GX-style vertex path, so channel
names should remain provisional for now. The safe promoted meaning is
per-element packed color/alpha/tint.

### Current conclusion

For the indexed HRSBin-style family, the `.bin` payload values are used as
runtime UI quad layout data:

- top-level records group related drawable elements;
- record `+0x14/+0x18` provide mutable/base X/Y offsets in multiple render
  paths;
- element records describe destination rectangles and per-element tint/alpha;
- fixed-data records describe texture/material index plus source UV rectangle;
- actual image data comes from companion MLD/MLL texture assets, not the `.bin`
  file itself.

This directly supports the UI/menu hypothesis. It does not yet prove the exact
screen ownership for every loose file. `field/hrs_bend.bin` and MLL-embedded
HRSBin members are confirmed through these indexed render helpers. The separate
battle consumer trace, summarized below, confirms that the battle `.bin` files
also flow into the same indexed object shape through the battle-cache wrapper
`FUN_801d6a74`.

## Battle Consumer Trace

This follow-up focused on `DAT_80347628` and `DAT_8034762c`, the two globals
initialized from battle cache indices 1 and 2 by `FUN_801d67dc`.

### `battle/HrsBinPCWin.bin`

`DAT_8034762c` is the indexed object for `battle/HrsBinPCWin.bin`. The direct
consumer functions found in this pass are `FUN_80022f74`, `FUN_8002356c`, and
`FUN_80023fec`.

`FUN_80022f74` builds per-character result-window record arrays:

- It scans active player-character stats until a `-1` sentinel, with a maximum
  of four active PCs.
- It allocates `activePcCount * 0x70`, which is 28 cloned record pointers per
  active PC.
- It clones record 0 as a shared base/window record.
- It chooses a character-specific pair from records `0x22..0x27` and `1..6`:
  character 0 uses `0x22` and `1`, character 1 uses `0x23` and `2`, through
  character 5 using `0x27` and `6`. The default path falls back to `0x22` and
  `1`.
- It clones common records `0x14`, `0x12`, `0x13`, `0x1f`, `7..0x10`,
  `0x15..0x1e`, and `0x20`.
- In the selected-PC state it calls `FUN_8002356c` for only
  `selectedInstance_2`; in the multi-PC state it calls `FUN_8002356c` for every
  active PC.
- In state 2 it frees the cloned records with `FUN_801d4954`.

`FUN_8002356c` is the richer draw/update consumer:

- It uses record `0x21` as a dynamic gauge/bar record. The function mutates the
  element table behind the record, including element coordinate/color data,
  based on a computed ratio.
- It uses record `0x28` as a status/element marker and changes its element color
  at runtime from a byte in the PC data. Observed colors are `0xff20ff0a`,
  `0xffff4020`, `0xffc800ff`, `0xff0096ff`, `0xffffc800`, and fallback
  `0xffdcdcdc`.
- It draws small icons for bitflags in a status/equipment-style bit field.

`FUN_80023fec` maps a short value to one of records `7..0x10`, with `0xffff`
selecting record `0x11` and the default path selecting record 7. This looks like
a compact status/icon selector used by the PCWin renderer.

Useful PCWin record groups from the decoded metadata:

| Records | Metadata pattern | Current interpretation |
| --- | --- | --- |
| `0` | 15 elements, 138x144 at base 22/114, textures 0 and 4 | Main PC result/status window base. |
| `1..6` | Single 56-96x24 elements at base 61/126, texture 1 | Character-specific labels or name/title pieces. |
| `7..0x11` | Single 12x16 elements around base 143/172 or 96/172, texture 2 | Small numeric/status selector icons. |
| `0x12..0x20` | Small stat text/icon/gauge pieces, including record `0x1f` with six elements and texture 3 | Common result-window stat rows and gauge/line pieces. |
| `0x21` | 2x5 record at base 51/191 | Runtime-mutated gauge/bar record. |
| `0x22..0x27` | 32x32 character-specific records at base 27/119, textures 17..22 | Character-specific portrait/marker pieces. |
| `0x28` | 32x32 at base 27/119, texture 16, runtime color mutation | Status/element marker record. |

### `battle/HrsBinCW.bin`

`DAT_80347628` is the indexed object for `battle/HrsBinCW.bin`. The direct
consumer functions found in this pass are `FUN_80022b04`, `FUN_800240ec`,
`FUN_8002468c`, `FUN_800250f4`, `FUN_80025624`, `FUN_80026b20`,
`FUN_80026fec`, `FUN_80027484`, `FUN_800277f4`, and `FUN_80027c04`.

High-signal consumers:

- `FUN_80022b04` clones records `0x5f`, `0x60`, `0x62`, `99`, `0x61`, `100`,
  `0x65`, and later `0x66`. It uses a bitmask from the battle UI state,
  animates `DAT_80346ac4` with direction `DAT_80346ac8`, and offsets record
  `0x66` by selected command index times `0x18`. This looks like the main battle
  command selection strip and cursor/selection indicator.
- `FUN_800240ec` chooses command list/window layouts based on a command category
  byte. Categories 0..2 use compact records `0x2e` or `0x2f`; categories 3..8
  use a seven-entry layout with records `0x3e..0x43` plus `0x3b`; categories
  9..0xe use a five-entry layout with records `0x44`, `0x45`, `0x4b`, `0x4c`,
  plus `0x3b`. It also sets text/list coordinate globals.
- `FUN_8002468c` updates the active command row, chooses records
  `0x2d..0x31`, changes record `0x21` element color for category groups, draws
  arrow/cursor records `0x5c..0x5e`, and calls `FUN_800250f4`.
- `FUN_800250f4` draws command-list item text and category/item/equipment icons.
  Categories 3..8 map to records `0x4d..0x52`; category 0/10 maps to
  party/equipment-specific records `0x53..0x58`; category 9 uses `0x59`,
  category `0xb` uses `0x5a`, and category `0xc` uses `0x5b`.
- `FUN_80025624` is a large action/grid controller using records `9`, `10`,
  `0xb..0x12`, `0x15`, `0x1d..0x20`, and `0x32..0x39`. It calls both
  `FUN_80026fec` and `FUN_80026b20`.
- `FUN_80026b20` maps selection/category cases to action icon records
  `0x15..0x1c` and alternate/animated variants `0x32..0x39`.
- `FUN_80026fec` clones and draws records `8`, `0xb..0x14`, and `0x39`, while
  maintaining an animation counter up to 0xb.
- `FUN_80027c04` builds a party/character panel using records `0..8`,
  `0xb..0x12`, `0x23..0x2c`, and `0x67`. It loops active party stats, uses
  `selectedInstance_2`, and draws text through the same UI text path.

Useful CW record groups from consumer and decoded metadata:

| Records | Metadata pattern | Current interpretation |
| --- | --- | --- |
| `0..8` | Party-panel pieces, including record 5 as 115x71 at base 12/22 and record 7 as 480x27 at base 127/40 | Party/character status panel base and row pieces. |
| `0xb..0x14` | Mostly 40x40 icon-like records, shared base 151/316 | Action grid or status icon pieces. |
| `0x15..0x1c` and `0x32..0x39` | 52x52 and related animated/alternate icon records | Category/action icons and highlight variants. |
| `0x2d..0x31` | List-row/window variants | Current command-row layouts. |
| `0x3a..0x4c` | Command list panels, arrows, and varying-height windows | Command list/window layouts. |
| `0x4d..0x5b` | 50-80x24 label/icon records and category-specific records | Command category, item, equipment, and spell icons/labels. |
| `0x5c..0x5e` | Cursor/arrow records | List navigation indicators. |
| `0x5f..0x66` | Wide window/strip records and 24x44 selection pieces | Main command selection strip and selected-command indicator. |
| `0x67` | 224x46, 9 elements, textures 2 and 7 | Special party/status panel record used by `FUN_80027c04`. |

### Local evidence produced

Ignored evidence for this trace is under
`SpiceBin/research/2026-06-16_battle_bin_consumer_trace/`.

Key files:

- `decoded/HrsBinCW.decoded.bin`
- `decoded/HrsBinPCWin.decoded.bin`
- `battle_record_index_usage.csv`
- `battle_used_record_metadata.csv`
- `ghidra_export/battle_callback_terms/term_matches/801d67dc_loadStageTexturesAndAudioMaybe.c`
- `ghidra_export/battle_callback_terms/term_matches/80022b04_FUN_80022b04.c`
- `ghidra_export/battle_callback_terms/term_matches/80022f74_FUN_80022f74.c`
- `ghidra_export/battle_callback_terms/term_matches/8002356c_FUN_8002356c.c`
- `ghidra_export/battle_callback_terms/term_matches/800240ec_FUN_800240ec.c`
- `ghidra_export/battle_callback_terms/term_matches/8002468c_FUN_8002468c.c`
- `ghidra_export/battle_callback_terms/term_matches/800250f4_FUN_800250f4.c`
- `ghidra_export/battle_callback_terms/term_matches/80025624_FUN_80025624.c`
- `ghidra_export/battle_callback_terms/term_matches/80026b20_FUN_80026b20.c`
- `ghidra_export/battle_callback_terms/term_matches/80026fec_FUN_80026fec.c`
- `ghidra_export/battle_callback_terms/term_matches/80027c04_FUN_80027c04.c`

## Corpus Correlation

US loose corpus scan under `D:\SoAGC\2002-12-19-gc-us-final_Skies_of_Arcadia_Legends` found five loose `.bin` files:

| File | Raw | Decoded | AKLZ | Indexed probe |
| --- | ---: | ---: | --- | --- |
| `battle/HrsBinCW.bin` | 5233 | 21788 | yes | yes |
| `battle/HrsBinPCWin.bin` | 1606 | 5704 | yes | yes |
| `field/HRSBin.bin` | 1066 | 3876 | yes | yes |
| `field/hrs_bend.bin` | 4489 | 17508 | yes | yes |
| `field/wmaparea.BIN` | 2016 | 2016 | no | no |

This matches the runtime split: HRSBin-style loose files probe as indexed layouts; `wmaparea.BIN` does not.

## Structure Completion Follow-up

The structure completion pass focused on remaining parser/exporter confidence
questions: clone-time fields, `field/hrs_bend.bin`, `field/wmaparea.BIN`,
world-map indexed UI payloads, and regional loose-file validation.

### Runtime clone fields are not serialized element fields

The field helper exports show that the `801c36xx/801c37xx` helper family writes
runtime clone/control records, not serialized 0x34-byte `.bin` element records.

| Function | Observed fields | Current interpretation |
| --- | --- | --- |
| `FUN_801c3618` | Writes shorts at clone `+0x46` and `+0x44`. | Stores update/test state for runtime clones. |
| `FUN_801c3624` | Reads clone `+0x46`. | Boolean-ish test for active runtime clone state. |
| `FUN_801c368c` | Writes clone `+0x0c`, `+0x10`, `+0x14`, `+0x18`, `+0x42`, and `+0x48`. | Sets target coordinates/interpolation deltas and runtime state. |
| `FUN_801c377c` | Writes clone `+0x10`, `+0x18`, `+0x0c`, `+0x14`, `+0x42`, and `+0x48`. | One-axis variant. |
| `FUN_801c37d4` | Writes clone `+0x0c`, `+0x14`, `+0x10`, `+0x18`, `+0x42`, and `+0x48`. | One-axis variant. |

Parser/exporter conclusion: keep the serialized element schema at 0x34 bytes
with `+0x18..+0x30` preserved as unknown for now. Do not model clone offsets
`+0x42..+0x48` as file fields.

### `field/hrs_bend.bin`

`FUN_800e3594` loads `/field/HRS_BEND.BIN` through
`FUN_801d6b58(DAT_80346dcc + 0x15f8, path)` and frees it with
`FUN_801d68dc(DAT_80346dcc + 0x15f8)` during shutdown. This confirms
`field/hrs_bend.bin` is a loose indexed HRSBin-style payload.

Field functions such as `FUN_800e36f8`, `FUN_800e4644`, `FUN_800e6e58`,
`FUN_800e7240`, and `FUN_800e7934` heavily use the clone/interpolation helpers
above. `FUN_800e7934` directly fetches record `0x14` from the `hrs_bend`
indexed object through `FUN_801d68c4(DAT_80346dcc + 0x15f8, 0x14)` and copies
color data from its element table for runtime field effects.

Current interpretation: `field/hrs_bend.bin` is field-side UI/effect layout
control data used as templates for cloned runtime records. Its exact screen
label is still not stable, and its companion texture source is still open.

### `field/wmaparea.BIN`

`field/wmaparea.BIN` remains separate from the HRSBin-style indexed family.
`FUN_800ee51c` loads `/field/wmaparea.bin`, copies exactly `0x7e0` bytes into
`DAT_80346e14 + 0x47c`, and frees the file buffer. It then derives world-map
cursor/clamp state from `System::CurrentSkyMapMode_80310a4c` and
`StageWksht_PTR_80347450->position`.

`FUN_801a686c` loads `/field/wmaparea.BIN` into
`*(param_1 + 0x24) + 0x204` and stores the `FUN_800ed8c4()` result as a short
at `*(param_1 + 0x24) + 0x10`. `FUN_800ed8c4` maps current sky-map mode and
player/world position to an area id through `FUN_800edaa4`, then normalizes
special/out-of-range values.

Parser/exporter conclusion: `wmaparea.BIN` should be a distinct fixed
world-map area/menu table family. It should not be forced through the indexed
layout parser.

### World-map indexed UI payloads

The world-map setup function `FUN_800edb70` separately loads
`/field/hrs_wmap.mll`, extracts members 0, 1, and 2 with `FUN_801e4984`, and
initializes three 0x14 indexed objects with `FUN_801d6998` at state offsets
`DAT_80346e14 + 0xdf0`, `+0xdf4`, and `+0xdf8`.

This means the world-map flow has both:

- `field/wmaparea.BIN`: separate fixed area table; and
- `field/hrs_wmap.mll` members: indexed HRSBin-style UI/layout payloads.

### Regional validation

The structure completion pass ran `SpiceBinCorpusScan` against the known US, EU,
and JP dumps. All three scans found five loose `.bin` files, four plausible
indexed tables, and no decode errors or warnings.

| Region | Files | AKLZ files | Indexed positives | Notes |
| --- | ---: | ---: | ---: | --- |
| US | 5 | 4 | 4 | `wmaparea.BIN` is uncompressed/non-indexed. |
| EU | 5 | 4 | 4 | Same family split as US. |
| JP | 5 | 5 | 4 | `wmaparea.BIN` is AKLZ-compressed to raw size 454 but decodes to 2016 bytes. |

Indexed record counts:

| File | US | EU | JP | Structural note |
| --- | ---: | ---: | ---: | --- |
| `battle/HrsBinCW.bin` | 104 | 104 | 104 | Same record count; decoded sizes differ by region. |
| `battle/HrsBinPCWin.bin` | 41 | 41 | 41 | Same record count; JP decoded size and offsets differ. |
| `field/HRSBin.bin` | 34 | 34 | 34 | Scan metadata matches across regions. |
| `field/hrs_bend.bin` | 63 | 63 | 63 | Scan metadata matches across regions. |

Exporter implication: preserve regional payload data exactly. The parser can
share one indexed HRSBin-style schema across these regional loose files, but
export tests should include regional fixtures for `HrsBinCW` and JP `PCWin`.

## Raw Evidence

Ignored raw outputs are under:

- `SpiceBin/research/2026-06-16_bin_ghidra_handler_trace/ghidra_export/`
- `SpiceBin/research/2026-06-16_bin_ghidra_handler_trace/us_bin_corpus/`

Key generated files:

- `ghidra_export/export_summary.txt`
- `ghidra_export/initializer_xrefs.tsv`
- `ghidra_export/bin_string_matches.tsv`
- `ghidra_export/bin_string_xrefs.tsv`
- `ghidra_export/functions/initializer_801d6998_FUN_801d6998.c`
- `ghidra_export/additional_functions/801d6b58_FUN_801d6b58.c`
- `ghidra_export/additional_functions/801d68dc_FUN_801d68dc.c`
- `us_bin_corpus/bin_corpus_files.csv`
- `us_bin_corpus/bin_corpus_indexed_tables.csv`
- `ghidra_export/loose_three_strings/battle_path_table.tsv`
- `ghidra_export/loose_three_terms/decompile_term_counts.tsv`
- `ghidra_export/loose_three_terms/decompile_term_matches.tsv`
- `ghidra_export/loose_three_additional/8022a854_FUN_8022a854.c`
- `ghidra_export/loose_three_additional/8022aca8_FUN_8022aca8.c`
- `ghidra_export/functions/stringxref_8022abec_FUN_8022abec.c`
- `ghidra_export/loose_three_additional/8006c124_FUN_8006c124.c`
- `ghidra_export/loose_three_additional/8006e00c_FUN_8006e00c.c`
- `SpiceBin/research/2026-06-16_bin_value_usage/ghidra_export/hrs_helper_range_refs/range_refs.tsv`
- `SpiceBin/research/2026-06-16_bin_value_usage/ghidra_export/hrs_core_functions/801d61f4_FUN_801d61f4.c`
- `SpiceBin/research/2026-06-16_bin_value_usage/ghidra_export/hrs_core_functions/801d68c4_FUN_801d68c4.c`
- `SpiceBin/research/2026-06-16_bin_value_usage/ghidra_export/hrs_render_functions/801c4074_FUN_801c4074.c`
- `SpiceBin/research/2026-06-16_bin_value_usage/ghidra_export/hrs_render_functions/801c4f24_FUN_801c4f24.c`
- `SpiceBin/research/2026-06-16_bin_value_usage/ghidra_export/hrs_vertex_functions/801c5030_FUN_801c5030.c`
- `SpiceBin/research/2026-06-16_bin_value_usage/ghidra_export/hrs_vertex_functions/80005d98_FUN_80005d98.c`
- `SpiceBin/research/2026-06-16_battle_bin_consumer_trace/battle_record_index_usage.csv`
- `SpiceBin/research/2026-06-16_battle_bin_consumer_trace/battle_used_record_metadata.csv`
- `SpiceBin/research/2026-06-16_battle_bin_consumer_trace/decoded/HrsBinCW.decoded.bin`
- `SpiceBin/research/2026-06-16_battle_bin_consumer_trace/decoded/HrsBinPCWin.decoded.bin`
- `SpiceBin/research/2026-06-16_bin_structure_completion/README.md`
- `SpiceBin/research/2026-06-16_bin_structure_completion/ghidra_export/focused_functions/801c3618_FUN_801c3618.c`
- `SpiceBin/research/2026-06-16_bin_structure_completion/ghidra_export/focused_functions/801c368c_FUN_801c368c.c`
- `SpiceBin/research/2026-06-16_bin_structure_completion/ghidra_export/focused_functions/800e3594_FUN_800e3594.c`
- `SpiceBin/research/2026-06-16_bin_structure_completion/ghidra_export/focused_functions/800e7934_FUN_800e7934.c`
- `SpiceBin/research/2026-06-16_bin_structure_completion/ghidra_export/focused_functions/800ee51c_FUN_800ee51c.c`
- `SpiceBin/research/2026-06-16_bin_structure_completion/ghidra_export/focused_functions/800ed8c4_FUN_800ed8c4.c`
- `SpiceBin/research/2026-06-16_bin_structure_completion/ghidra_export/focused_functions/800edb70_FUN_800edb70.c`
- `SpiceBin/research/2026-06-16_bin_structure_completion/region_us/bin_corpus_files.csv`
- `SpiceBin/research/2026-06-16_bin_structure_completion/region_eu/bin_corpus_files.csv`
- `SpiceBin/research/2026-06-16_bin_structure_completion/region_jp/bin_corpus_files.csv`
- `SpiceBin/research/2026-06-16_battle_bin_consumer_trace/ghidra_export/battle_callback_terms/term_matches/801d67dc_loadStageTexturesAndAudioMaybe.c`
- `SpiceBin/research/2026-06-16_battle_bin_consumer_trace/ghidra_export/battle_callback_terms/term_matches/80022b04_FUN_80022b04.c`
- `SpiceBin/research/2026-06-16_battle_bin_consumer_trace/ghidra_export/battle_callback_terms/term_matches/80022f74_FUN_80022f74.c`
- `SpiceBin/research/2026-06-16_battle_bin_consumer_trace/ghidra_export/battle_callback_terms/term_matches/8002356c_FUN_8002356c.c`
- `SpiceBin/research/2026-06-16_battle_bin_consumer_trace/ghidra_export/battle_callback_terms/term_matches/80023fec_FUN_80023fec.c`
- `SpiceBin/research/2026-06-16_battle_bin_consumer_trace/ghidra_export/battle_callback_terms/term_matches/800240ec_FUN_800240ec.c`
- `SpiceBin/research/2026-06-16_battle_bin_consumer_trace/ghidra_export/battle_callback_terms/term_matches/8002468c_FUN_8002468c.c`
- `SpiceBin/research/2026-06-16_battle_bin_consumer_trace/ghidra_export/battle_callback_terms/term_matches/800250f4_FUN_800250f4.c`
- `SpiceBin/research/2026-06-16_battle_bin_consumer_trace/ghidra_export/battle_callback_terms/term_matches/80025624_FUN_80025624.c`
- `SpiceBin/research/2026-06-16_battle_bin_consumer_trace/ghidra_export/battle_callback_terms/term_matches/80026b20_FUN_80026b20.c`
- `SpiceBin/research/2026-06-16_battle_bin_consumer_trace/ghidra_export/battle_callback_terms/term_matches/80026fec_FUN_80026fec.c`
- `SpiceBin/research/2026-06-16_battle_bin_consumer_trace/ghidra_export/battle_callback_terms/term_matches/80027c04_FUN_80027c04.c`

Commands used:

```powershell
$env:USERPROFILE=(Resolve-Path .\tools\ghidra\.ghidra_userhome).Path
$env:APPDATA=(Resolve-Path .\tools\ghidra\.ghidra_appdata).Path
$env:LOCALAPPDATA=(Resolve-Path .\tools\ghidra\.ghidra_localappdata).Path
$env:JAVA_HOME='C:\Program Files\Java\jdk-21'
.\tools\ghidra\ghidra_11.4.3_PUBLIC\support\analyzeHeadless.bat .\tools\ghidra\ghidra_soal Skies_of_Arcadia_Legends -process US_jahorta_main.dol -readOnly -noanalysis -scriptPath .\tools\ghidra\ghidra_scripts -postScript export_bin_handler_trace.py SpiceBin\research\2026-06-16_bin_ghidra_handler_trace\ghidra_export 801d6998
.\tools\ghidra\ghidra_11.4.3_PUBLIC\support\analyzeHeadless.bat .\tools\ghidra\ghidra_soal Skies_of_Arcadia_Legends -process US_jahorta_main.dol -readOnly -noanalysis -scriptPath .\tools\ghidra\ghidra_scripts -postScript export_function_list.py SpiceBin\research\2026-06-16_bin_ghidra_handler_trace\ghidra_export\additional_functions 801d6b58 801d68dc 801d6c3c 801d66d4 801d668c 800e36f8 800e4644 800edfa8 800ee450 800ef0d8 800eefb0 800eeca8 800eedd4
.\x64\Debug\SpiceBinCorpusScan.exe D:\SoAGC\2002-12-19-gc-us-final_Skies_of_Arcadia_Legends SpiceBin\research\2026-06-16_bin_ghidra_handler_trace\us_bin_corpus
.\tools\ghidra\ghidra_11.4.3_PUBLIC\support\analyzeHeadless.bat .\tools\ghidra\ghidra_soal Skies_of_Arcadia_Legends -process US_jahorta_main.dol -readOnly -noanalysis -scriptPath .\tools\ghidra\ghidra_scripts -postScript dump_bin_candidate_strings.py SpiceBin\research\2026-06-16_bin_ghidra_handler_trace\ghidra_export\loose_three_strings
.\tools\ghidra\ghidra_11.4.3_PUBLIC\support\analyzeHeadless.bat .\tools\ghidra\ghidra_soal Skies_of_Arcadia_Legends -process US_jahorta_main.dol -readOnly -noanalysis -scriptPath .\tools\ghidra\ghidra_scripts -postScript search_decompile_terms.py SpiceBin\research\2026-06-16_bin_ghidra_handler_trace\ghidra_export\loose_three_terms FUN_8022abec PTR_s__battle_command_mld_802f91f8 hrsbincw hrsbinpcwin HRSBin HRSBIN
.\tools\ghidra\ghidra_11.4.3_PUBLIC\support\analyzeHeadless.bat .\tools\ghidra\ghidra_soal Skies_of_Arcadia_Legends -process US_jahorta_main.dol -readOnly -noanalysis -scriptPath .\tools\ghidra\ghidra_scripts -postScript export_function_list.py SpiceBin\research\2026-06-16_bin_ghidra_handler_trace\ghidra_export\loose_three_additional 8022aca8 8022a854 8022ae88 8006c124 8006e00c 8006e5bc 8006e4cc 8006e640 8006e244 801e150c
.\tools\ghidra\ghidra_11.4.3_PUBLIC\support\analyzeHeadless.bat .\tools\ghidra\ghidra_soal Skies_of_Arcadia_Legends -process US_jahorta_main.dol -readOnly -noanalysis -scriptPath .\tools\ghidra\ghidra_scripts -postScript export_address_range_ref_scan.py SpiceBin\research\2026-06-16_bin_value_usage\ghidra_export\hrs_helper_range_refs 801d6000 801d7000
.\tools\ghidra\ghidra_11.4.3_PUBLIC\support\analyzeHeadless.bat .\tools\ghidra\ghidra_soal Skies_of_Arcadia_Legends -process US_jahorta_main.dol -readOnly -noanalysis -scriptPath .\tools\ghidra\ghidra_scripts -postScript export_function_list.py SpiceBin\research\2026-06-16_bin_value_usage\ghidra_export\hrs_core_functions 801d61f4 801d6398 801d65b4 801d68c4 801d6944 801d6a74 801d6738 801d6528 801d665c 801d6850 801d6890 801d67dc 801d6998 801d6b58
.\tools\ghidra\ghidra_11.4.3_PUBLIC\support\analyzeHeadless.bat .\tools\ghidra\ghidra_soal Skies_of_Arcadia_Legends -process US_jahorta_main.dol -readOnly -noanalysis -scriptPath .\tools\ghidra\ghidra_scripts -postScript export_function_list.py SpiceBin\research\2026-06-16_bin_value_usage\ghidra_export\hrs_render_functions 801d52f4 801d538c 801d56c8 801d5a50 801d5dd8 801d81c0 801c4f24 801c4074 801c441c 801c45d4 801c47e4 801c4b88 801c4d00
.\tools\ghidra\ghidra_11.4.3_PUBLIC\support\analyzeHeadless.bat .\tools\ghidra\ghidra_soal Skies_of_Arcadia_Legends -process US_jahorta_main.dol -readOnly -noanalysis -scriptPath .\tools\ghidra\ghidra_scripts -postScript export_function_list.py SpiceBin\research\2026-06-16_bin_value_usage\ghidra_export\hrs_vertex_functions 801c5030 80291064 80297dc8 80299040 80288f54 80005d98
.\tools\ghidra\ghidra_11.4.3_PUBLIC\support\analyzeHeadless.bat .\tools\ghidra\ghidra_soal Skies_of_Arcadia_Legends -process US_jahorta_main.dol -readOnly -noanalysis -scriptPath .\tools\ghidra\ghidra_scripts -postScript export_function_list.py SpiceBin\research\2026-06-16_battle_bin_consumer_trace\ghidra_export\battle_bin_helpers 801d4954 801d497c 801d4a2c 801d4c9c 801d4f54 801d5340 801d52f4 801d538c 801d56c8 801d5a50 801d5dd8
.\bin\x64\Debug\SpiceFileParsing.exe --decompress-aklz D:\SoAGC\2002-12-19-gc-us-final_Skies_of_Arcadia_Legends\battle\HrsBinCW.bin .\SpiceBin\research\2026-06-16_battle_bin_consumer_trace\decoded\HrsBinCW.decoded.bin
.\bin\x64\Debug\SpiceFileParsing.exe --decompress-aklz D:\SoAGC\2002-12-19-gc-us-final_Skies_of_Arcadia_Legends\battle\HrsBinPCWin.bin .\SpiceBin\research\2026-06-16_battle_bin_consumer_trace\decoded\HrsBinPCWin.decoded.bin
.\tools\ghidra\ghidra_11.4.3_PUBLIC\support\analyzeHeadless.bat .\tools\ghidra\ghidra_soal Skies_of_Arcadia_Legends -process US_jahorta_main.dol -readOnly -noanalysis -scriptPath .\tools\ghidra\ghidra_scripts -postScript export_function_list.py SpiceBin\research\2026-06-16_bin_structure_completion\ghidra_export\focused_functions 800e3594 800e36f8 800e4644 800e6e58 800e7240 800e7934 800ed8c4 800ee51c 801a686c 800edb70 800edfa8 800ee450 800eeca8 800eedd4 800eefb0 800ef0d8 801c3618 801c3624 801c368c 801c377c 801c37d4 801c4074 801c4f24 801c5030 801db1a4 801db244 80227810
.\x64\Debug\SpiceBinCorpusScan.exe D:\SoAGC\2002-12-19-gc-us-final_Skies_of_Arcadia_Legends SpiceBin\research\2026-06-16_bin_structure_completion\region_us
.\x64\Debug\SpiceBinCorpusScan.exe D:\SoAGC\2003-03-05-gc-eu-final_Skies_of_Arcadia_Legends SpiceBin\research\2026-06-16_bin_structure_completion\region_eu
.\x64\Debug\SpiceBinCorpusScan.exe D:\SoAGC\2002-11-12-gc-jp-final_Eternal_Arcadia_Legends SpiceBin\research\2026-06-16_bin_structure_completion\region_jp
```

## Remaining Questions

- Choose stable semantic names for the battle CW and PCWin record groups now
  that their consumer roles are known.
- Find whether `field/HRSBin.bin` is unused/leftover, loaded by a constructed
  path, or referenced indirectly by a non-literal path source.
- Decode the per-entry schema of the `wmaparea.BIN` 0x7e0-byte world-map area
  table as a separate family.
- Identify the companion texture source for `field/hrs_bend.bin`.
- Trace how fixed-data texture indices map onto companion MLD/MLL texture slots
  in each runtime context, especially `field/hrs_bend.bin`.
- Add regional round-trip/export fixtures, at minimum for US/EU/JP
  `HrsBinCW`, US/JP `HrsBinPCWin`, shared `field/HRSBin.bin`, shared
  `field/hrs_bend.bin`, and non-indexed `wmaparea.BIN`.
- Name the indexed layout family and its 0x14 runtime object fields in `SpiceBin` once consumer naming is stable.
