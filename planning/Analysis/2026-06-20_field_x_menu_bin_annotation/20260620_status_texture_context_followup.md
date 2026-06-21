# Field X-Menu BIN Texture Context Follow-Up

This note records the current answer to why the first-pass `HrsBin_Status.mll`
record contact sheet was wrong for many rows.

## Finding

The renderer assumed fixed-data word `0x00` was a direct ordinal into the
extracted GVR list from `HrsBin_Status.mll` member 4 (`ts0009.mld`). That is not
valid for the checked direct draw path.

`FUN_801d4f0c` passes records into `FUN_801d4f54` with `&DAT_80347568` as the
render context. `FUN_801d4a2c` and `FUN_801d4c9c` are sibling direct submitters
with the same important behavior: they iterate record elements, use the passed
render/context pointer, and do not read fixed-data word `0x00` as a texture
ordinal. In the direct submitter family, each element uses:

- `record + 0x04` as the element table pointer.
- `element + 0x00` as the fixed-data index.
- `record + 0x00 + fixedDataIndex * 0x14` as the fixed-data record.
- fixed-data offsets `0x04`, `0x08`, `0x0c`, and `0x10` as UV/source rectangle
  coordinates.
- element offsets `0x04..0x10` as destination coordinates.
- element offset `0x14` as tint/color input.

The checked direct draw paths do not read fixed-data word `0x00`. Texture or
material state is supplied from the active render context (`DAT_80347568`) and
the state reached through `FUN_80299040` / `FUN_80297dc8`.

The lower material path now explains why the first-pass contact sheet failed so
broadly. `FUN_80299040` records the active context pointer in `DAT_803457ec`.
The submitter then calls `FUN_80297dc8` with the material id reached from the
active context/material entry, not with the `.bin` fixed-data word. `FUN_80297dc8`
uses `FUN_802982f0` to scan the global material registry and, when the id is
found, binds the registered texture descriptor through `FUN_8029976c`.

Texture loads populate that same registry. `loadTextures_801db124` and
`FUN_801db244` iterate MLD texture blocks through `FUN_80227af0`; each texture
entry is registered with `FUN_80227d5c` using the current global material-id
counter or an explicit TKStringList id. `FUN_80227810` performs the corresponding
external-GVR path for TKStringList references. The default field/UI texture
context is a special case: `FUN_80226efc` and `FUN_80227068` set
`DAT_80347568 = &DAT_80311894` and register `/field/ts000110.gvr` with material
id `0x1869e`.

A follow-up material-context export found 485 references across 19 targets and
282 caller functions. In that pass, `DAT_80347568` had only two writes, both in
the default-context helpers above. `FUN_80298544` initializes the global material
registry pointer/count (`DAT_80347ed4`, `DAT_80347ed0`), and
`FUN_802982f0` scans entries with a 0x18-byte stride. Registry entry `+0x0c`
points at the material/texture descriptor whose first word is the material id
used by the direct submitters.

The registry initializer xref is now exported too. `FUN_80299078` is a small
engine setup wrapper:

- `FUN_80298544(param_1, param_2)` initializes the global 0x18-byte material
  registry and count.
- `FUN_8029833c(param_3, param_4)` initializes the companion context-entry
  storage.

Its caller is `FUN_801dc62c`, the main engine startup routine reached from
`main_801dcb28`. That routine allocates `DAT_803475ec` as 0x6000 bytes and
`DAT_803475e8` as 0x12000 bytes, then calls
`FUN_80299078(DAT_803475ec, 0x400, DAT_803475e8, 0x400)`. So the material
registry and context-entry storage are global engine tables with 0x400 declared
slots, not status-menu-local arrays.

The context slots used by `FUN_80298fe4` are 0x0c-byte entries. `FUN_80298a04`
links a context slot to a material by writing
`DAT_80347ed4 + materialIndex * 0x18` to `context[slot] + 8`; `FUN_80298fe4`
later reads exactly that pointer from `*DAT_803457ec + selector * 0x0c + 8`.
So a correct preview/export material manifest needs two mappings:

