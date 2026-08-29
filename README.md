# SPICE

SPICE is the Skies Package Interchange and Content Encoder. It provides
file-type libraries, reusable file operations, a command-line frontend, and a
Qt desktop frontend for Skies of Arcadia content tooling.

## Frontend architecture

- `SpiceRoot` contains low-level, file-type-neutral primitives used throughout
  the repository.
- `SpiceMix` contains reusable, non-Qt operations. It accepts typed requests,
  reports structured events, and supports cancellation without depending on a
  particular frontend.
- `SpiceGrinder` is the command-line frontend. It parses CLI arguments, runs a
  SpiceMix request, and renders operation events to the console.
- `SpiceRack` is the Qt desktop frontend. Its first workbenches inspect MLD
  documents and edit standalone or embedded GVR textures through SpiceMix.

SpiceGrinder and SpiceRack are separate executables. Running SpiceGrinder with
no arguments prints CLI help and does not launch the GUI.

## Scope

Included projects:

- Compression
- SpiceRoot
- SpiceMix
- SpiceGrinder
- SpiceRack
- SpiceGvm
- SpiceSCT
- SpiceMLD
- SpiceMll
- SpiceContentGraph
- SpiceTests
- Sa3Dport
- tools/sa3d_ref_runner
- third-party/SA3D.Modeling
- third-party/googletest-1.17.0

## Build

The full solution requires the MSVC v145 toolchain and the Qt VS Tools
registration `6.10.3_msvc2022_64` (Qt Core, Gui, and Widgets). Use the VS 18
MSBuild toolchain from the repo root:

```powershell
& "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe" SPICE.sln /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal
```

## Test

```powershell
.\x64\Debug\SpiceTests.exe
```

## CLI

Running with no arguments or with `--help` prints the command list. Inputs and
outputs are always explicit; use `<command> --help` for command-specific usage.

```powershell
.\bin\x64\Debug\SpiceGrinder.exe parse-sct --input <input_dir> --output <output_dir>
.\bin\x64\Debug\SpiceGrinder.exe export-mld-entry-list --input <input_dir> --output <output_dir>
.\bin\x64\Debug\SpiceGrinder.exe inventory-mld-gvr-formats --input <mld_input_dir> --output <output_dir>
.\bin\x64\Debug\SpiceGrinder.exe export-content-graph --input <input_dir> --output <output_dir> --projection sections
.\bin\x64\Debug\SpiceGrinder.exe export-gvr-image-ir --input <input_dir> --output <output_dir>
.\bin\x64\Debug\SpiceGrinder.exe import-gvr-image-ir --input <ir_dir> --output <output_dir> --aklz preserve
.\bin\x64\Debug\SpiceGrinder.exe create-gvr --input texture.png --output texture.gvr --format cmpr --mipmaps on
.\bin\x64\Debug\SpiceGrinder.exe replace-gvr --source original.gvr --input replacement.png --output texture.gvr
.\bin\x64\Debug\SpiceGrinder.exe replace-mld-texture --source source.mld --replacement replacement.png --output output.mld --texture-name tk000000 --format rgba8 --allow-dimension-change
.\bin\x64\Debug\SpiceGrinder.exe create-gvr-batch --input <png_dir> --output <gvr_out_dir> --format ci8 --palette-format rgb5a3
.\bin\x64\Debug\SpiceGrinder.exe replace-gvr-batch --input <png_dir> --source-gvr-dir <source_gvr_dir> --output <gvr_out_dir>
```

## GUI

Launch the desktop application with:

```powershell
.\bin\x64\Debug\SpiceRack.exe
```

SpiceRack uses closable document tabs. The MLD workbench exposes overview,
entry, texture, export, and diagnostic pages; GVR and PVR textures can be
replaced from PNG and staged across multiple entries before saving to a new MLD. The Exports
page writes Blender IR JSON, detailed entry-list JSON, or both from the current
document state, including staged changes. Standalone GVR and PVR workbenches can
open native textures or create new textures from PNG. Advanced GVR controls
expose format, palette, mipmap, global-index, and AKLZ wrapper choices; PVR
controls expose pixel format, data layout, and global index.
All texture workbenches use a shared viewport with crisp nearest-neighbor,
whole-number fit as the default. Linear sampling is available as an approximate
filtered preview, alongside explicit zoom and transparency-background controls.
Editing controls live in a resizable right sidebar. Job history stays collapsed
above the status bar until its arrow button is selected; warnings and errors
highlight that button without opening the history automatically.

For automated launch validation, `SpiceRack.exe --smoke-test` initializes the
main window and verifies the status/event toggle plus deterministic viewport
rendering. Supplying an MLD, GVR, or PVR path also waits for the background document
load and checks the loaded workbench; failures return a nonzero code:

```powershell
.\bin\x64\Debug\SpiceRack.exe --smoke-test <document.mld-gvr-or-pvr>
```

## Breaking project-name migration

This architecture is a clean break. Update project references, includes,
namespaces, and executable invocations using this mapping:

| Retired name | Replacement |
| --- | --- |
| `SpiceCore` | `SpiceRoot` |
| `SpiceFileParsing` executable/CLI | `SpiceGrinder` |
| reusable operation sources formerly inside `SpiceFileParsing` | `SpiceMix` |

Standalone `.gvr` image IR export writes lossless RGBA PNG files plus
`.gvr.json` sidecars. Import supports I4, I8, IA4, IA8, RGB565, RGB5A3, RGBA8,
CI4, CI8, CI14X2, and CMPR GVR output through the sidecar `importTextureFormat`
field. Indexed output supports IA8, RGB565, and RGB5A3 internal palettes through
`importPaletteFormat`. `--aklz preserve|compressed|raw` controls wrapping;
`preserve` keeps AKLZ wrapping when the sidecar says the source file was
AKLZ-compressed.

Sidecar-free GVR creation and replacement accept PNG input directly. New GVRs default
to RGBA8, no mipmaps, raw output, and no global index. Replacement preserves the
source GVR format, palette format, mipmap flag, AKLZ wrapping, and GCIX/global-index
value unless explicit `--format`, `--palette-format`, `--mipmaps`, `--aklz`, or
`--global-index` overrides are supplied.

MLD GVR format sampling writes `mld_gvr_format_inventory.json` and
`mld_gvr_format_priority_report.md` without raw texture payloads.

Embedded MLD texture replacement rebuilds the texture archive, so replacement
GVR payloads may grow or shrink. Select the target with `--texture-index` or
`--texture-name`; output preserves MLD AKLZ wrapping by default through
`--aklz preserve`. If an archive is not terminal, size-changing replacements
fail unless `--allow-post-archive-shift` is supplied.

Reference materials and sample parser fixtures are under `soa_parser_reference_bundle/`.
