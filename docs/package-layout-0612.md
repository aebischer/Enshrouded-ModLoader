# ModLoader 0.6.12: package isolation and failure dialogs

The package root is now `Mods/EnshroudedModLoader`. Package IDs remain relative
to that root: `carryon/carryon.dll` and `lantern/lantern.dll` do not change.
Activation profiles and hashed per-world settings files are not rewritten.

Automatic discovery requires a DLL and adjacent manifest with the
`ENSHROUDED_PACKAGE_V1` header (UTF-8 BOM accepted). Other manifests and bare DLLs
are ignored. Enabled IDs remain in the catalogue even if missing or invalid,
so a broken enabled package cannot silently disappear. The existing traversal
limits apply only to the dedicated tree; shared framework trees are not visited.

The installer moves identified legacy package directories, including resources,
logs and saved package data. Root-level DLL/manifest pairs are moved as pairs;
unassociated root-level resources are not guessed. Authors should use a private
package directory for such resources. Mixed directories containing foreign DLLs,
linked package trees and destination collisions require manual resolution.
Never merge or overwrite two package copies automatically.

Moves are recorded in the existing installation transaction before execution.
Failed DLL/receipt publication restores package locations as well as the previous
proxy. An interrupted transaction is recovered on the next installer run.
The journal uses UTF-8, including non-ASCII package directory names.

Both UI-failure and startup-failure message boxes use translated titles and bodies
from the 15-language catalogue. UTF-8 is converted to UTF-16 for MessageBoxW.
The error path needs no heap allocation for conversion. Before a verified game
build exposes its language, or if the native locale pointers cannot be read,
dialogs fall back to English. Technical logs remain in English.

## Offline validation

- Loader and both gameplay-mod build scripts compile with the new output paths.
- Catalogue test: more than 8,300 neighbouring framework files, foreign manifest,
  bare DLL, nested package, refresh, enabled missing/invalid package.
- Localization: 15 complete catalogues, all four dialog strings round-trip through
  UTF-16, unknown language and unavailable/invalid native locale fallback.
- SDK example, package validator, profiles, settings, new-world policy, dependency
  hardening and the 0.6.7/0.6.8 regression suites pass.
- Installer: fresh install/reinstall/uninstall, foreign proxy protection, invalid
  PE refusal, receipt-lock rollback, interrupted recovery.
- Package migration: root pairs, nested resources, Lantern markers, unchanged
  world choices/settings, Unicode names, partial interrupted moves, collision
  and mixed-folder refusal.
- CarryOn SDK/selection/transport policy tests and Lantern equipment, 400
  save/load cycles, localization and content tests pass.

In-game visual and linguistic review remains a player check. No world or
character save is edited by this update.
