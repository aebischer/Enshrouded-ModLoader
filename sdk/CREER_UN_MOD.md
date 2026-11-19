# Create and distribute a compatible mod

This guide targets **ModLoader 0.6.11**, **manifest V1** and **ABI 1**.
The [standard](STANDARD_V1.txt) defines the contract; the
[public header](include/enshrouded_mod.h) defines the C signatures.
The repository is public and can be cloned without a GitHub account. Compiled player
packages are available separately through GitHub Releases.

The SDK lets a Windows x64 DLL be discovered, configured per world and
initialized. It does not provide item creation, recipes, textures, 3D models,
placement, save manipulation or networking APIs. These require a mod-specific
engine adapter verified against the supported client. Package acceptance is
not gameplay certification.

## 1. Build the loader and SDK

Requirements: Windows x64, Git, PowerShell and Visual Studio 2022 Build Tools
with C++ x64 and Windows SDK. Root scripts expect
`C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools`.
For another installation, adjust the relevant `vcvars64.bat` paths.
MinHook is already included.

From `cmd.exe`, stop at the first failing command:

```bat
git clone https://github.com/aebischer/Enshrouded-ModLoader.git
cd Enshrouded-ModLoader
build.cmd
build-sdk.cmd
tests/test-mod-i18n.cmd
```

| Output | Purpose |
|---|---|
| `dist\dinput8.dll` | Loader proxy |
| `build\sdk-package\Mods\example\example_mod.dll` | Sample mod without gameplay effects |
| `build\sdk-package\Mods\example\example_mod.dll.mod` | Sample manifest |
| `build\validate_mod_package.exe` | Static package validation; does not execute mod DLLs |
| `build\mod_sdk_test.exe` | SDK test in a temporary environment; executes the sample |

A C compilation test also verifies the public ABI header. Translation helpers
use C++17. None of these scripts installs a mod. Test_A/Test_B are separate
development fixtures under `build/test-mods`, not part of the player package.

## 2. Start from the compilable example

For a standalone project, copy these files with their relative paths intact
(or adjust compiler `/I` paths):

```text
MyProject/
  include/
    enshrouded_mod.h       required ABI contract
    enshrouded_i18n.h      optional C++17 localization
  example/
    build.cmd
    example_mod.cpp
    example_mod.dll.mod
    localized_texts.h      optional sample catalogue
```

Open **x64 Native Tools Command Prompt for VS 2022**:

```bat
cd C:\path\MyProject
example\build.cmd C:\path\MyPackage
```

The output is `MyPackage\Mods\example\example_mod.dll` and its manifest.
Distribute `Mods` and useful documentation, not `.obj`, `.lib` or `.exp` files.

To give the mod its own identity, change the output folder and `example_mod`
name in the build script and rename its manifest accordingly. For example:
`Mods\mystudio\mystudio_mymod.dll` and `mystudio_mymod.dll.mod`.
Its ID is `mystudio/mystudio_mymod.dll`. Keep this stable after publication or
provide a profile/settings migration. The DLL basename must be unique even
among DLLs installed in other subdirectories.

## 3. Declare the manifest and settings

Use **UTF-8**, the exact **`.dll.mod`** extension (not `.dll.mod.txt`), optional
BOM, and LF or CRLF. Example:

```text
ENSHROUDED_PACKAGE_V1
kind=mod
version=1.0.0
abi=1
setting=messages|bool|Messages|true
setting=mode|choice|Mode|normal|slow|normal|fast
setting=intensity|percent|Intensity|100|0|200|25
text=en|setting.messages|Messages
text=fr|setting.messages|Messages de test
text=en|setting.mode|Mode
text=fr|setting.mode|Mode de fonctionnement
text=en|choice.mode.0|Slow
text=en|choice.mode.1|Normal
text=en|choice.mode.2|Fast
text=fr|choice.mode.0|Lent
text=fr|choice.mode.1|Normal
text=fr|choice.mode.2|Rapide
text=en|setting.intensity|Intensity
text=fr|setting.intensity|Intensité
```

| Type | Fields after `setting=` | Value delivered to the mod |
|---|---|---|
| Boolean | `key\|bool\|label\|default` | `true` or `false` |
| Choice | `key\|choice\|label\|default\|choice1\|choice2...` | Exact internal choice value |
| Percentage | `key\|percent\|label\|default\|min\|max\|step` | Integer without `%`, e.g. `125` |

Limits: 16 settings, keys of 1–40 characters `[a-z0-9_]`, 2–8 choices,
percentages from 0 to 10000 with at most 32 positions, and a 16 KiB manifest.
See the standard for all field restrictions. Unknown keys such as `author`,
`id`, `assets`, `side` or `min_loader_version` are rejected. Put such metadata
in your README instead.

Translations affect presentation only, never saved keys or values. Manifest
text entries translate settings only. See the [localization guide](LOCALISATION.txt)
for item text adapters, supported languages and strict coverage checks.

## 4. Implement exports and respect their lifetime

Use the header macros without changing names, types or packing:

```cpp
ENSHROUDED_MOD_EXPORT int ENSHROUDED_MOD_CALL EnshroudedModConfigure(
    const char* world_id, const char* settings_utf8);

ENSHROUDED_MOD_EXPORT int ENSHROUDED_MOD_CALL EnshroudedModInitialize(
    const EnshroudedModContext* context);
```

Initialize is mandatory. Configure is mandatory when any setting is declared
and runs before Initialize. The example implements both. If removing all
settings, also adapt its `configuredWorld` check: Configure will no longer run.

