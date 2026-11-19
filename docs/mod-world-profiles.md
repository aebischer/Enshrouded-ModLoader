# World profiles and loading lifecycle

Normative reference: [SDK V1](../sdk/STANDARD_V1.txt). Current loader: 0.6.12.

## Packages

Install a DLL under `Mods/EnshroudedModLoader/` with its adjacent `name.dll.mod` manifest:

```ini
ENSHROUDED_PACKAGE_V1
kind=mod
version=1.0
abi=1
require=mymod/support.dll@1.0
```

Optional `require` paths are relative to Mods/EnshroudedModLoader, use `/` and specify exact versions.
Unknown keys, duplicates, unsupported ABI/version syntax and cycles are rejected.
Limits: depth 64, 1,024 modules, 64 direct dependencies and a 16 KiB manifest.

A `kind=library` package must not export EnshroudedModInitialize, cannot be enabled
alone and loads automatically when required. A required `kind=mod` package must
be explicitly enabled. Initialize uses the public SDK C export, checks context
size/ABI and returns 1 on success. It runs once outside DllMain. The context and
world GUID remain valid until process exit. No native world pointer is supplied.
Exceptions/refusals block launch; partially loaded DLLs require a restart.
Native memory violations can still crash the process.

## Preflight

Before the first LoadLibrary, validate the whole dependency graph: manifests,
exact versions, canonical paths, physical identities, unambiguous DLL names,
x64 PE format, exports and declared ordinary/delay imports. System dependencies
are resolved in System32. Validated files remain open without write/delete
sharing during loading. Physical aliases and an already loaded module outside
the profile are rejected because their state cannot be controlled.

These checks do not sandbox DLLs or stop a mod calling LoadLibrary itself.
Package files and write access to the game directory remain trust boundaries.

## Profiles

`Mods/EnshroudedModLoader/worlds/<GUID>.mods` contains:

```text
ENSHROUDED_MODS_V1
mymod/main.dll
```

UTF-8, optional BOM, LF/CRLF. Paths are normalized. Limits: 1,024 entries,
1,024 bytes per path; duplicates rejected. Reads are bounded before allocation.
An invalid profile never silently becomes an empty list.

A missing profile means an empty configuration for any world, new or existing.
No confirmation step is needed. Enabling a mod creates the profile on its first
save. Deleting it resets choices; refresh afterwards. Resident DLLs are not
unloaded, so a restart may still be required if the requested list differs.

Writes use a lock and compare the current file against the previously read
state. On conflict, refresh before retrying. A unique temporary is flushed and
published by replacement. External editors that ignore the lock are outside
this protocol; close the game before editing/importing profiles manually.

Transfer the matching GUID profile, world save, settings directory and required
mod versions together. Steam Cloud synchronization of profiles is not implemented.
An omitted profile is equivalent to zero enabled mods.

## Sessions and menus

The list distinguishes mods, required/unused libraries, missing files and invalid
manifests. It shows filenames and per-world states, with version and options in
the details pane. Pagination keeps selection visible. Metadata is cached on
opening/refresh; rendering does not scan the filesystem. Discovery is bounded
by entry count, depth and time.

Changing the loaded list or entering a different world with resident mods
requires a full restart. Reusing the same world and loaded settings is allowed.
A failed partial load blocks launch until restart. Remote joining is refused
while local mods remain resident or loading left a tainted session. There is
no hot unload, server-mod protocol or automatic other-framework compatibility.

Bootstrap checks the executable SHA-256 and hooked bytes against the file.
Incompatibility displays a Windows warning. The proxy remains resident and
DllCanUnloadNow returns S_FALSE. Logs rotate around 2 MiB plus one previous file.
Native navigation, controllers, resolutions and gameplay require in-game checks;
offline tests cannot certify all of these paths.

## Export/import with a save

With the game closed, use `tools/transfer-profile.ps1` with `-Action Export` or
`Import`, `-GamePath`, `-WorldId` (full GUID) and `-Path` (`.worldmods` file).
The package contains world identity, activation profile and its digest. Import
validates GUID, size and integrity, acquires the lock and atomically replaces
the profile. The old profile is retained as `.before-import`.
It contains neither settings, game saves nor DLLs; transfer those separately.

```powershell
.\tools\transfer-profile.ps1 -Action Export -GamePath 'D:\SteamLibrary\steamapps\common\Enshrouded' -WorldId '32_CHARACTER_WORLD_GUID' -Path 'D:\Backups\world.worldmods'
```
