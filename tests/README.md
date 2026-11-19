# ModLoader tests

Run `build.cmd` once from the repository root to build native prerequisites.
Then run the desired `tests/test-*.cmd` launcher from any working directory.
Launchers resolve the repository relative to their own location and keep outputs
under `build/`.

- `build-test-mods.cmd`: compile and validate Test_A/Test_B fixtures in `fixtures/`.
  These fixtures are never included in the installer or installed in the game.
- `test-mod-*.cmd` and `test-new-world-profiles.cmd`: regression suites.
- `audit-mod-067.cmd`, `audit-mod-068.cmd`: historical regression assertions.
- `audit-mod-loader.cmd`: static analysis and hardening regression suite.
- `test_install.ps1`, `audit-install.ps1`, `audit-install-format-068.ps1`:
  installer checks in isolated temporary folders; require the pinned game EXE.
- `test_transfer_profile.ps1`: profile export/import round trip in a fixture.

The reusable SDK keeps its self-contained catalogue tests in `sdk/tests/`.
Package validation and profile transfer utilities remain under `tools/`.

`test_package_migration.ps1` exercises the dedicated package directory upgrade,
byte-preserved settings/assets/markers, rollback and interrupted recovery,
foreign packages, mixed directories and destination collisions.
