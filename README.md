# Texture Swapper

`TextureSwapper.asi` is a GTA San Andreas plugin that replaces single
textures inside `.txd` dictionaries from loose PNG files while the game is
running.

Mod Loader works with whole files: to change one icon you have to supply the
entire `.txd`. Texture Swapper puts your PNG into the dictionary after the
game has built it, so nothing on disk is modified and no `.txd` or `.img`
has to be rebuilt. It works on top of Mod Loader and without it.

## Features

- Replaces single textures inside a TXD and leaves the rest alone.
- Works with TXDs streamed from the IMG archives and with standalone ones
  such as `hud.txd`, `fonts.txd` and `particle.txd`.
- Picks up edits while the game runs; deleting a PNG restores the original.
- Adds the texture if the dictionary has none by that name.

## Requirements

- GTA San Andreas 1.0 US (Compact or Hoodlum executable).
- An ASI loader, such as Silent's ASI Loader or Ultimate ASI Loader.

Other executables are left untouched.

## Installation

1. Extract `TextureSwapper.asi` into the GTA San Andreas directory or its
   `scripts` directory.
2. Start the game once; it creates a `swapper` folder next to `gta_sa.exe`.
3. Put PNG files inside it, one folder per TXD.

The folder holding a PNG names the TXD, and the file name names the texture:

```text
swapper\hud\fist.png   ->  texture "fist" in hud.txd
swapper\models\gta3.img\camera\cameraCrosshair.png
                       ->  texture "cameraCrosshair" in camera.txd
```

Folders above the TXD folder are free-form, a `.txd` suffix on the folder is
accepted, and names are matched case-insensitively. Texture names rarely
match the file names a mod uses, so look them up in a TXD editor such as
Magic.TXD.

PNG is the only supported format. Replacements are uploaded uncompressed and
without mipmaps, so a 1024x1024 file takes 4 MB of video memory. Texture
names longer than 31 characters are skipped.

## Release Integrity

Releases are built by GitHub Actions from the tagged commit and carry a
SHA-256 file and a build-provenance attestation:

```text
gh attestation verify TextureSwapper-vX.Y.Z.zip -R sonochiwa/sa-texture-swapper
```

## License

MIT. See [LICENSE](LICENSE).
