# Alt-Spam Input Buffer (Geode mod for Geometry Dash)

## Project layout
```
mod.json            mod metadata + settings
CMakeLists.txt      build script
src/main.cpp        all mod code
about.md / changelog.md / logo.png
.github/workflows/build.yml   optional cloud build (Win/Mac/Android)
```

## Build (local)
1. Install the Geode CLI: https://docs.geode-sdk.org/getting-started/geode-cli
2. `geode sdk install` (and `geode sdk install-binary` for your platform)
3. In this folder:
   ```
   geode build
   ```
4. Output: `build/yourname.alt-spam-buffer.geode` -> drop it in your GD `geode/mods` folder.

## Before you build
- In `mod.json` change `id` (`yourname.alt-spam-buffer`) and `developer`.
- Match `gd` versions / `geode` version to what you have installed.

## Cloud build (no setup)
Push to GitHub; the Actions workflow builds for Windows, macOS and Android.

## Notes
- Gameplay-affecting mod: don't submit runs made with it to places that forbid input assists.
- Orbs: the live press still reaches the game immediately; the buffer only adds a landing replay.
- Fly modes (ship/UFO/wave/swing) are not buffered.
