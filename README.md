# SPICE

SPICE is the Skies Parser, Importer, and Content Extractor split from SOASim. This first split preserves the existing Skies of Arcadia parser/content tooling behavior, including SCT parsing, MLD entry-list export, content graph export, and SA3D-backed geometry/Blender IR support.

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

The SOASim runtime, DB, Qt, and Dolphin application code are intentionally not part of this repo.

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
.\bin\x64\Debug\SpiceFileParsing.exe <input_dir> <output_dir> --content-graph --content-graph-projection sections
```

Reference materials and sample parser fixtures are under `soa_parser_reference_bundle/`.
