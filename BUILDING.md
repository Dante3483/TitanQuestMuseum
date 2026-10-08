# Titan Quest Toolkit

Component documentation: [Museum](museum/BUILDING.md).

See [README](README.md) for the four-component structure and current migration status.

The root `build.bat` builds all four components and, after they succeed, copies
their ASI binaries into the root `dist/` folder for installation. Each component
also keeps its own binary and symbols in its local `dist/` folder.
Museum language resources are also copied into root `dist/localization/`.
Install that folder beside the ASI files in the game's `scripts/` directory.
