# launcher.exe nopatch note

Canonical documentation for the original `-nopatch` path now lives at:

- `nopatch/README.md`

## Short version

`-nopatch` skips the patch pipeline, but it still performs launcher-side initialization before `InitClientDLL`.
That launcher-side setup is part of the behavior we need to reproduce.
