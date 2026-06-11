# SPICE

SPICE is the Skies Package Interchange and Content Encoder. This first split preserves the existing Skies of Arcadia parser/content tooling behavior, including SCT parsing, MLD entry-list export, content graph export, and SA3D-backed geometry/Blender IR support.

## Scope

Included projects:

- Compression
- SpiceGvm
- SpiceSCT
- SpiceMLD
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

```powershell
.\bin\x64\Debug\SpiceFileParsing.exe <input_dir> <output_dir> --sct-only
.\bin\x64\Debug\SpiceFileParsing.exe <input_dir> <output_dir> --export-mld-entry-list-only
.\bin\x64\Debug\SpiceFileParsing.exe <mld_input_dir> <output_dir> --sample-mld-gvr-formats
.\bin\x64\Debug\SpiceFileParsing.exe <input_dir> <output_dir> --content-graph --content-graph-projection sections
.\bin\x64\Debug\SpiceFileParsing.exe <input_dir> <output_dir> --gvr-only --export-gvr-image-ir
.\bin\x64\Debug\SpiceFileParsing.exe <ir_dir> <output_dir> --gvr-only --import-gvr-image-ir --gvr-aklz preserve
.\bin\x64\Debug\SpiceFileParsing.exe --create-gvr texture.png texture.gvr --gvr-format cmpr --gvr-mipmaps on
.\bin\x64\Debug\SpiceFileParsing.exe --replace-gvr original.gvr replacement.png texture.gvr
.\bin\x64\Debug\SpiceFileParsing.exe --replace-mld-texture source.mld replacement.png output.mld --mld-texture-name tk000000 --gvr-format rgba8 --mld-allow-dimension-change
.\bin\x64\Debug\SpiceFileParsing.exe <png_dir> <gvr_out_dir> --gvr-only --create-gvr-batch --gvr-format ci8 --gvr-palette-format rgb5a3
.\bin\x64\Debug\SpiceFileParsing.exe <png_dir> <gvr_out_dir> --gvr-only --replace-gvr-batch <source_gvr_dir>
```

Standalone `.gvr` image IR export writes lossless RGBA PNG files plus
`.gvr.json` sidecars. Import supports I4, I8, IA4, IA8, RGB565, RGB5A3, RGBA8,
CI4, CI8, CI14X2, and CMPR GVR output through the sidecar `importTextureFormat`
field. Indexed output supports IA8, RGB565, and RGB5A3 internal palettes through
`importPaletteFormat`. `--gvr-aklz preserve|compressed|raw` controls wrapping;
`preserve` keeps AKLZ wrapping when the sidecar says the source file was
AKLZ-compressed.

Sidecar-free GVR creation and replacement accept PNG input directly. New GVRs default
to RGBA8, no mipmaps, raw output, and no global index. Replacement preserves the
source GVR format, palette format, mipmap flag, AKLZ wrapping, and GCIX/global-index
value unless explicit `--gvr-format`, `--gvr-palette-format`, `--gvr-mipmaps`,
`--gvr-aklz`, or `--gvr-global-index` overrides are supplied.

MLD GVR format sampling writes `mld_gvr_format_inventory.json` and
`mld_gvr_format_priority_report.md` without raw texture payloads.

Embedded MLD texture replacement rebuilds the texture archive, so replacement
GVR payloads may grow or shrink. Select the target with `--mld-texture-index` or
`--mld-texture-name`; output preserves MLD AKLZ wrapping by default through
`--mld-aklz preserve`. If an archive is not terminal, size-changing replacements
fail unless `--mld-allow-post-archive-shift` is supplied.

Reference materials and sample parser fixtures are under `soa_parser_reference_bundle/`.
