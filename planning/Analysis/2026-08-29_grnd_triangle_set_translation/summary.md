# GRND Triangle-Set Translation Correction

## Result

The GRND triangle-set header floats at `+0x00`, `+0x04`, and `+0x08` are now modeled as a set-local-to-resource translation. Source-view vertices remain stored-local, while canonical GRND mesh positions add the translation exactly once. Normals are unchanged. The separate inner-header X/Z collision-grid origins are also promoted.

This correction occurs in `GrndParser`, so canonical resources, projected world/search meshes, and Blender IR all receive the same resource-local geometry. It is not an A103B- or display-specific adjustment. MLD index-entry transforms remain separate whole-resource transforms.

## Writer Contract

Unedited GRND resources continue to copy their exact source bytes. When semantic edits require a rebuild, the writer canonicalizes the translated mesh into one zero-translation triangle set and writes the modeled grid origins. This preserves placement even when the source used several translated triangle sets.

## Validation

- Big- and little-endian synthetic GRNDs decode translation and grid origins identically.
- Synthetic source-view vertices remain local, canonical positions are translated once, and normals are unchanged.
- The US GameCube `A103B.MLD` lower floor resolves to Y 80, the entry-7 ramp resolves from Y 80 through Y 136, and the upper floor resolves to Y 136.
- Canonical, projected WorldModel, and Blender IR entry-7 bounds agree.
- EU and US Dreamcast `A103B.MLD` files contain 72 nonzero translated sets covering 628 declared triangles; both no-edit writes remain byte-identical.
- The focused 17-test GRND/writer/patching suite passes. Dreamcast selector-provenance and patch fingerprints remain unchanged.
- The complete Debug x64 solution builds successfully with the v145 MSVC toolchain.
- The complete `SpiceTests` run passes 325 of 328 tests. The three failures are the existing unrelated baseline failures: `SpiceMllParser.ParsesNamedOffsetSizeMemberTable`, `SctIr.MetadataNamesKnownControlAndResourceOpcodes`, and `SctIr.BuilderEnrichesLegacyParseResults`.

No raw game assets or generated geometry are tracked.
