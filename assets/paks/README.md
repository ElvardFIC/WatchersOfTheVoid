# assets/paks: the starter-skin paks

The release zip's `pak\` route is filled from here by `.github/ci/package.ps1` (included
automatically when the directory is non-empty). The contents are committed; see Tracking below
for why. Expected contents: one bundle pak carrying the four starter scientists the mod assigns
randomly to a new identity, plus the four skin-browser preview tiles named by member:

```
scientists.pak     (walter_v1sc + sci_v1sc + rvi_scientist_v1sc + luther_v1sc)
walter_v1sc.png  sci_v1sc.png  rvi_scientist_v1sc.png  luther_v1sc.png
```

Additional preview tiles are included for skins already present in the game. Their filenames
are the skin keys used by the skin browser; `dr_kel.png` previews the pak-less native body.
The supplied duplicate white-robot image is included only once.

| Preview file | Skin |
|---|---|
| `dr_kel.png` | Native Dr. Kel |
| `kerfur_maxwell.png` | Maxwell cat |
| `scrappy_keith.png` | Scrappy Keith |
| `kerfur_antibreather.png` | Antibreather |
| `kerfur_ariral.png` | Ariral |
| `kerfur_alien.png` | Ayy Lmao / alien figure |
| `kerfur_maid.png` | Striped maid robot |
| `kerfur_monique.png` | Alternate striped maid robot |
| `kerfur_mannequin.png` | Mannequin |
| `kerfur_erie_v4.png` | Erie V4 |
| `kerfur_keljoy.png` | Keljoy |
| `kerfur_krampus.png` | Krampus |
| `kerfur_omega.png` | White Omega |
| `kerfur_omega_h.png` | Purple Omega |
| `kerfur_omega_m.png` | Red Omega |
| `kerfur_omega_nc.png` | Blue Omega |
| `kerfur_mynet.png` | Mynet (user-confirmed supplied preview) |
| `kerfur_skeleton.png` | Skeleton |

The bundle-to-members mapping lives in `skin_registry.cpp` (`kSkinBundles`); the loader itself
never keys on the pak filename, since assets resolve by their internal
`/Game/Mods/VOTVCoop/<name>` paths. Rebuild the bundle from the four single-skin paks with repak
(UE4 pak V11, mount `../../../`, Zlib):

```
repak unpack -o un_<name> <name>.pak      # each source pak
# merge the four trees into one directory
repak pack -m "../../../" --version V11 -p 0 --compression Zlib <dir> scientists.pak
```

**Compress it.** The uncompressed pak is 5.2x larger for nothing: 14 343 848 bytes uncompressed
against 2 776 794 with Zlib, unpack-compare byte-identical, and both peers load skins from the
compressed pak. UE mounts a Zlib pak natively; nothing in the mod's code
sees the difference.

**Why it was that large, and the deeper fix, not done.** The meshes are 90-200 KB each; the whole
cost is the textures, stored as uncompressed BGRA8. Three are exactly 4 MiB (1024x1024x4) and one
is 1 MiB (512x512x4), for Half-Life-era art. The skin converter should emit a
block-compressed format (BC1/DXT1 is 8:1 on an opaque diffuse, BC7 4:1); that would cut the
payload again and cut GPU memory at runtime, which Zlib does not, since a compressed pak still
decompresses to 4 MiB per texture in VRAM. Zlib is the packaging fix; the texture format is the
asset fix.

## Tracking

The binaries are tracked. They were untracked once as public-repo caution for game-derived
meshes, and the caution bought less than it appeared to: these exact bytes are in every public
release archive already. What the ignore actually cost was the automated release. The release
workflow publishes from a GitHub runner whose fresh checkout had no paks, so the packaging step,
which fails closed on a missing pak, made the lane unusable. Tracking them is what makes a tagged
release publishable without a human assembling the zip on a stocked machine.

Worth stating once: git history is permanent. A release asset can be replaced or deleted; a
committed blob stays reachable.

If this directory is ever emptied, restock from any dev install
(`Game_0.9.0n_HOST/WindowsNoEditor/VotV/Content/Paks/LogicMods/multivoid/`) or rebuild from the
model sources with the converter.
