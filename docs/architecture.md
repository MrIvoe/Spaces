# Architecture

## Core Direction

The host remains core-first and stability-focused.

Boundaries:

- Host: lifecycle, persistence, rendering shell, settings host, diagnostics, recovery, plugin runtime guardrails.
- Plugins: optional features, commands, providers, settings pages, and extension behaviors.
- Theme platform: Win32ThemeSystem-backed canonical theme application with host-owned rendering.

## Current Theme Integration Notes

- Canonical source: theme.source = win32_theme_system
- Canonical selector path: community.visual_modes
- Canonical key: theme.win32.theme_id
- Fallback: graphite-office

Detailed references:

- docs/THEME_SYSTEM_DESIGN.md
- docs/PUBLIC_THEME_AUTHORING.md
- docs/MANUAL_THEME_VERIFICATION.md
