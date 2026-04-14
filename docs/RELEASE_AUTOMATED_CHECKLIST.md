# Automated Release Checklist

Use this script before each release publish:

- builds app (Debug + Release)
- builds installer
- confirms installer artifact path
- blocks publish if non-runtime paths appear in git status

## Script

- `scripts/release-checklist.ps1`

## Run

From repo root:

```powershell
./scripts/release-checklist.ps1
```

Optional flags:

```powershell
./scripts/release-checklist.ps1 -SkipDebugBuild
./scripts/release-checklist.ps1 -SkipTests
```

## What It Verifies

1. Required files exist:
   - `build/Spaces.slnx`
   - `installer/Spaces.iss`
2. App build:
   - Debug build (unless skipped)
   - Release build
   - Release executable exists at `build/bin/Release/Spaces.exe`
3. Tests:
   - `build/Debug/HostCoreTests.exe` runs (unless skipped)
4. Installer build:
   - uses `BUILD_OUTPUT_DIR=build/bin/Release`
   - compiles `installer/Spaces.iss`
   - verifies output: `installer/output/Spaces.<version>.exe`
5. Non-runtime upload guard:
   - fails if git status includes paths under:
     - `build/`
     - `out/`
     - `.vs/`
     - `installer/output/`
     - `docs/wiki/`
     - `crashdumps/`

## Notes

- Script reads `MyAppVersion` directly from `installer/Spaces.iss` so artifact verification always matches release version.
- Keep `.gitignore` aligned with non-runtime folders to reduce accidental uploads.