- material registry entry to source texture payload;
- active context entry index to material registry entry.

For direct submitters, the serialized record does not select the context entry;
the caller/context does. For queued submitters, fixed-data word `0x00` can select
a context entry, but only relative to the active context pointer set just before
drawing.

So the preview renderer's extracted-GVR ordinal shortcut skipped the real
runtime state: it used member-4 extraction order where the game uses an engine
material registry plus the currently active context.

The descriptor-submit path is different, but it still does not prove the
first-pass preview assumption. `FUN_801d52a8` is a wrapper into
`FUN_801d5a50`, and the descriptor family (`FUN_801d5a50`, `FUN_801d538c`,
`FUN_801d56c8`, and `FUN_801d5dd8`) calls `FUN_801d61f4` to build transient
0x14-byte descriptors. `FUN_801d61f4` copies fixed-data word `0x00` into
descriptor offset `0x10` as a short, while using fixed-data offsets `0x04`,
`0x08`, `0x0c`, and `0x10` for source coordinates. That makes word `0x00` a
preserved selector/material field for now, not an extracted-GVR ordinal.

This explains the visual pattern reported in the contact sheet:

- `StaDeco 0x02` can look correct because its source rectangle happens to line
  up with the embedded portrait-like `ts0009.mld` texture bank.
- `StaPaper 0x2c`, `StaPaper 0x40`, `StaSprite00 0x0c`, and similar common UI
  rows can look blank or nonsensical because their real texture state is not the
  extracted member-4 ordinal used by the first-pass preview renderer.

## Supporting Functions

| Function | Current evidence |
| --- | --- |
| `FUN_801d68c4` | Record accessor: returns `object.recordBase + object.recordOffsets[index]`. |
| `FUN_801d4f0c` | Wrapper that calls `FUN_801d4f54` and passes `&DAT_80347568`. |
| `FUN_801d4a2c` / `FUN_801d4c9c` / `FUN_801d4f54` | Direct quad submitters. Read fixed-data offsets `0x04..0x10` as UVs; do not read fixed-data `0x00` in the checked direct paths. |
| `FUN_801d52a8` / `FUN_801d5a50` | Descriptor-submit wrapper and implementation. `FUN_801d52a8` forwards to `FUN_801d5a50`; the implementation builds transient descriptors through `FUN_801d61f4`. |
| `FUN_801d538c` / `FUN_801d56c8` / `FUN_801d5dd8` | Other descriptor submitters that call `FUN_801d61f4` before submitting through the render queue/context path. |
| `FUN_801d61f4` / `FUN_801d6398` | Descriptor builders. They read fixed-data offsets `0x04..0x10` as scaled UV/source coordinates and copy fixed-data word `0x00` into the transient descriptor as a short. This supports preserving the word but does not make it an extracted-GVR ordinal. |
| `FUN_800ff348` | Queue/enqueue helper only. Copies a 0x1c-byte descriptor plus x/y/scale/flags into a render queue. It is not the texture binder. |
| `FUN_801db124` | MLD texture loader loop. Calls `GXInvalidateTexAll()` and `FUN_80227af0` per texture id. |
| `FUN_80227810` | Processes `TKStringList` texture references and can load external GVR files through `FUN_80227d5c`. |
| `FUN_8012ab84` | Loads `/field/HrsBin_sbp.mll`, then loads member texture banks 6, 5, and 4 and initializes members 0..3 as indexed BIN banks. This is a strong candidate source for common UI atlases used by status-menu rows. |
| `FUN_80226efc` | Loads `/field/ts000110.gvr`, sets `DAT_80347568 = &DAT_80311894`, and registers material id `0x1869e` through `FUN_80227d5c`. |
| `FUN_80227068` | Ensures the same default field texture context exists. It searches registered materials for id `0x1869e`, loads `/field/ts000110.gvr` when missing, and also sets `DAT_80347568 = &DAT_80311894`. |
| `FUN_80299040` | Stores the active context pointer in `DAT_803457ec`. This is context selection, not record-local texture lookup. |
| `FUN_80297dc8` | Resolves/binds a registered material entry through `FUN_802982f0` and `FUN_8029976c`. |
| `FUN_802982f0` | Scans the global material registry for the requested engine material id. |
| `FUN_8029976c` | Initializes and binds the registered GX texture object from the resolved registry descriptor. |
| `FUN_80227af0` / `FUN_80227d5c` | Register GVR/GVRT texture data from MLD texture blocks or external texture buffers into the engine material registry. |
| `FUN_80298544` | Initializes the global material registry pointer and count (`DAT_80347ed4`, `DAT_80347ed0`); entries are initialized as 0x18-byte slots. |
| `FUN_80299078` | Engine setup wrapper that initializes the material registry through `FUN_80298544` and the context-entry storage through `FUN_8029833c`. |
| `FUN_801dc62c` | Startup caller that allocates the global material/context buffers and calls `FUN_80299078(DAT_803475ec, 0x400, DAT_803475e8, 0x400)`. |
| `FUN_80298a04` | Links a context slot to a material registry entry by storing `DAT_80347ed4 + materialIndex * 0x18` at context entry `+0x08`. |
| `FUN_801c5228` | Generic MLL indexed-layout initializer. It loads member 4 textures, stores the texture string list, and records `&DAT_80347568` as the render context pointer for the initialized layout group. |

