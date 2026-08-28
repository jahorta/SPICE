# SPICE

SPICE is the Skies Package Interchange and Content Encoder. This first split preserves the existing Skies of Arcadia parser/content tooling behavior, including SCT parsing, MLD entry-list export, content graph export, and SA3D-backed geometry/Blender IR support.

## Scope

Included projects:

- Compression
- SpiceGvm
- SpiceSCT
- SpiceMLD
- SpiceMll
- SpiceContentGraph
- SpiceTests
- SpiceFileParsing
- Sa3Dport
- tools/sa3d_ref_runner
- third-party/SA3D.Modeling
- third-party/googletest-1.17.0

## Build

Use the VS 18 MSBuild toolchain from the repo root:

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
.\bin\x64\Debug\SpiceFileParsing.exe parse-sct --input <input_dir> --output <output_dir>
.\bin\x64\Debug\SpiceFileParsing.exe export-mld-entry-list --input <input_dir> --output <output_dir>
.\bin\x64\Debug\SpiceFileParsing.exe inventory-mld-gvr-formats --input <mld_input_dir> --output <output_dir>
.\bin\x64\Debug\SpiceFileParsing.exe export-content-graph --input <input_dir> --output <output_dir> --projection sections
.\bin\x64\Debug\SpiceFileParsing.exe export-gvr-image-ir --input <input_dir> --output <output_dir>
.\bin\x64\Debug\SpiceFileParsing.exe import-gvr-image-ir --input <ir_dir> --output <output_dir> --aklz preserve
.\bin\x64\Debug\SpiceFileParsing.exe create-gvr --input texture.png --output texture.gvr --format cmpr --mipmaps on
.\bin\x64\Debug\SpiceFileParsing.exe replace-gvr --source original.gvr --input replacement.png --output texture.gvr
.\bin\x64\Debug\SpiceFileParsing.exe replace-mld-texture --source source.mld --replacement replacement.png --output output.mld --texture-name tk000000 --format rgba8 --allow-dimension-change
.\bin\x64\Debug\SpiceFileParsing.exe create-gvr-batch --input <png_dir> --output <gvr_out_dir> --format ci8 --palette-format rgb5a3
.\bin\x64\Debug\SpiceFileParsing.exe replace-gvr-batch --input <png_dir> --source-gvr-dir <source_gvr_dir> --output <gvr_out_dir>
```

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
