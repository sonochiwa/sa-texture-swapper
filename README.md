# Texture Swapper

An ASI plugin for **GTA San Andreas 1.0 US** that replaces single textures
inside `.txd` dictionaries from loose PNG files, while the game is running.

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
- Adds the texture if the dictionary has none by that name, and logs the similar
  names it does have, since that is usually a typo.
- Stays inactive on any executable other than 1.0 US.

## Requirements

- **GTA San Andreas 1.0 US** (`gta_sa.exe`, image base `0x400000`). Hook
  addresses are specific to this build; 1.01, 3.0 Steam and the Definitive
  Edition are rejected at startup.
- An ASI loader, such as Silent's ASI Loader or Ultimate ASI Loader.

## Installation

1. Copy `TextureSwapper.asi` and `TextureSwapper.ini` into the game directory,
   next to `gta_sa.exe` (or into `scripts`).
2. Create a folder named `swapper` next to `gta_sa.exe`.
3. Put PNG files inside it, one folder per TXD.
4. Start the game.

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
- Set `loggingEnabled=1` in the INI to get `TextureSwapper.log` next to the
  plugin, listing every replacement and every skipped file. A folder that is
  close to a real dictionary name but matches none is reported there with the
  names it should have.

Texture names inside a `.txd` rarely match the file names a mod uses, so look
them up in a TXD editor such as Magic.TXD.

## Configuration

`TextureSwapper.ini` sits next to `TextureSwapper.asi`. When it is missing, the
plugin writes this file:

```ini
# Texture Swapper v1.0.0
# Created by sonochiwa
# Source code: https://github.com/sonochiwa/sa-texture-swapper

[general]
isEnabled=1
loggingEnabled=0
hotReload=1
```

| Setting | Default | Meaning |
| --- | --- | --- |
| `isEnabled` | `1` | Master switch. `0` performs no replacements. |
| `loggingEnabled` | `0` | Write `TextureSwapper.log` next to the plugin. Off by default, so no file is created at all. |
| `hotReload` | `1` | Watch the folder and apply edits while the game runs. |

Turn `loggingEnabled` on when a replacement does not show up: the log then names
every texture that was replaced and every file that was skipped, with the reason.
That is also the only place the plugin reports an unsupported executable, so with
logging off it simply stays silent.

The textures folder is always `<game>\swapper` and cannot be moved. Settings are
read once at startup; `hotReload` covers the PNG files, not the INI. There are no
hotkeys.

PNG is the only supported format. Replacements are uploaded uncompressed and
without mipmaps, so a 1024x1024 file takes 4 MB of video memory, and a replaced
world texture aliases at a distance where the original did not. Texture names
longer than 31 characters are skipped, and non power-of-two sizes are reported,
both in the log.

## Building

Requires Visual Studio with the C++ desktop workload. A pre-build step uses
Windows PowerShell to generate the embedded configuration template from
`Config\TextureSwapper.ini`, which is the single source of truth for defaults.

```powershell
msbuild TextureSwapper.sln /t:Rebuild /p:Configuration=Release /p:Platform=Win32 /m
```

Output is `build\TextureSwapper.asi` with `build\TextureSwapper.ini` beside it.
`tools\Package.ps1 -Version X.Y.Z` stages and zips a release from that build.

## Release Integrity

Release archives are built by GitHub Actions from the tagged commit. Each release
carries a `.sha256` file and a signed build-provenance attestation:

```text
gh attestation verify TextureSwapper-v1.0.0.zip -R sonochiwa/sa-texture-swapper
```

That proves the bytes came from this repository's workflow at a specific commit.
It says nothing about the code being free of defects.

## Repository Layout

```text
TextureSwapper.sln
README.md
CHANGELOG.md
LICENSE
Config\
  TextureSwapper.ini         Canonical release configuration
src\
  TextureSwapper.vcxproj
  main.cpp                   DllMain, version check, hooks, deferred startup
  config.cpp/.h              INI handling
  overrides.cpp/.h           Folder scan; maps TXD name hash to PNG files
  applier.cpp/.h             Applies, reapplies and reverts replacements
  texture.cpp/.h             PNG to RwRaster
  game.cpp/.h                RenderWare structures and game addresses
  watcher.cpp/.h             Folder watcher for hot reload
  log.cpp/.h
  version.h
vendor\
  minhook\                   Function hooking
  stb\                       PNG decoding
tools\
  GenerateConfigTemplate.ps1
  Package.ps1
.github\workflows\release.yml
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

## License

MIT. See [LICENSE](LICENSE).

Third-party components: [MinHook](https://github.com/TsudaKageyu/minhook)
(BSD-2-Clause) and [stb_image](https://github.com/nothings/stb) (public domain
or MIT).
