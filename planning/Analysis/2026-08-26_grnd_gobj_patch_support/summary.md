# Dreamcast GRND/GOBJ Patch Support

## Result

SpiceMLD now retains exact source provenance for canonical GRND and GOBJ triangle metadata and can plan atomic, fixed-size patches for the decimal selector digit in the third stream word. The mechanism accepts digits 0 through 9 on every decoded Dreamcast GRND or GOBJ and deliberately applies no gameplay or area policy.

The representative Dreamcast validation covered EU and US Disc 1 copies of `A099A.MLD`, `A106A.MLD`, and `A106C.MLD`. All 39,550 decoded GRND triangles and 60 decoded GOBJ triangles had one-to-one source provenance. Both regional `A099A.MLD` files exposed a patchable GOBJ target. An additional 12 lexically ordered FIELD MLD files per region were checked for provenance cardinality.

Synthetic validation covers exact offsets, mixed GRND/GOBJ plans, selector digits 0 through 9, winding preservation, no-ops, low-15 overflow, duplicate and conflicting edits, stale models and bytes, incorrect resource shape, unsupported inputs, overlapping records, and atomic failure.

## Validation

- The complete Debug x64 solution builds with the v145 MSVC toolchain.
- All 14 focused parser, patching, and Dreamcast corpus tests pass.
- The complete `SpiceTests` run passes 290 of 293 tests. The three failures are the pre-existing unrelated `SpiceMllParser.ParsesNamedOffsetSizeMemberTable`, `SctIr.MetadataNamesKnownControlAndResourceOpcodes`, and `SctIr.BuilderEnrichesLegacyParseResults` failures.
- Corpus fingerprints: provenance `0860fb34e777f083`; planned patches `ef890738675cab0b`.

## Boundaries

- Physical in-place patching is limited to uncompressed little-endian Dreamcast MLD files.
- Only the decimal tens digit of the third triangle metadata word is editable through this API.
- GameCube/AKLZ output, arbitrary metadata-word editing, topology changes, file I/O, and editor behavior remain out of scope.
- Raw Dreamcast assets and patched outputs remain untracked.
