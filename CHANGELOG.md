# Changelog

## 0.0.013

- Theme system consolidated to Win32ThemeSystem with backward-compatible migration and public theme package support.
- Added runtime conflict prevention for appearance selector ownership (community.visual_modes canonical path).
- Added failure-focused validation tests for malformed/invalid third-party theme packages.
- Added rapid-switch debounce and telemetry counters for migration/apply lifecycle observability.
- Added telemetry smoke snapshot output in HostCoreTests for release verification.
- Added plugin compatibility status/reason surfacing in host registry and diagnostics output.
- Added read-only Plugin Manager scaffold page in settings with per-plugin compatibility visibility.
- Added Plugin Manager scaffold filters for status and text-based plugin triage.

## Notes

- Versioning currently remains in the unstable track (0.0.xxx).
- Future milestone transitions should follow the ecosystem versioning policy from MASTER PROMPT V3.
