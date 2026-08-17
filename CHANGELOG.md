# Changelog

## 1.0.0

- Added replacement of single textures inside GTA San Andreas texture
  dictionaries from loose PNG files, addressed by the containing folder name
  (the TXD, with or without a `.txd` suffix) and the file name (the texture).
  Folders above the TXD folder are free-form. A PNG placed directly inside a
  folder ending in `.img` is rejected, because an archive does not identify a
  dictionary.
- Added matching by the name of the `.txd` file as well as by the name of the
  slot the game loads it into. The two differ for the frontend dictionaries
  (`models\fronten_pc.txd` becomes the slot `frontend_pc`), and only the file
  name is something a person can see. A folder that is close to either name but
  matches neither is reported in the log with the names that would work.
- Added support for both TXDs streamed from the IMG archives and standalone ones
  such as `hud.txd`, `fonts.txd` and `particle.txd`, by patching the dictionary
  after the game finished loading it. Replacements therefore apply on top of
  dictionaries supplied by Mod Loader without depending on it.
- Added hot reload: edited PNG files are reapplied while the game runs,
  including to already loaded dictionaries, and deleting a PNG restores the
  game's original texture.
- Added insertion of a texture that does not exist in the target dictionary. When
  the dictionary contains similarly named textures, the log reports them, because
  a missing name is usually a misspelled file name rather than a deliberate
  insertion.
- Added an executable check that identifies 1.0 US by the PE headers, which no
  other plugin rewrites, and falls back to byte signatures while tolerating
  functions another plugin has already hooked. On any other build no hooks are
  installed. This is what lets the plugin load from `scripts\`, after loaders
  such as CLEO, MoonLoader and SAMPFUNCS have hooked the same functions.
- Added `TextureSwapper.ini` with `isEnabled`, `loggingEnabled` and `hotReload`.
  A missing INI is generated from the canonical configuration. Logging is off by
  default and writes no file until `loggingEnabled=1`.
