# Release 1.01.002 - Fix Release

## Overview

**Spaces** has been updated to **version 1.01.002** as the current public fix release.

This release keeps the version-isolated installer model introduced in 1.01.001 and rolls forward the installer, runtime, and UI polish fixes completed in the current stabilization pass.

## Included in 1.01.002

- Installer and app version bumped from `1.01.001` to `1.01.002`
- Installer output renamed to `Spaces-Setup-1.01.002.exe`
- Per-user versioned install path updated to `C:\Users\<user>\AppData\Local\Programs\Spaces\1.01.002\`
- Runtime-facing version metadata updated in the application and plugin package downloader
- Release-facing documentation aligned to the current fix release

## Versioning Policy

Patch releases should continue incrementing in this format:

- `1.01.002`
- `1.01.003`
- `1.01.004`

For each fix release, update these authoritative sources together:

1. `src/AppVersion.h`
2. `installer/Spaces.iss`
3. Any release-facing docs that reference the latest installer filename or install path

## Build Output

After a Release build and installer compile, the expected setup artifact is:

```text
output/Spaces-Setup-1.01.002.exe
```

## Notes

- `RELEASE_1.01.001.md` remains the historical release note for the initial installer release.
- `RELEASE_1.01.002.md` is the current fix-release note.
