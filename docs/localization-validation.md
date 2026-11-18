# Localization validation — November 18, 2026

Installed/tested versions: ModLoader 0.6.11, CarryOn 0.1.49-dev, Lantern 0.1.27-dev.
Catalogues contain 15 languages with 69 loader keys, 14 CarryOn keys and 6 Lantern
keys. Exact-language coverage is required; English fallback does not count.

Passed offline checks:

- All three DLL builds and SDK/package validation, including the combined mod set.
- Loader localization, hardening and audit-067 regression tests.
- Catalogue rejection for missing languages/keys, invalid UTF-8, empty text,
  altered markup/placeholders and invalid control characters.
- Complete CarryOn validation: transport, interruptions, teleportation,
  resources, memory, floor corpus and independent model decoding.
- Lantern content, equipment and production native text lookup/format tests,
  including equipment title, language changes and unrelated native passthrough.

Installation was performed with the game closed and backups retained. Installed
hashes matched the compiled files. No automated gameplay test was performed.
New translations still need visual and linguistic review in-game.

English documentation does not change in-game language selection, saved values,
resource IDs or the tested lighting parameters.