The `DAT_80347568` reference dump found two writes in this pass, both assigning
`&DAT_80311894`:

- `FUN_80226efc` at `80226f24`.
- `FUN_80227068` at `80227094`.

Other references pass `&DAT_80347568` as a parameter to draw/context helpers,
including `FUN_801d4f0c`, `FUN_801d49e4`, and `FUN_801d4c54`.

## Evidence Paths

- `SpiceBin/research/2026-06-20_field_hrsbin_menu_controller/ghidra_export/texture_submitter_audit/`
- `SpiceBin/research/2026-06-20_field_hrsbin_menu_controller/ghidra_export/texture_submitter_audit_2/`
- `SpiceBin/research/2026-06-20_field_hrsbin_menu_controller/ghidra_export/render_submitter_modes_deep/`
- `SpiceBin/research/2026-06-20_field_hrsbin_menu_controller/ghidra_export/render_submitter_801d52a8/`
- `SpiceBin/research/2026-06-20_field_hrsbin_menu_controller/ghidra_export/texture_descriptor_helpers/`
- `SpiceBin/research/2026-06-20_field_hrsbin_menu_controller/ghidra_export/texture_context_refs/`
- `SpiceBin/research/2026-06-20_field_hrsbin_menu_controller/ghidra_export/texture_context_functions/`
- `SpiceBin/research/2026-06-20_field_hrsbin_menu_controller/ghidra_export/material_context_trace/`
- `SpiceBin/research/2026-06-20_field_hrsbin_menu_controller/ghidra_export/material_registry_init_followup/`
- `SpiceBin/research/2026-06-20_field_hrsbin_menu_controller/material_context_trace_summary.md`
- `SpiceBin/research/2026-06-20_field_hrsbin_menu_controller/status_mll_record_contact_sheets/HrsBin_Status_high_confidence_keyed_contact_sheet_diagnostic.png`
- `SpiceBin/research/2026-06-20_field_hrsbin_menu_controller/status_mll_record_contact_sheets/HrsBin_Status_high_confidence_keyed_contact_sheet_diagnostic.tsv`

## Parser/Exporter Consequences

