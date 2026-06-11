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
```

Standalone `.gvr` image IR export writes lossless RGBA PNG files plus
`.gvr.json` sidecars. Import supports RGBA8, RGB5A3, CMPR, and CI4/RGB5A3 GVR output
through the sidecar `importTextureFormat` field, and supports `--gvr-aklz
preserve|compressed|raw`; `preserve` keeps AKLZ wrapping when the sidecar says
the source file was AKLZ-compressed.

MLD GVR format sampling writes `mld_gvr_format_inventory.json` and
`mld_gvr_format_priority_report.md` without raw texture payloads.

Reference materials and sample parser fixtures are under `soa_parser_reference_bundle/`.
