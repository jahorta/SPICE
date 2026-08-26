# PVM/PVR Parser, Decoder, and Encoder Validation

## Scope

This analysis promoted the Dreamcast PVM/PVR contract needed by the current Skies of Arcadia corpus into the standalone `SpicePvm` library. No source game assets or generated images are stored in the repository.

Primary format evidence came from the local Dreamcast Kamui SDK format constants and Small VQ tables, then was checked against the current EU and US disc dumps. The neighboring local SA Tools implementation was used as a cross-check for PVMH flags, Morton addressing, VQ vector orientation, and mip ordering.

## Corpus selection

The representative EU set contains all `.MLD` files under `BATTLE`, `BCHARA`, `TITLE`, and `BEFF`, plus every standalone `.PVR` and `.PVM` below the EU Disc 1 asset root. The US compatibility set uses the same selection rule below the US Disc 1 asset root.

The validation is read-only. It scans each selected source for structurally valid `PVRT` chunks, parses each texture, decodes every logical mip, and hashes decoded RGBA bytes in deterministic relative-path/physical-offset order.

## Results

| Region | Source files | Textures | Decoded | Base pixels | All mip pixels | Base RGBA FNV-1a 64 | All-mip RGBA FNV-1a 64 |
| --- | ---: | ---: | ---: | ---: | ---: | --- | --- |
| EU Disc 1 | 318 | 2,721 | 2,721 | 51,023,872 | 62,187,457 | `5a32d9c1f3827de2` | `2377b8ec424356e4` |
| US Disc 1 | 294 | 2,144 | 2,144 | 35,449,088 | 43,445,633 | `c839867b5351df25` | `d2c63063e6312423` |

EU pixel formats: ARGB1555 412, RGB565 1,748, ARGB4444 561.

EU layouts: twiddled 296, twiddled mipmaps 6, VQ 360, VQ mipmaps 1,898, rectangle 10, Small VQ 74, Small VQ mipmaps 76, and DMA-prefixed twiddled mipmaps 1.

No selected texture used an unsupported tuple. All 2,721 previously inventoried EU `PVRT` chunks decoded successfully.

The encoding follow-up added deterministic output for every promoted pixel/layout tuple, formal PVMH writing, and Dreamcast MLD integration. Read-only MLD validation parsed and no-edit wrote 277 EU archives containing 2,319 PVR records and 263 US archives containing 2,113 PVR records. All 540 selected archives reproduced their exact source bytes.

## Promoted observations

- VQ-mipmap chunks in the SoA corpus pad the logical payload with zero bytes to a 32-byte boundary. The padding is inside the declared `PVRT` chunk size and is retained as a separate source range.
- Physical mip storage is smallest-to-largest, while the public result is largest-to-smallest.
- Layout `0x12` uses the SDK-documented six-byte prefix. Ordinary twiddled mipmaps use the two-byte 1x1 storage prefix.
- Small VQ codebook sizes differ between mipmapped and non-mipmapped textures and match the SDK dimension table.
- Dreamcast MLD texture records are not formal PVMH archives. They are a count plus 44-byte records followed by individual `GBIX`/`PVRT` blocks.
- MLD record control `0x80000000` aligns the following `GBIX` to an absolute 32-byte boundary. The leading alignment bytes are outside the record's declared encoded-block size.

## Remaining unknowns

The upper PVMH dimension byte, flag `0x0100`, and optional PVM metadata chunk semantics remain uninterpreted. They are preserved in the owning model. Unobserved PVR pixel/data identifiers are also retained raw but not decoded.
