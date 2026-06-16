# US MLD Handler Function Names

Date: 2026-06-15

Corpus: `D:\SoAGC\2002-12-19-gc-us-final_Skies_of_Arcadia_Legends`

Method:

- Exported entry-list JSON for every US `.mld` file with `SpiceFileParsing.exe --export-mld-entry-list-only`.
- Preserved source subdirectories in the local generated output under `SpiceFileParsing\parsed\us_mld_handler_entry_lists_v2`.
- Read each index entry `function` field from the exported JSON.
- Ignored blank function names.
- Ignored function names ending in `.nj`, using a case-insensitive suffix check.

Summary:

- MLD files scanned: 1,791
- Entry-list JSON files generated: 1,791
- Parsed index entries: 41,740
- Distinct non-`.nj` handler names: 154

The `Examples` column lists up to five source files where the handler appears.

| Handler | Entries | Files | Examples |
| --- | ---: | ---: | --- |
| advertise_exit | 2 | 2 | `field\a297a.mld`; `field\a299a.mld` |
| append | 1 | 1 | `field\a101b.mld` |
| common | 1 | 1 | `field\a111a.mld` |
| commonC | 26 | 15 | `field\a010a.mld`; `field\a033ab08.mld`; `field\a102a.mld`; `field\a102b.mld`; `field\a103ba14.mld` |
| commonilink | 162 | 75 | `field\a002aa09.mld`; `field\a002ba18.mld`; `field\a002da10.mld`; `field\a004aj12.mld`; `field\a005bc04.mld` |
| deformv | 7 | 4 | `field\a103a.mld`; `field\a103b.mld`; `field\a109e.mld`; `field\a250ak22d.mld` |
| doorwall | 334 | 106 | `field\a002a.mld`; `field\a002b.mld`; `field\a002c.mld`; `field\a002d.mld`; `field\a002e.mld` |
| drop | 1 | 1 | `field\a111b.mld` |
| e0200000_e020000jn.0 | 1 | 1 | `beff\e0200000.mld` |
| eff_camera | 136 | 34 | `field\a102a.mld`; `field\a102b.mld`; `field\a249al01_efe.mld`; `field\a513a.mld`; `field\a523a.mld` |
| Eff_FadeWipe | 53 | 46 | `field\a002aa13.mld`; `field\a002ba18.mld`; `field\a002da10.mld`; `field\a005ac24.mld`; `field\a013af15.mld` |
| eff_Grum | 919 | 70 | `field\a102b.mld`; `field\a106a.mld`; `field\a116g.mld`; `field\a116h.mld`; `field\a126dk20.mld` |
| Eff_LightLocus | 14 | 4 | `field\a115cf27_efe.mld`; `field\a249al01_efe.mld`; `field\a523a.mld`; `field\a577a.mld` |
| eff_man | 1270 | 457 | `field\a002aa13.mld`; `field\a002aa15_efe.mld`; `field\a002aa26.mld`; `field\a002b.mld`; `field\a002ba18.mld` |
| eff_motLight | 277 | 81 | `field\a017af36.mld`; `field\a017ag20.mld`; `field\a017hk04ef28.mld`; `field\a017ik04ef28.mld`; `field\a017xk04ef11.mld` |
| eff_polyBoard | 74 | 15 | `field\a013bf29_efe.mld`; `field\a112ce17_eff.mld`; `field\a116cg07ef03.mld`; `field\a201aa02_efe.mld`; `field\a202aa12.mld` |
| eff_polyBoard2 | 90 | 21 | `field\a017xk23ef_h.mld`; `field\a017xk23ef_i.mld`; `field\a099ak10eff.mld`; `field\a109e.mld`; `field\a123ci07_efe.mld` |
| eff_putmodel2 | 693 | 55 | `field\a002aa15_efe.mld`; `field\a002cc19_efe.mld`; `field\a017af17.mld`; `field\a017af18.mld`; `field\a017af19.mld` |
| eff_putmodel3 | 4982 | 261 | `field\a002aa26.mld`; `field\a013bf29_efe.mld`; `field\a017af17.mld`; `field\a017af18.mld`; `field\a017af34.mld` |
| eff_putmodelEx | 50 | 6 | `field\a130dl14a.mld`; `field\a250ak12_3.mld`; `field\a250al02eff2.mld`; `field\a250al12_3.mld`; `field\a299a02.mld` |
| eff_ropeact | 2 | 2 | `field\a102v.mld`; `field\a541a.mld` |
| Eff_ScreenFX1 | 2 | 2 | `field\a028ea07.mld`; `field\a250ak12_2.mld` |
| Eff_ShamLensFx | 12 | 3 | `field\a101b.mld`; `field\a103c.mld`; `field\a112c.mld` |
| eff_smoke | 28 | 2 | `field\a101ca05.mld`; `field\a111de12_efe.mld` |
| eff_smoke2 | 247 | 20 | `field\a002aa15_efe.mld`; `field\a028a.mld`; `field\a028a_e0.mld`; `field\a028a_e1.mld`; `field\a028b_e0.mld` |
| eff_smoke3 | 3793 | 277 | `field\a002b.mld`; `field\a002cc19_efe.mld`; `field\a008a.mld`; `field\a013bf29_efe.mld`; `field\a013df30_efe.mld` |
| eff_smoke3clip | 8 | 4 | `field\a130dl14a.mld`; `field\a131il18_efe.mld`; `field\a250al02eff2.mld`; `field\a250al02eff3.mld` |
| eff_sparcAnim | 213 | 80 | `field\a013bf29_efe.mld`; `field\a017xk23ef_h.mld`; `field\a017xk23ef_i.mld`; `field\a028ck13_efe.mld`; `field\a099ak10eff.mld` |
| eff_sparcLine | 450 | 91 | `field\a013bf29_efe.mld`; `field\a017xk23ef_h.mld`; `field\a017xk23ef_i.mld`; `field\a028b_e0.mld`; `field\a034h.mld` |
| eff_Sprite2D | 941 | 252 | `field\a002aa15_efe.mld`; `field\a002cc19.mld`; `field\a004ab05.mld`; `field\a004ab12.mld`; `field\a004aj12.mld` |
| eff_SS_Cntrl2 | 1 | 1 | `field\a249al01_efe.mld` |
| effSilverShild | 8 | 6 | `field\a249al01_efe.mld`; `field\a249al08.mld`; `field\a249al10.mld`; `field\a249al16efeA.mld`; `field\a249al16efeC.mld` |
| esahaiti | 38 | 36 | `field\a002d.mld`; `field\a002e.mld`; `field\a004a.mld`; `field\a008a.mld`; `field\a010a.mld` |
| eventhook | 37 | 15 | `field\a099a.mld`; `field\a099b.mld`; `field\a099c.mld`; `field\a099d.mld`; `field\a099e.mld` |
| fldAster | 87 | 32 | `field\fiel00a.mld`; `field\fiel10a.mld`; `field\fiel20a.mld`; `field\fiel20b.mld`; `field\fiel21a.mld` |
| fldAsterWall | 78 | 23 | `field\fiel00a.mld`; `field\fiel10a.mld`; `field\fiel20a.mld`; `field\fiel20b.mld`; `field\fiel30a.mld` |
| fldCloud2 | 48 | 16 | `field\a099a.mld`; `field\a099b.mld`; `field\a099c.mld`; `field\a099d.mld`; `field\a099e.mld` |
| fldColorsky | 16 | 16 | `field\a099a.mld`; `field\a099b.mld`; `field\a099c.mld`; `field\a099d.mld`; `field\a099e.mld` |
| fldColorsky2 | 16 | 16 | `field\a032a.mld`; `field\a033a.mld`; `field\a034a.mld`; `field\a034b.mld`; `field\a034c.mld` |
| fldEfcontrol | 16 | 16 | `field\a099a.mld`; `field\a099b.mld`; `field\a099c.mld`; `field\a099d.mld`; `field\a099e.mld` |
| fldFish1 | 67 | 44 | `field\fiel01a.mld`; `field\fiel03a.mld`; `field\fiel05a.mld`; `field\fiel05b.mld`; `field\fiel15a.mld` |
| fldFish2 | 171 | 67 | `field\fiel00a.mld`; `field\fiel01a.mld`; `field\fiel03a.mld`; `field\fiel05a.mld`; `field\fiel05b.mld` |
| fldFlycloud | 71 | 16 | `field\a099a.mld`; `field\a099b.mld`; `field\a099c.mld`; `field\a099d.mld`; `field\a099e.mld` |
| fldFogch | 16 | 16 | `field\a099a.mld`; `field\a099b.mld`; `field\a099c.mld`; `field\a099d.mld`; `field\a099e.mld` |
| fldGansyo | 87 | 33 | `field\fiel00a.mld`; `field\fiel10a.mld`; `field\fiel11a.mld`; `field\fiel11b.mld`; `field\fiel12a.mld` |
| fldGeneral | 202 | 43 | `field\fiel02a.mld`; `field\fiel14a.mld`; `field\fiel15a.mld`; `field\fiel22a.mld`; `field\fiel22b.mld` |
| fldGeneralAlpha | 73 | 9 | `field\fiel21a.mld`; `field\fiel22a.mld`; `field\fiel22b.mld`; `field\fiel22c.mld`; `field\fiel23a.mld` |
| fldGeneralAR | 1 | 1 | `field\fiel01a.mld` |
| fldGoscript | 2 | 2 | `field\fiel32g.mld`; `field\fiel36a.mld` |
| fldHakken | 164 | 73 | `field\fiel01a.mld`; `field\fiel02a.mld`; `field\fiel03a.mld`; `field\fiel04a.mld`; `field\fiel04b.mld` |
| fldHakken2 | 2 | 2 | `field\a099aa.mld`; `field\a122a.mld` |
| fldIsland | 628 | 92 | `field\fiel01a.mld`; `field\fiel02a.mld`; `field\fiel03a.mld`; `field\fiel04a.mld`; `field\fiel04b.mld` |
| fldLightch | 16 | 16 | `field\a099a.mld`; `field\a099b.mld`; `field\a099c.mld`; `field\a099d.mld`; `field\a099e.mld` |
| fldManshipA | 2 | 1 | `field\a099aa.mld` |
| fldManshipD | 16 | 1 | `field\a099aa.mld` |
| fldManshipE | 91 | 44 | `field\fiel02a.mld`; `field\fiel15a.mld`; `field\fiel20a.mld`; `field\fiel20b.mld`; `field\fiel30a.mld` |
| fldManshipF | 24 | 20 | `field\fiel02a.mld`; `field\fiel14a.mld`; `field\fiel21a.mld`; `field\fiel21b.mld`; `field\fiel21c.mld` |
| fldMoon2 | 96 | 16 | `field\a099a.mld`; `field\a099b.mld`; `field\a099c.mld`; `field\a099d.mld`; `field\a099e.mld` |
| fldMultiple | 2 | 2 | `field\a099c.mld`; `field\a099f.mld` |
| fldName | 87 | 58 | `field\fiel02a.mld`; `field\fiel03a.mld`; `field\fiel06a.mld`; `field\fiel06b.mld`; `field\fiel14a.mld` |
| fldRain | 32 | 16 | `field\a099a.mld`; `field\a099b.mld`; `field\a099c.mld`; `field\a099d.mld`; `field\a099e.mld` |
| fldSfilter | 136 | 16 | `field\a099a.mld`; `field\a099b.mld`; `field\a099c.mld`; `field\a099d.mld`; `field\a099e.mld` |
| fldShip | 22 | 22 | `field\a099a.mld`; `field\a099b.mld`; `field\a099c.mld`; `field\a099d.mld`; `field\a099e.mld` |
| fldSilverShild | 1 | 1 | `field\fiel32f.mld` |
| fldSky | 13 | 13 | `field\a028a.mld`; `field\a033b.mld`; `field\a101c.mld`; `field\a109a.mld`; `field\a109g.mld` |
| fldStorm1 | 121 | 53 | `field\fiel01a.mld`; `field\fiel02a.mld`; `field\fiel04a.mld`; `field\fiel05a.mld`; `field\fiel05b.mld` |
| fldTouch | 42 | 16 | `field\a099a.mld`; `field\a099b.mld`; `field\a099c.mld`; `field\a099d.mld`; `field\a099e.mld` |
| fldWave | 24 | 16 | `field\a099a.mld`; `field\a099b.mld`; `field\a099c.mld`; `field\a099d.mld`; `field\a099e.mld` |
| fscranim | 5 | 1 | `field\a103ba21.mld` |
| fstone | 15 | 5 | `field\a112b.mld`; `field\a121b.mld`; `field\a121c.mld`; `field\a121d.mld`; `field\a123b.mld` |
| goscript | 2579 | 188 | `field\a002a.mld`; `field\a002aa.mld`; `field\a002ab.mld`; `field\a002ac.mld`; `field\a002ad.mld` |
| ground | 1445 | 288 | `field\a002a.mld`; `field\a002aa.mld`; `field\a002ab.mld`; `field\a002ac.mld`; `field\a002ad.mld` |
| haikei_kumo-oku. | 3 | 3 | `beff\e0105401.mld`; `beff\e0800011.mld`; `beff\e0800031.mld` |
| haikei_kumo-temajn.e | 2 | 2 | `beff\e0800012.mld`; `beff\e0800032.mld` |
| haikei_tuki-ao.n | 2 | 2 | `beff\e0800014.mld`; `beff\e0800034.mld` |
| haikei1_tuki-aka | 2 | 2 | `beff\e0800013.mld`; `beff\e0800033.mld` |
| hakkenTexture | 89 | 89 | `field\hakken00.mld`; `field\hakken01.mld`; `field\hakken02.mld`; `field\hakken03.mld`; `field\hakken04.mld` |
| hasi | 1 | 1 | `field\a004a.mld` |
| hasigo1 | 62 | 37 | `field\a002a.mld`; `field\a002aa.mld`; `field\a002ae.mld`; `field\a002af.mld`; `field\a002ba.mld` |
| hasigo2 | 27 | 19 | `field\a002ae.mld`; `field\a002af.mld`; `field\a002be.mld`; `field\a002ce.mld`; `field\a002cf.mld` |
| hasigo3 | 35 | 21 | `field\a002a.mld`; `field\a002aa.mld`; `field\a002ba.mld`; `field\a002c.mld`; `field\a002ca.mld` |
| hikaritubu | 2 | 1 | `field\a122a.mld` |
| hload | 124 | 35 | `field\a002a.mld`; `field\a002b.mld`; `field\a002c.mld`; `field\a002d.mld`; `field\a002e.mld` |
| house | 218 | 114 | `field\a002a.mld`; `field\a002aa.mld`; `field\a002ab.mld`; `field\a002ac.mld`; `field\a002ae.mld` |
| housein1 | 127 | 28 | `field\a002a.mld`; `field\a002b.mld`; `field\a002c.mld`; `field\a002d.mld`; `field\a002e.mld` |
| housein2 | 30 | 18 | `field\a002d.mld`; `field\a002e.mld`; `field\a004a.mld`; `field\a010a.mld`; `field\a017b.mld` |
| kagi | 16 | 3 | `field\a002af.mld`; `field\a002cf.mld`; `field\a111a.mld` |
| kisiue | 76 | 3 | `field\a109b.mld`; `field\a109c.mld`; `field\a109d.mld` |
| kmap | 124 | 91 | `field\a002a.mld`; `field\a002b.mld`; `field\a002c.mld`; `field\a002d.mld`; `field\a002e.mld` |
| kyoriB | 34 | 1 | `field\a122a.mld` |
| linePut | 14 | 2 | `field\a109d.mld`; `field\a201a.mld` |
| lockfstn | 16 | 1 | `field\a112b.mld` |
| man | 2195 | 527 | `field\a002a.mld`; `field\a002aa.mld`; `field\a002aa09.mld`; `field\a002aa13.mld`; `field\a002aa15.mld` |
| modifi | 12 | 8 | `field\a002aa09.mld`; `field\a126dk17.mld`; `field\a127aj13.mld`; `field\a202aa12.mld`; `field\a204ad13.mld` |
| modifi2 | 6 | 2 | `field\a116e.mld`; `field\a127b.mld` |
| modlight | 1 | 1 | `field\a107d.mld` |
| moonfish | 32 | 29 | `field\a002a.mld`; `field\a002b.mld`; `field\a002c.mld`; `field\a002d.mld`; `field\a002e.mld` |
| motloop | 22 | 1 | `field\a103b.mld` |
| motmv | 2 | 1 | `field\a111a.mld` |
| motrand | 5 | 2 | `field\a005a.mld`; `field\a111b.mld` |
| motscpt | 2776 | 591 | `field\a002a.mld`; `field\a002aa09.mld`; `field\a002aa09_2.mld`; `field\a002aa15.mld`; `field\a002aa15_2.mld` |
| motscptCol | 8 | 4 | `field\a107d.mld`; `field\a116e.mld`; `field\a116h.mld`; `field\a126d.mld` |
| motscptuv | 2 | 2 | `field\a527a.mld`; `field\a578a.mld` |
| Mtrain | 2 | 2 | `field\a107d.mld`; `field\a209a.mld` |
| nametag | 172 | 86 | `field\a102a.mld`; `field\a102b.mld`; `field\a102c.mld`; `field\a102e.mld`; `field\a102v.mld` |
| NoPutfldIsland | 1 | 1 | `field\test.mld` |
| noritama | 14 | 3 | `field\a109b.mld`; `field\a109c.mld`; `field\a109d.mld` |
| objPlayerAct | 2 | 2 | `field\player.mld`; `player.mld` |
| onemotion | 9 | 2 | `field\a116h.mld`; `field\a126d.mld` |
| onepic | 18 | 5 | `field\a002a.mld`; `field\a002b.mld`; `field\a002c.mld`; `field\a017f_hf.mld`; `field\a398a.mld` |
| pchangemot | 975 | 117 | `field\a002a.mld`; `field\a002aa.mld`; `field\a002ab.mld`; `field\a002ac.mld`; `field\a002ad.mld` |
| pcharand | 311 | 11 | `field\a116g.mld`; `field\a130b.mld`; `field\a131a.mld`; `field\a131d.mld`; `field\a131e.mld` |
| pcharandever | 176 | 2 | `field\a106b.mld`; `field\a116g.mld` |
| playersub | 12 | 12 | `field\a008a.mld`; `field\a013a.mld`; `field\a013b.mld`; `field\a013c.mld`; `field\a017a.mld` |
| promi | 4 | 1 | `field\a109d.mld` |
| roll1 | 26 | 11 | `field\a002a.mld`; `field\a002af.mld`; `field\a002c.mld`; `field\a002cf.mld`; `field\a004a.mld` |
| rollax | 17 | 1 | `field\a122a.mld` |
| rollmot | 9 | 3 | `field\a034h.mld`; `field\a035b.mld`; `field\a239a.mld` |
| rope1 | 12 | 7 | `field\a002d.mld`; `field\a002e.mld`; `field\a010a.mld`; `field\a017e.mld`; `field\a121b.mld` |
| rope2 | 12 | 6 | `field\a002d.mld`; `field\a002e.mld`; `field\a010a.mld`; `field\a017e.mld`; `field\a121b.mld` |
| salenter | 17 | 1 | `field\a122a.mld` |
| salexit | 14 | 1 | `field\a122a.mld` |
| sb_arm | 18 | 3 | `field\a523a.mld`; `field\a547a.mld`; `field\a577a.mld` |
| sb_croot | 433 | 86 | `field\a102a.mld`; `field\a102b.mld`; `field\a102c.mld`; `field\a102e.mld`; `field\a102v.mld` |
| sb_enemy02 | 24 | 24 | `field\a102a.mld`; `field\a102b.mld`; `field\a506a.mld`; `field\a513a.mld`; `field\a515a.mld` |
| sb_fun | 276 | 46 | `field\a102c.mld`; `field\a102e.mld`; `field\a102v.mld`; `field\a102z.mld`; `field\a501a.mld` |
| sb_gun | 958 | 86 | `field\a102a.mld`; `field\a102b.mld`; `field\a102c.mld`; `field\a102e.mld`; `field\a102v.mld` |
| sb_lgun | 221 | 86 | `field\a102a.mld`; `field\a102b.mld`; `field\a102c.mld`; `field\a102e.mld`; `field\a102v.mld` |
| sb_other | 570 | 86 | `field\a102a.mld`; `field\a102b.mld`; `field\a102c.mld`; `field\a102e.mld`; `field\a102v.mld` |
| sb_point | 1402 | 86 | `field\a102a.mld`; `field\a102b.mld`; `field\a102c.mld`; `field\a102e.mld`; `field\a102v.mld` |
| sb_root | 866 | 86 | `field\a102a.mld`; `field\a102b.mld`; `field\a102c.mld`; `field\a102e.mld`; `field\a102v.mld` |
| sb_rudder | 132 | 48 | `field\a102c.mld`; `field\a102e.mld`; `field\a500a.mld`; `field\a501a.mld`; `field\a503a.mld` |
| sb_ship00 | 148 | 86 | `field\a102a.mld`; `field\a102b.mld`; `field\a102c.mld`; `field\a102e.mld`; `field\a102v.mld` |
| sb_tama00 | 1353 | 86 | `field\a102a.mld`; `field\a102b.mld`; `field\a102c.mld`; `field\a102e.mld`; `field\a102v.mld` |
| sb_tama01 | 586 | 86 | `field\a102a.mld`; `field\a102b.mld`; `field\a102c.mld`; `field\a102e.mld`; `field\a102v.mld` |
| sb_window | 389 | 81 | `field\a102a.mld`; `field\a102b.mld`; `field\a102c.mld`; `field\a102e.mld`; `field\a102z.mld` |
| sb_wing | 388 | 86 | `field\a102a.mld`; `field\a102b.mld`; `field\a102c.mld`; `field\a102e.mld`; `field\a102v.mld` |
| smoke1 | 31 | 15 | `field\a002a.mld`; `field\a002c.mld`; `field\a004a.mld`; `field\a004af.mld`; `field\a005b.mld` |
| stormgrd | 18 | 6 | `field\a099g.mld`; `field\a099h.mld`; `field\a099i.mld`; `field\a099j.mld`; `field\a099k.mld` |
| tamaoti | 2 | 2 | `field\a109b.mld`; `field\a109c.mld` |
| tamaue | 14 | 3 | `field\a109b.mld`; `field\a109c.mld`; `field\a109d.mld` |
| timingPmot | 2 | 1 | `field\a109e.mld` |
| train | 29 | 2 | `field\a107d.mld`; `field\a209a.mld` |
| trainPic | 281 | 2 | `field\a107d.mld`; `field\a209a.mld` |
| treasure | 139 | 52 | `field\a002a.mld`; `field\a002b.mld`; `field\a002c.mld`; `field\a002d.mld`; `field\a002e.mld` |
| ts900115 | 2 | 2 | `field\title0.mld`; `title\title0.mld` |
| ts900116 | 2 | 2 | `field\title0.mld`; `title\title0.mld` |
| ts900117 | 2 | 2 | `field\title0.mld`; `title\title0.mld` |
| ts900119 | 2 | 2 | `field\title0.mld`; `title\title0.mld` |
| ts900121 | 2 | 2 | `field\title0.mld`; `title\title0.mld` |
| wall | 2102 | 321 | `field\a002a.mld`; `field\a002aa09.mld`; `field\a002ahiru.mld`; `field\a002ayuu.mld`; `field\a002b.mld` |
| wallmot | 211 | 74 | `field\a002a.mld`; `field\a002c.mld`; `field\a004a.mld`; `field\a004ae.mld`; `field\a004af.mld` |
| walluv | 1498 | 344 | `field\a002a.mld`; `field\a002aa09.mld`; `field\a002ad.mld`; `field\a002ahiru.mld`; `field\a002ayuu.mld` |
| yuki | 4 | 1 | `field\a122a.mld` |
