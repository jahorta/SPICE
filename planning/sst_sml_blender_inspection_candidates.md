# SST/SML Blender Inspection Candidates

Created: 2026-06-13

This is the current ranked shortlist of `.sml` / `.sst` battle-stage pairs that
look useful to inspect in Blender. The set is capped at 10 stages and is biased
toward US files because the Ghidra-backed schema work is US-primary.

Use the US dump first:

`D:\SoAGC\2002-12-19-gc-us-final_Skies_of_Arcadia_Legends\battle`

Generated Blender/export artifacts should stay under ignored research outputs.
Living per-stage annotation JSON and media should stay under:

`SpiceSstSml\research\results\state_annotations\<stem>\`

## Ranking Criteria

- Known visual/research value from prior annotations.
- Command type coverage, especially rare or semantically unresolved types.
- SML record count and decoded embedded MLD size.
- Model-link count from SST commands to SML records.
- Preference for stages that are likely to expose different behavior, rather
  than only selecting the largest stages.

## Ranked Set

| Rank | Pair | Why It Is Interesting | US Corpus Metadata |
| ---: | --- | --- | --- |
| 1 | `s062.sml` / `s062.sst` | Best current annotated stage. Shows addressable overlay/fallback geometry: central/ring layout, nubs that can be disabled, and entry `0` fallback mesh coverage. Strong command diversity with type `3`, `8`, and `9`. | `33` records; command counts `{0:33, 1:1, 3:21, 8:17, 9:17}`; model links `55/55`; decoded SML size `1832008`. |
| 2 | `s006.sml` / `s006.sst` | Large raw/not-AKLZ stage already known to contain embedded motion animation. Useful for validating combined Blender IR, animation carry-through, and type `8` attachments. | `28` records; command counts `{0:28, 1:1, 8:9}`; model links `9/9`; decoded SML size `2846288`. |
| 3 | `s044.sml` / `s044.sst` | Annotated damaged-homebase stage with type `8`, `9`, and rare type `10` in the same stage. Entry `0` is confirmed as the floor after the MLD parser fix; fire-plane entries give good type `8`/`9` targets. The elevator remains unexplained, so this is still useful for checking already-loaded resources or type `10`-adjacent behavior. | `22` records; command counts `{0:22, 1:1, 8:10, 9:10, 10:1}`; model links `21/21`; decoded SML size `2501128`. |
| 4 | `s038.sml` / `s038.sst` | Similar high-value type `8`/`9`/`10` mix to `s044`, with a different stage and almost identical command density. Useful as a comparison stage for type `10`. | `21` records; command counts `{0:21, 1:1, 8:10, 9:10, 10:1}`; model links `21/21`; decoded SML size `2540612`. |
| 5 | `s086.sml` / `s086.sst` | Heavy repeated type `8`/`9` stage with many resolved model links. Useful for identifying repeated visual effect patterns and whether multiple commands point to shared SML entries. | `25` records; command counts `{0:25, 1:1, 8:22, 9:11}`; model links `33/33`; decoded SML size `3370024`. |
| 6 | `s031.sml` / `s031.sst` | Same command shape as `s086`, but a separate stage. Good comparison candidate to separate general type `8`/`9` behavior from stage-specific layout. | `25` records; command counts `{0:25, 1:1, 8:22, 9:11}`; model links `33/33`; decoded SML size `2973900`. |
| 7 | `s053.sml` / `s053.sst` | Type `3`-heavy stage with some type `9`. Useful for correlating type `3` child/linked geometry behavior against visible repeated structures. | `22` records; command counts `{0:22, 1:1, 3:30, 9:6}`; model links `36/36`; decoded SML size `2193088`. |
| 8 | `s008.sml` / `s008.sst` | One of only three known type `2` stages, and it also includes type `3` and `9`. Good target for visually checking the current type `2` sine/displacement hypothesis. | `8` records; command counts `{0:8, 1:1, 2:1, 3:1, 9:3}`; model links `5/5`; decoded SML size `643100`. |
| 9 | `s021.sml` / `s021.sst` | Only known type `11` case. Small enough for focused inspection, and useful for checking whether the rare target-vector/ramp behavior has a visible stage element. | `4` records; command counts `{0:4, 1:1, 11:1}`; model links `1/1`; type `11` consumer windows `1`; decoded SML size `498096`. |
| 10 | `s014.sml` / `s014.sst` | Compact mixed type `3`/`4` stage. Included specifically to get type `4` visual coverage instead of another large type `8`/`9` stage. | `16` records; command counts `{0:16, 1:1, 3:1, 4:2}`; model links `3/3`; decoded SML size `1311576`. |

## Deferred Alternates

These are worth keeping nearby, but are not in the first 10-stage batch:

- `s002`: compact rare type `10` case; useful if `s044`/`s038` are too busy.
- `s017` and `s018`: the other two known type `2` stages.
- `s005` and `s015`: smaller type `8`/`9` stages that may be easier to inspect
  than `s031`, `s086`, or `s062`.

## Suggested Export Command Shape

For one stage copied into a small input folder with same-stem `.sml` and `.sst`:

```powershell
.\bin\x64\Debug\SpiceFileParsing.exe <input_dir> <output_dir> --export-sml-embedded-mld --export-sml-combined-blender-ir --export-sst-sml-command-map
```

The combined Blender IR goes to `<output_dir>\<stem>\<stem>_combined_blender_ir_scene.json`.
When combined IR is exported, a copy is also placed beside the living annotation
JSON as
`SpiceSstSml\research\results\state_annotations\<stem>\<stem>_combined_blender_ir_scene.json`.
The command map goes to `<output_dir>\<stem>\<stem>.sst_sml_command_map.json`.
The living annotation document goes to
`SpiceSstSml\research\results\state_annotations\<stem>\<stem>.stage_annotation.json`.
