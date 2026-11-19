# World profiles directory â€” ModLoader 0.6.15

Active profiles and per-world settings now live under
`Mods/EnshroudedModLoader/worlds/`, beside the installed mod packages.
`ModLoader/mod-loader.log` remains the log location. `worlds` is reserved for
configuration and excluded from package discovery.

Close the game and run the complete release's `Install.cmd`, in standalone or
Kiln mode. It automatically migrates **all files** from the old
`ModLoader/worlds/`, including `.mods`, `.settings-v2` and legacy `.settings`
subdirectories, preserving their relative paths and exact bytes.

The installer checks all conflicts first, creates a verified backup under
`RandomWorld/worlds-backup.<id>/`, then copies and verifies each file. Old files
are retired only after successful loader installation and complete verification.
Existing identical files are accepted; destination-only files stay untouched.
Different bytes at the same relative path stop the update, preserving both
copies. Resolve that explicit conflict before retrying; neither version wins
silently. Linked paths are refused.

An interrupted copy can be resumed by rerunning the installer. If installation
fails, originals remain available to the previous loader. If cleanup is
interrupted, the new loader refuses unresolved legacy files rather than starting
with potentially different choices. Rerun Install.cmd to finish. Empty legacy
directories are harmless. Uninstall preserves the new profiles and backups.

Do not update only the DLL: use Install.cmd for this migration. Mod IDs and
world IDs are unchanged. The same package works with or without Kiln; its proxy
is preserved in Kiln mode. CarryOn and Lantern need no binary change.

Back up `Mods/EnshroudedModLoader/worlds/` with your game saves. Steam Cloud does
not synchronize this directory. The profile transfer tool uses the new path;
it still exports activation profiles only, not per-mod settings.

## Validation

Offline tests cover byte-preserving profile/settings migration, Unicode names,
legacy settings, identical/destination-only files, conflicting copies, interrupted
copy retry, installation rollback, backups, repeated updates and uninstall.
Standalone/Kiln transitions, profile transfer, fresh-process world settings,
missing profiles, hardening and catalogue exclusion (8,300 settings files) pass.
The loader refuses unresolved legacy files after a manual/incomplete update.
