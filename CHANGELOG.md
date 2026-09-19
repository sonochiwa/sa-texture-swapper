# Changelog

## 1.2.1

- Added `README.txt` to the release archive.

## 1.2.0

- Removed the INI and the log; hot reload is always on and the textures
  folder is always `swapper` next to the executable.

## 1.1.0

- Changed `loggingEnabled` to `log`.
- Added version information to the plugin file.
- Removed `README.txt` from the release archive.

## 1.0.0

- Replaces single textures inside TXD dictionaries from loose PNG files,
  named by folder (the TXD) and file (the texture).
- Works with streamed and standalone TXDs, on top of Mod Loader or without
  it.
- Hot reload: edited PNGs apply while the game runs, deleted ones restore
  the original.
- Adds a texture the dictionary does not have.
- Stays inactive on any executable other than 1.0 US.
