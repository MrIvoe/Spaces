# Themes

## Canonical Theme Path

The host applies themes through Win32ThemeSystem-driven canonical settings.

- source key: theme.source
- source value: win32_theme_system
- canonical id key: theme.win32.theme_id
- display-only key: theme.win32.display_name

## Safety Behavior

- Unknown IDs fall back to graphite-office.
- Invalid package content is rejected with diagnostics.
- Theme apply is atomic and debounced for rapid switches.

Detailed references:

- docs/THEME_SYSTEM_DESIGN.md
- docs/PUBLIC_THEME_AUTHORING.md
- docs/MANUAL_THEME_VERIFICATION.md
