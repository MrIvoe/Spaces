# Changelog

## 1.01.010

- Simplified default settings surface to a smaller, preset-first set of pages and controls for general users.
- Rewrote README to a consumer-first structure with accurate install, safety, data-path, and release-publish guidance.
- Expanded developer wiki onboarding with deep plugin creation, troubleshooting, and new-solution playbooks.
- Updated installer documentation and added an explicit installer validation checklist.
- Bumped app and installer version from 1.01.009 to 1.01.010.

## 1.01.009

- Applied `appearance.ui.icon_size` preset to settings icon font rendering, including legacy alias support (`sm/md/lg`).
- Extended AppKernel theme-change propagation to include new preset keys (`opacity_profile`, `transparency_enabled`, `settings_density`, `toggle_size`, `tray_menu_size`, `icon_size`) for live UI updates.
- Bumped app and installer version from 1.01.008 to 1.01.009.

## 1.01.008

- Simplified settings UI to preset-driven controls for general users:
  - icon size now uses `smaller/small/normal/large`
  - transparency now uses an `opacity_profile` preset
  - layout now uses `settings_density`, `toggle_size`, and `tray_menu_size` presets
- Added runtime preset mapping in `ThemePlatform` while keeping backward compatibility with existing numeric settings keys.
- Bumped app and installer version from 1.01.007 to 1.01.008.

## 1.01.007

- Fixed settings text-field rendering by stopping custom field-frame paint from filling over control client content.
- Added listbox color handling in settings right-pane scroll panel so combo dropdown text remains readable across themes.
- Bumped app and installer version from 1.01.006 to 1.01.007.

## 1.01.006

- Fixed a settings-window crash caused by a null `m_navList` dereference in `WM_CTLCOLOR*` handling during early window/control creation.
- Bumped app and installer version from 1.01.005 to 1.01.006.

## 1.01.005

- Hardened settings command routing so `plugin.openSettings` still opens the window if command registration drifts.
- Added explicit settings-open failure logging to avoid silent tray-click no-op behavior.
- Added stale `SettingsWindow` handle detection/recovery so the window is recreated when HWND state is invalid.
- Bumped app and installer version from 1.01.004 to 1.01.005.

## 1.01.004

- Added post-build publish step so every Spaces build copies `Spaces.exe` to `installer/output/Spaces.exe` for testing.
- Bumped app and installer version from 1.01.003 to 1.01.004.

## 1.01.003

- Bumped app and installer version from 1.01.002 to 1.01.003.
- Restored normal tray menu contributions by loading built-in plugins at kernel startup again.
- Kept startup resilient by continuing in degraded mode when individual plugin loads fail.

## 1.01.002

- Bumped the public app and installer version from 1.01.001 to 1.01.002 for the current fix release.
- Aligned installer output naming, install path examples, and release-facing documentation with the new version.
- Updated the plugin package downloader user agent to report the current release version.

## 0.0.013

- Theme system consolidated to Win32ThemeSystem with backward-compatible migration and public theme package support.
- Added runtime conflict prevention for appearance selector ownership (community.visual_modes canonical path).
- Added failure-focused validation tests for malformed/invalid third-party theme packages.
- Added rapid-switch debounce and telemetry counters for migration/apply lifecycle observability.
- Added telemetry smoke snapshot output in HostCoreTests for release verification.
- Added plugin compatibility status/reason surfacing in host registry and diagnostics output.
- Added read-only Plugin Manager scaffold page in settings with per-plugin compatibility visibility.
- Added Plugin Manager scaffold filters for status and text-based plugin triage.
- Expanded settings Diagnostics page with per-plugin triage summaries and compatibility counters.
- Added persisted Plugin Manager enable/disable overrides per plugin id (applied on next plugin host load).
- Added Plugin Manager `apply_now` action to safely reload plugin host and apply overrides immediately.
- Added plugin host reload summary visibility in diagnostics and re-enable reload test coverage.
- Added last plugin host reload UTC timestamp visibility in settings diagnostics.
- Added shared cross-repo theme contract documentation with explicit namespace, compatibility, fallback, and error-handling rules.

## Notes

- Versioning currently remains in the unstable track (0.0.xxx).
- Future milestone transitions should follow the ecosystem versioning policy from MASTER PROMPT V3.
