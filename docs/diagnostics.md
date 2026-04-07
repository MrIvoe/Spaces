# Diagnostics

## Logging and Visibility

Diagnostics are designed to be actionable and non-blocking.

Current high-signal areas:

- plugin load failures and compatibility conflicts
- appearance selector conflict enforcement
- theme fallback and apply failure messages
- migration and apply telemetry counters

## Settings Diagnostics Surface

The settings Diagnostics page now includes:

- aggregate counts for failed, disabled, incompatible, compatible, and unknown plugin states
- per-plugin triage rows with state, compatibility status, compatibility reason, and last error

This keeps plugin failure analysis visible in-app without requiring immediate log parsing.

Manual verification references:

- docs/THEME_MANUAL_CHECK_ARTIFACT.md
- docs/MANUAL_THEME_VERIFICATION.md
