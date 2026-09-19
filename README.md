# Texture Swapper

`TextureSwapper.asi` is a standalone GTA San Andreas plugin that replaces
single textures inside `.txd` dictionaries from loose PNG files, while the
game is running.

Mod Loader works with whole files: to change one icon you have to supply the
entire `.txd`. That is awkward for small edits, like swapping the fist icon, the
radar ring or a crosshair inside an HD weapon pack. Texture Swapper goes one
level deeper: the game builds the dictionary as usual, and the plugin puts your
PNG into the finished dictionary.

Edit a PNG while playing and the texture updates without a restart. Delete it
and the original comes back. Nothing on disk is modified, and no `.txd` or
`.img` has to be rebuilt.

Because it patches a dictionary that is already built, it composes with Mod
Loader instead of competing with it, and it does not need Mod Loader installed.

## Features

- Replaces single textures inside a TXD and leaves the rest alone.
- Works with TXDs streamed from the IMG archives and with standalone ones such
  as `hud.txd`, `fonts.txd` and `particle.txd`.
- Picks up edits at runtime; deleting a PNG restores the original texture.
- Applies on top of dictionaries supplied by Mod Loader, and works without it.
- Adds the texture if the dictionary has none by that name.
- Stays inactive on any executable other than 1.0 US.

## Requirements

- GTA San Andreas 1.0 US (Compact or Hoodlum executable). Hook addresses are
  specific to this build; 1.01, 3.0 Steam and the Definitive Edition are
  rejected at startup and the plugin stays inactive.
- An ASI loader, such as Silent's ASI Loader or Ultimate ASI Loader.

## Installation

1. Extract `TextureSwapper.asi` into the GTA San Andreas directory or its
   `scripts` directory.
2. Start the game once; it creates a `swapper` folder next to `gta_sa.exe`.
3. Put PNG files inside it, one folder per TXD. Edits are picked up while the
   game runs.

The folder holding a PNG names the TXD, and the file name names the texture:

```text
swapper\hud\fist.png   ->  texture "fist" in hud.txd
swapper\models\gta3.img\camera\cameraCrosshair.png
                       ->  texture "cameraCrosshair" in camera.txd
```

- A `.txd` suffix on the folder is accepted too, so `hud\` and `hud.txd\` are the
  same target.
- A few dictionaries are loaded into a slot whose name differs from the file:
  `models\fronten_pc.txd` ends up in a slot called `frontend_pc`. Either name
  works, so naming the folder after the `.txd` file is always enough.
- Folders above the TXD folder are free-form, so the tree can mirror the game's
  layout or group files by mod. They are not part of the match: the game
  identifies a dictionary by its name alone, with no notion of the archive it
  came from.
- Names are matched case-insensitively.

Texture names inside a `.txd` rarely match the file names a mod uses, so look
them up in a TXD editor such as Magic.TXD. There is nothing to configure: the
folder is always `swapper` next to the executable and hot reload is always on.

PNG is the only supported format. Replacements are uploaded uncompressed and
without mipmaps, so a 1024x1024 file takes 4 MB of video memory, and a replaced
world texture aliases at a distance where the original did not. Texture names
longer than 31 characters are skipped.

## Building

Visual Studio 2022 (v143), `Release|Win32`. Open `TextureSwapper.sln` or run:

```powershell
msbuild TextureSwapper.sln /t:Rebuild /p:Configuration=Release /p:Platform=Win32
```

The plugin is written to `build\TextureSwapper.asi`.

## Repository Layout

```text
TextureSwapper.sln
README.md
CHANGELOG.md
LICENSE
.github\workflows\release.yml   Tagged release build, checksum and attestation
src\
  TextureSwapper.cpp            DllMain: version check, hook installation, startup thread
  TextureSwapper.rc             Version resource
  TextureSwapper.vcxproj
  addresses.h                   Game and RenderWare addresses
  applier.cpp / applier.h       Applies, reapplies and reverts replacements
  game.cpp / game.h             RenderWare structures and wrappers
  game_check.cpp / game_check.h Executable identification
  hooks.cpp / hooks.h           CTxdStore and CTimer hooks, deferred initialisation
  overrides.cpp / overrides.h   Folder scan; maps TXD name hash to PNG files
  stb_image_impl.cpp            stb_image implementation unit
  texture.cpp / texture.h       PNG to RwRaster
  watcher.cpp / watcher.h       Folder watcher for hot reload
  resource.h
  version.h
vendor\
  minhook\                      MinHook, compiled into the plugin
  stb\                          stb_image.h
```

## How It Works

| Hook | Address | Purpose |
| --- | --- | --- |
| `CTxdStore::LoadTxd` | `0x731DD0` | Standalone TXDs such as `hud.txd`. |
| `CTxdStore::FinishLoadTxd` | `0x731E40` | TXDs streamed from the IMG archives. |
| `CTxdStore::RemoveTxd` | `0x731E90` | Frees the original rasters held for a slot. |
| `CTimer::Update` | `0x561B10` | Per-frame tick that applies hot-reload edits. |

Both load hooks run after the original, so the plugin patches a dictionary that
is already complete, whoever produced its data. Folder names are hashed with the
game's own `CKeyGen::GetUppercaseKey` and compared against `TxdDef::hash` of the
slot being loaded.

A replacement swaps the `RwRaster` inside the existing `RwTexture` rather than
replacing the texture object, so pointers already held by sprites and by
materials of models loaded later stay valid, and the original raster is kept
aside for restoring.

`DllMain` only compares bytes and installs hooks. Configuration, logging and the
folder scan happen on a startup thread, so no file access occurs under the loader
lock.

## Release Integrity

Tagged releases are built by GitHub Actions from the tagged commit. Each
release carries `TextureSwapper-vX.Y.Z.zip`, its SHA-256 in
`TextureSwapper-vX.Y.Z.zip.sha256` and a signed build-provenance attestation,
which proves that the archive was produced by this repository's workflow
from that revision. It does not prove the code is bug-free.

```text
gh attestation verify TextureSwapper-vX.Y.Z.zip -R sonochiwa/sa-texture-swapper
```

## License

MIT. See [LICENSE](LICENSE).
