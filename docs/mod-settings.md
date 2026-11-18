# Per-world mod settings

The normative contract is [SDK V1](../sdk/STANDARD_V1.txt). This document explains
storage and UI behavior in ModLoader 0.6.11.

A `.dll.mod` manifest declares settings without executing DLL code in the menu:

```ini
setting=messages|bool|Test messages|true
setting=effects|bool|Test effects|false
setting=intensity|percent|Test intensity|100|0|200|25
setting=mode|choice|Test mode|normal|slow|normal|fast
```

Fields are key, type, label, default, then allowed values for choices.
Keys are unique `[a-z0-9_]`, at most 40 characters. Limits: 16 settings,
2–8 distinct choices, 100 bytes per field, strict `true`/`false` booleans.
Invalid UTF-8 and inconsistent schemas are rejected. Libraries cannot declare settings.

The UI uses native difficulty components: selectable rows, hover highlighting,
labels on the left and values between arrows. Booleans display localized
Disabled/Enabled. Choice arrows disappear at endpoints. Native scrolling uses
the game's gold scrollbar. Selectors save on change; percentage sliders save
on release. Refresh reloads the files.

Percentages use integer default/min/max/step values from 0 to 10000. The step
must divide the range, the default must lie on a step, and there must be 2–32
positions. The native slider receives a ratio; storage retains the integer
percentage. Missing newly added keys use their defaults without changing others.
The native `c3a` flag enables selector arrows (introduced in 0.6.2).

## Storage

Current path:
`ModLoader/worlds/<GUID>.settings-v2/<SHA256-normalized-UTF8-mod-id>.ini`.
The digest is 64 lowercase hexadecimal characters. Content remains
`ENSHROUDED_SETTINGS_V1`. Legacy `<GUID>.settings/<mod-path.dll>.ini` files are
moved under locks on first read without overwriting an existing destination.
Migration errors leave settings available for retry; corrupt data remains an
error rather than silently becoming defaults.

Each world/mod pair has independent settings. Missing file = defaults; present
but invalid file = launch refusal. Reads are bounded to 16 KiB and values are
validated. Writes use an exclusive lock, compare against the last read state,
flush a unique temporary file, then replace atomically. Disabled mods keep their
settings. Deleting a settings file and refreshing restores defaults. Activation
profiles `.mods` are independent.

Back up the entire `ModLoader/worlds` directory. The activation-profile transfer
utility transports only `.mods`, not settings, save data or DLLs.

## Initialization and restart

Mods with settings must export C-linked
`int EnshroudedModConfigure(const char* world_id, const char* settings_utf8)`.
The loader validates the PE export and all settings before loading the first
DLL, then calls Configure followed by Initialize. Configure receives sorted
`key=value` lines, including defaults. It must return 1 and copy borrowed data
if retaining it. ABI 1 context layout is unchanged.

Different settings for a resident mod require restarting. Restoring the exact
loaded values removes this requirement. Settings are not applied live.
Test_A/Test_B merely log configuration and world IDs; their settings have no
gameplay effects and the fixtures are not installed in player packages.

Tests cover world/mod isolation, defaults, supported types, invalid values,
write conflicts, disabled-mod persistence, immediate saves, restart/reversion,
fresh processes with different settings, corrupted configuration and duplicate
schema rejection. Native control appearance still requires visual game testing.

All custom loader labels and errors follow the selected game language. See
[localization](../sdk/LOCALISATION.txt); saved keys and values are never translated.
