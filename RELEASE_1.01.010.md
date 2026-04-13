# Spaces 1.01.010 Release Notes

## Summary

Spaces 1.01.010 focuses on usability and release readiness:

- simplified default settings surface for consumers
- consumer-first README rewrite with accurate install and safety guidance
- detailed developer wiki onboarding for plugin and solution development
- installer documentation improvements with explicit validation checklist

## Included Changes

### Consumer Experience

- README is now consumer-first and easier to follow for install and daily usage.
- Data storage and troubleshooting sections were corrected to match runtime behavior.

### Settings UX

- Default settings surface was reduced to core, preset-driven controls.
- Advanced and internal variables were removed from default settings pages.

### Developer Experience

- Added detailed wiki pages for:
  - plugin quickstart
  - plugin deep-dive development practices
  - debugging and troubleshooting
  - new solution playbook

### Installer Quality

- Installer docs now align with current release and output paths.
- Added concrete installer validation steps for reliable release checks.

## Versioning

- App version: 1.01.010
- Installer version: 1.01.010

## Verification Performed

- Debug build: passed
- Release build: passed
- HostCoreTests: passed
- Installer compilation (`ISCC`): passed
- Installer artifact produced: `installer/output/Spaces-Setup-1.01.010.exe`

## Upgrade Notes

- Existing settings remain compatible.
- Settings UI is intentionally simplified for consumer workflows.
- No migration action required for existing users.
