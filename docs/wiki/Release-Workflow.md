# Release Workflow

## Goals

- predictable consumer release quality
- clean versioning discipline
- repeatable packaging

## Standard Flow

1. Implement changes in focused commits.
2. Build Debug and validate core tests.
3. Build Release and verify executable output.
4. Run manual smoke and settings persistence checks.
5. Update release notes and changelog.
6. Package installer.
7. Publish release artifacts.

## Version Bump Checklist

When bumping public release versions, update:

- `src/AppVersion.h`
- `installer/Spaces.iss`
- versioned docs referencing installer filename

## Verification Checklist

- App launches and tray menu works
- New Space action works
- Space move/resize works
- Drag/drop into Space works
- Persistence reload works on restart
- Settings persist and reload correctly

## References

- [../../CHANGELOG.md](../../CHANGELOG.md)
- [../MANUAL_RUNTIME_SMOKE_TEST.md](../MANUAL_RUNTIME_SMOKE_TEST.md)
- [../MANUAL_SETTINGS_PERSISTENCE_CHECKLIST.md](../MANUAL_SETTINGS_PERSISTENCE_CHECKLIST.md)
- [../../installer/README.md](../../installer/README.md)