- Preserve fixed-data word `0x00` as raw/unknown for now.
- Do not expose fixed-data word `0x00` as an extracted-GVR ordinal.
- For editor/exporter naming, prefer `unknownWord0` or
  `materialSelectorWord` until the engine material-registry mapping is proven.
- Record previews must take an explicit runtime texture context or companion
  texture-bank mapping.
- `HrsBin_Status.mll` records should remain consumer-evidence-backed in
  `Docs/BinFileRecords.md`; preview images are not enough for row promotion.

## Load Order Follow-Up

The higher-level field load trace now explains why common status-menu rows can
depend on field/common UI atlases that are not embedded in `HrsBin_Status.mll`.

Normal field-load order from the current Ghidra exports:

1. `loadArea_801015ac` calls `loadScript_80101264`.
2. `loadScript_80101264` calls `FUN_8019282c`, which creates the
   `menu_listener@801920ac` child thread and sets `DAT_803473e8 = 0`.
3. `FUN_80101828` later reaches state `7`, runs initial scripts as needed, then
   calls `FUN_8012a39c`.
4. `FUN_8012a39c` performs field/stage setup and calls `FUN_8012aa44`.
5. `FUN_8012aa44` is a batch field UI/resource initializer. Its first call is
   `FUN_8012ab84`.
6. `FUN_8012ab84` loads `/field/HrsBin_sbp.mll`, loads member texture banks
   `6`, `5`, and `4`, calls `FUN_80227810` for each bank's texture string list,
   and initializes members `0..3` as indexed BIN banks.
7. After `FUN_8012a39c` returns, `FUN_80101828` sets `DAT_803473ec = 0` and
   `DAT_803473e8 = 1`.

`menu_listener@801920ac` checks `DAT_803473e8 == 1` before accepting the X-button
open request. Therefore, in the normal field entry path, `/field/HrsBin_sbp.mll`
is loaded before the X-menu can open and before `/field/HrsBin_Status.mll` is
requested by the listener. This makes `HrsBin_sbp.mll` a plausible provider for
common UI atlas/material state used by `StaPaper` and `StaSprite00` records, even
though the exact engine material-id-to-atlas mapping remains unresolved.

## State Callback Asset Audit

The local state/load map is:

- `SpiceBin/research/2026-06-20_field_hrsbin_menu_controller/xmenu_state_asset_load_map.md`

`FUN_80191ec8` creates and initializes the menu child, then calls
`FUN_80195f24` each frame while the child is active. `FUN_80195f24` runs common
status backing/deco rendering through `FUN_80195f9c`, then dispatches
`PTR_LAB_802ebbb4[DAT_80347110]`.

Current callback-table and `DAT_80347110` write audits support normal reachable
X-menu states `0..19`; entry `20` is null. Wider callback-table slices after
that null contain adjacent selector/control helper thunks and should not be
modeled as normal dynamic-load states without new runtime evidence.

Within the normal state callbacks, the only currently identified dynamic MLD
load after the status menu is open is state `19`, `FUN_80185c00`, the wanted
poster viewer. It loads `field/wanted_%02d{a,b}.mld` through `FUN_801db244`,
registers its `TKStringList` with `FUN_80227810`, and unregisters/frees it with
`FUN_802275a4(..., 1)` plus `memFree_proxy(DAT_803470d0)`. The other normal
state callbacks reuse the field/status material context established by
`HrsBin_sbp.mll`, `HrsBin_Status.mll` member 4, and the default field/UI
context.

## Remaining Question

The registry mechanism is now identified, but the exact visible texture-bank
mapping for failed previews is not fully proven yet. The next pass should build
or capture a concrete material-context manifest:

- material id to source texture asset;
- active context entry to material id;
- load-order provenance for `HrsBin_sbp.mll`, `HrsBin_Status.mll`, wanted MLDs,
  and default field textures.

That manifest can come from runtime capture of the registry after the relevant
field-menu loads, or from an offline reconstruction of MLD/TK load order that
preserves the same engine material ids.