The 24-byte context provides `size`, `abi_version`, `world_id` and `write_log`.
Check pointers, size and ABI 1 before use. Return exactly `1` for success or `0`
for failure. C++ exceptions must not cross exported functions.

Configure receives the 32-character world GUID and all effective values,
including defaults, sorted by key and separated by LF. For the example:

```text
intensity=100
messages=true
mode=normal
```

Copy any Configure data you retain, then parse it into typed settings. The
Initialize context remains valid until process exit. Do not modify/free
loader memory or exchange C++ objects/allocations across modules. `write_log`
accepts a NUL-terminated UTF-8 string, limited to 4,096 bytes.

Initialization occurs once per process, outside DllMain. There is no SDK update,
world-change, shutdown or unload callback. A hooked mod owns its session state
and must not retain native object pointers after destruction. Never start
gameplay or install hooks inside DllMain.

## 5. Dependencies and resources

A third-party support library is another x64 DLL with its own manifest:

```text
Mods/
  mystudio/
    mystudio_mymod.dll
    mystudio_mymod.dll.mod
    mystudio_support.dll
    mystudio_support.dll.mod
```

Add to the main manifest:

```text
require=mystudio/mystudio_support.dll@1.0.0
```

Library manifest:

```text
ENSHROUDED_PACKAGE_V1
kind=library
version=1.0.0
abi=1
```

Libraries declare neither settings nor EnshroudedModInitialize and load
automatically when required. A `kind=mod` dependency must be explicitly enabled.
Declare every non-system import, including delayed imports, with exact versions.
Do not redistribute system DLLs. The example uses the static C++ runtime `/MT`.

Additional data files are optional and their format is mod-specific. The loader
does not automatically interpret `.json`, `.png`, `.fbx`, recipes or an `assets`
directory. Your code must resolve its own DLL directory, read external assets
and connect them to the engine. Do not rely on the game's current working
directory. Document asset paths/formats; embedding them in the DLL is also valid.
The SDK has no universal native-resource format.

## 6. Validate and install

From the loader repository root:

```bat
build\validate_mod_package.exe C:\path\MyPackage mystudio/mystudio_mymod.dll
```

For the unmodified example, use `example/example_mod.dll`. The first argument
contains **Mods**, not the DLL directly. Add other enabled/required mod IDs as
additional arguments; libraries resolve automatically. Exit codes: `0` passed,
`1` refused, `2` invalid arguments. This does not execute the mod DLL.

Close the game and install the loader:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File install.ps1 -GamePath "D:\SteamLibrary\steamapps\common\Enshrouded"
```

Adjust the path, or use `Install.cmd` for Steam-directory detection. Only one
`dinput8.dll` can occupy this location. Do not replace it with another framework's
proxy. The loader installer does not copy your mod: merge `MyPackage\Mods`
into `Enshrouded\Mods` separately.

```text
Enshrouded/
  enshrouded.exe
  dinput8.dll
  Mods/
    mystudio/
      mystudio_mymod.dll
      mystudio_mymod.dll.mod
  ModLoader/
    mod-loader.log
    worlds/
      <GUID>.mods
      <GUID>.settings-v2/
        <SHA256-normalized-mod-id>.ini
```

Do not create `Mods\Mods`, put mod DLLs in `ModLoader`, or distribute personal
world profiles, saves or logs. The installer's historical `RandomWorld`
directory holds receipts/recovery data, not mods; no world-generation mod is included.

Supported `enshrouded.exe` SHA-256:

```text
5D9EF2F140B8ED49E30DBEE655841B89D08BAEC20F799A1817E171A18A850C77
```

Check with PowerShell `Get-FileHash -Algorithm SHA256`. A different game build
may require porting both the loader and your adapter.

Start the game, open **Edit → MODS** for the desired world, refresh if needed,
enable the mod and configure it. The DLL executes when starting that world;
appearing in the list does not mean it has loaded. Choices are per-world.
Changing them after DLL loading may require a complete restart. There is no
hot unloading. Replace binaries only while the game is closed.

## 7. Troubleshooting and publication

| Symptom | Check |
|---|---|
| Mod missing | `Mods` layout, `.dll` extension, Refresh |
| Manifest refused | Exact `.dll.mod` name, UTF-8, keys/types/values, validator |
| Load refused | x64 DLL, C exports, declared dependencies and exact versions |
| Setting missing | `setting=` line, 16-setting limit, installed manifest |
| Translation missing | Loader 0.6.10+, `text` keys, game language, fallback chain |
| Settings have no effect | Configure parsing, required restart, correct world |
| Listed mod has no effect | Successful Initialize, logs, implemented engine adapter |
| Restart required | Completely exit the process, then relaunch |

Read `ModLoader\mod-loader.log`; messages sent through `context->write_log`
appear there. Settings storage belongs to the loader. Missing files mean
defaults; invalid files are errors. Incompatible schema changes require a
documented migration; V1 provides no migration callback.

Before release:

1. Validate the package and dependencies; inspect exports with `dumpbin /exports` if needed.
2. Test two worlds, different settings, limits, disabling, restarts and save/reload.
3. Test all supported translations and native text separately from SDK labels.
4. Document mod/loader versions, actually tested game hash/build, dependencies,
   installation, updates and remaining limitations.
5. Distribute the `Mods` tree without the loader proxy, raw game files, build
   objects or personal data.

Dedicated servers and multiplayer are not certified. There is no network API
or automatic settings synchronization. The SDK does not sandbox DLLs, and static
validation does not prove safety or absence of hook conflicts.
