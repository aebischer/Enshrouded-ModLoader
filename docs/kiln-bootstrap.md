# Unified loader: ModLoader 0.6.15 development

## One binary, automatic installation

`build.cmd` compiles **one** DLL: `dist/dinput8.dll`. It exports both the standard
DirectInput proxy functions and `kiln_native_init`. There is no Kiln-specific
compile flag and no second implementation of the loader.

Run `Install.cmd` with the game closed:

- Without Kiln's root proxy, install as `<game>/dinput8.dll` (standalone).
- With a valid x64 proxy exporting `KilnProxyInfo`, install the **identical bytes**
  as `<game>/kiln/native/enshrouded-modloader/modloader.dll`, with `native.json`
  requesting client side and boot phase. Never replace Kiln's proxy.

An arbitrary foreign proxy is refused, not overwritten. A folder called `kiln`
without its active proxy does not select Kiln mode. Existing native files must
match our receipt before updating/removing them. Profiles may disable native
modules or exclude drop-ins: enable `enshrouded-modloader` in Kiln if necessary.
The installer does not override Kiln's profile policy or `.disabled` markers.

Both modes resolve paths from the executable: `Mods/EnshroudedModLoader/` and
`Mods/EnshroudedModLoader/worlds/`. SDK ABI, mod IDs and settings are unchanged. CarryOn and
Lantern remain our per-world packages, not independent Kiln native modules.

## Migration and recovery

When Kiln previously moved our standalone proxy into `kiln/chain`, its original
`RandomWorld/installed.sha256` receipt identifies the exact owned file. Installation
removes that file and its matching `chain.ini` / `dinput8-chain.ini` lines together
with publishing the native module. Foreign chain files/entries are retained.
Unknown copies without an ownership receipt are not deleted automatically.

A journal backs up every changed file before publication. Failure restores the
old bytes; an interrupted transaction is recovered on the next installer run.
Changed foreign files and recovery-path collisions are refused. Successful
transactions retain a `RandomWorld/dual-install.backup.*.json` backup. Package
migration keeps relative mod IDs, assets, equipment markers and per-world settings.

`Uninstall.cmd` removes only our managed files, preserving Kiln, packages and
profiles. If Kiln is removed first, rerun `Install.cmd`: it installs our root
proxy and removes the owned native copy in a recoverable transaction.

## Startup and double initialization

DllMain starts a worker outside the Windows loader lock. With a root proxy
exporting `KilnProxyInfo`, the worker exits **before** claiming initialization,
leaving the official native callback in control. Without Kiln, it initializes
standalone. Native initialization validates ABI/context, client side, boot phase,
owner/module, enabled hooks and the API logging table. Safe idle and unsupported
contexts never claim the initializer. Kiln errors are reported through its API;
detailed errors remain in `ModLoader/mod-loader.log`.

An atomic state prevents concurrent initialization of one DLL. A process-local
named mutex prevents differently named new copies from initializing twice. A
failure is terminal until restart. Pre-0.6.13 copies do not implement this guard;
that is why the installer retires receipt-identified legacy chain copies rather
than merely installing another copy beside them.

## Tests and compatibility limits

`tests/test-loader-kiln.cmd` tests the real unified DLL with a simulated proxy,
native context gates, concurrent initialization, unsupported executable refusal,
duplicate copies and proxy deferral. `tests/test_kiln_install.ps1` exercises
standalone-to-Kiln and reverse transitions, identical DLL hashes, chain retirement,
settings/package preservation, update failure rollback and interrupted recovery.
The existing profile, settings, catalogue, localization, hardening and installer
suites remain applicable.

This phase retains MinHook, the pinned executable hash and the pristine-target
checks for all eight loader hooks. CarryOn and Lantern retain their own checks.
It does **not** silently accept previously modified targets, nor implement the
future `kiln.hooks` / SDK hook adapter. A conflict is refused.

Real-runtime validation: the public GitHub packages were installed with Kiln
0.2.0-dev.29.19-beta.1 (core and client sigdb). Logs and loaded modules confirmed
one loader initialization and one instance of each mod. The user confirmed
CarryOn gameplay and, with Lantern 0.1.31-dev, light rendering and equipment
persistence after reload. Standalone startup and installation remain supported.
Other hosted frameworks, multiplayer, dedicated servers and future builds are
not certified by this test. Use Lantern 0.1.31-dev or newer with this setup.

Upstream contracts: [native mods](https://github.com/frostdev-ops/kiln/blob/main/docs/kiln/native-mods.md),
[dual entry](https://github.com/frostdev-ops/kiln/blob/main/docs/kiln/modding/native-asi.md).
