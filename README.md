# Enshrouded ModLoader 0.6.15

Native Windows x64 mod loader for Enshrouded, with a **MODS** page in each
world's **Edit** menu. Enabled mods and settings are stored separately for each world.
This independent project is not an official Keen Games SDK.

For installation without compiling, download the ZIP from
[Releases](https://github.com/aebischer/Enshrouded-ModLoader/releases/tag/v0.6.15) and follow
[INSTALLATION.txt](INSTALLATION.txt). The repository and downloads are public; no GitHub account is required. Mods are installed separately.

## Automatic standalone / Kiln installation

One DLL supports both startup paths. `Install.cmd` detects Kiln's proxy exports:
without Kiln it installs `dinput8.dll`; with Kiln it installs the **same binary**
as `kiln/native/enshrouded-modloader/modloader.dll` plus a boot-phase manifest.
Kiln's proxy is never replaced. Owned legacy chain copies are retired in the
same recoverable transaction. Profiles, packages and SDK ABI are unchanged.
See [installation, migration and validation](docs/kiln-bootstrap.md). Solo testing
with Kiln 0.2.0-dev.29.19-beta.1 confirmed CarryOn 0.1.52-dev and Lantern
0.1.31-dev, including lantern light and equipment persistence. Other hosted
frameworks and multiplayer are not certified. Install Kiln first when using it,
then run our installer; enable our native module in Kiln if your profile requires it.

## Install and use

Close Enshrouded, extract the complete release ZIP, then run **Install.cmd**.
For a custom game directory:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File install.ps1 -GamePath "D:\SteamLibrary\steamapps\common\Enshrouded"
```

The installer validates the x64 proxy and its exports, saves the previous
version, and restores it if installation fails. **Uninstall.cmd** removes the
managed proxy while preserving profiles, logs and installed mods.

Open **Edit â†’ MODS**, select a mod, enable it and adjust its settings. Changes
apply when starting the world. Once DLLs have loaded, switching mods or worlds
may require completely restarting the game. There is no hot unloading or
automatic restart. No test mods are included in the player package.

Supported `enshrouded.exe` SHA-256:

```text
5D9EF2F140B8ED49E30DBEE655841B89D08BAEC20F799A1817E171A18A850C77
```

Game updates may require a loader update. If an incompatibility warning appears,
do not start a modded world. Dedicated-server and multiplayer support is not
certified; see [multiplayer decisions](docs/server-multiplayer-decisions.txt).

## Package directory and upgrades

Only `Mods/EnshroudedModLoader/` is scanned. Other frameworks may use the shared
`Mods` / `mods` directory (the names are equivalent on Windows) without appearing
in our catalogue or consuming its scan budget. A DLL needs an adjacent `.dll.mod`
manifest with the `ENSHROUDED_PACKAGE_V1` header to appear automatically;
enabled missing/broken packages still report errors.

Run the current installer before copying updated mod packages. It moves recognized
legacy package folders into the dedicated directory, preserving relative IDs,
resources and saved package data. It does not rewrite `Mods/EnshroudedModLoader/worlds`.
Mixed folders (our packages and foreign DLLs), linked paths and destination
collisions are refused without overwriting files. Installation failure rolls back
both the proxy and package moves; an interrupted transaction is recovered on the
next installer run. Do not manually copy the proxy to perform this migration.

## World profiles and settings

- `Mods/EnshroudedModLoader/worlds/<GUID>.mods` stores enabled mods. A missing file means no
  enabled mods; the Enable controls are immediately available for any world.
- `Mods/EnshroudedModLoader/worlds/<GUID>.settings-v2/<SHA256-normalized-mod-id>.ini` stores a
  mod's settings for that world, independently of whether it is enabled.
- Deleting a file resets the corresponding choices. Invalid files cause an
  error instead of silently reverting to defaults. Legacy `.settings` files
  are migrated on first read without overwriting existing settings.
- Back up the entire `Mods/EnshroudedModLoader/worlds` directory with your worlds. Steam Cloud
  does not synchronize these profiles. The `.mods` transfer utility currently
  transfers activation profiles only, not settings, game saves or DLLs.

## Build

Install Visual Studio 2022 Build Tools with C++ x64 tools and the Windows SDK.
The root scripts currently expect:
`C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools`.
MinHook is included under its [original license](third_party/minhook/LICENSE.txt).

Run from the repository root, stopping if any command fails:

```bat
build.cmd
build-sdk.cmd
```

`build.cmd` produces `dist/dinput8.dll`. `build-sdk.cmd` builds the sample mod,
package validator and SDK tests. Generated binaries, game files, saves, logs
and personal profiles are excluded from Git.

## Create a mod

Start with the [end-to-end mod authoring guide](sdk/CREER_UN_MOD.md),
[SDK overview](sdk/README.txt), [V1 / ABI 1 standard](sdk/STANDARD_V1.txt), and
[localization guide](sdk/LOCALISATION.txt). These documents are in English;
their existing filenames are retained to keep links stable.

Each mod provides a Windows x64 DLL, an adjacent `.dll.mod` manifest and the
SDK integration exports. The repository includes a compilable example and a
validator that uses the loader's actual preflight checks. DLLs written for
other frameworks are not automatically compatible. Native DLLs run with the
game's permissions; this loader is not a sandbox.

## Interface languages

All loader text added inside the gameâ€”navigation, states, settings, errors and
launch refusalsâ€”covers the game's 15 interface languages: English, French,
German, Italian, Spanish, Russian, Polish, Japanese, Korean, Brazilian Portuguese,
Thai, Turkish, Simplified Chinese, Traditional Chinese and Ukrainian.
The selected game language controls the interface, including failure dialogs.
Before the native language is available, failure dialogs use English. English is the default
language of project documentation and technical diagnostics.

The source catalogue is `assets/localization/loader.json`. The build validates
every language and key before generating `src/loader_texts.h`.
`tests/test-mod-i18n.cmd` checks exact coverage; English fallback does not count as
a translation. New translations still need visual and linguistic review in-game.

## Validation

After building:

- `tests/build-test-mods.cmd` builds Test_A/Test_B fixtures under `build/test-mods`
  only. Run it before settings and new-world tests; do not install these fixtures.
- `tests/test-mod-catalog.cmd`, `tests/test-mod-profiles.cmd`, `tests/test-mod-settings.cmd`.
- `tests/test-new-world-profiles.cmd`, `tests/test-mod-hardening.cmd`, `tests/test-mod-i18n.cmd`.
- `tests/audit-mod-067.cmd`, `tests/audit-mod-068.cmd`.
- `tests/analyze-loader.cmd` runs MSVC static analysis.
- `tests/test_install.ps1` and `tests/audit-install-format-068.ps1` exercise
  installation in isolated directories.

Some tests require a local supported game executable or machine-specific paths.
No game files are redistributed. Offline checks do not replace gameplay tests.
See [localization validation](docs/localization-validation.md) for the latest results.

The root `.gitignore` is an explicit publication allowlist. Add new project
files to it before committing them.

## World profiles migration (ModLoader 0.6.15)

Run the loader Install.cmd before updating or starting the game. It migrates
old ModLoader/worlds files to Mods/EnshroudedModLoader/worlds, with verified
backups, conflict checks and restartable copying. Preserve the new worlds
folder when updating mods. Use 0.6.15 or newer for this directory layout.
