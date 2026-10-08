# Nearby farming panel (moved)

The radar now lives in the separate `TitanQuestFarmRadar` folder beside this
project. Its ASI owns the live scanner, bottom-left panel, F8 shortcut and its own
INI/log folder. Museum alone shows no radar and has no radar configuration keys.

Museum retains the existing contextual loot worker and exposes the versioned
read-only C API described in [ADDON_API.md](ADDON_API.md). Both ASIs use the same
current source rankings; no second database or model is loaded. Catalogue,
loot-model and collection files stay where they are and need no deletion.

Install the matching Museum provider build and the add-on ASI together. Removing
only the add-on ASI, with the game closed, removes the panel while keeping Museum.
See the separate project's README for settings, builds and verification limits.
