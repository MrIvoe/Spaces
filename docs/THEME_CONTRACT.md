# Cross-Repo Theme Contract

Status: active
Contract version: 1.0.0
Updated: 2026-04-08

## Purpose

Define the public contract between Themes, Spaces, and Spaces-Plugins for token bundles, semantic mappings, compatibility, and fallback behavior.

## Source Of Truth

- Themes is the canonical source for token schema and semantic mapping schema.
- Spaces consumes exported semantic outputs and applies platform adapters.
- Spaces-Plugins consume host-provided semantic tokens and do not write raw palette values.

## Canonical Namespaces

- `mrivoe.theme`
- `mrivoe.theme.tokens`
- `mrivoe.theme.semantic`
- `mrivoe.theme.win32` (adapter-specific surface)

## Token Bundle Contract

Producer: Themes

Required:
- Stable token paths with semantic meaning.
- Exported resolved bundle for consumer adapters.
- Deterministic output for identical inputs.

Validation:
- Token colors must be valid hex values.
- Required token groups must exist.

## Semantic Mapping Contract

Producer: Themes

Required:
- Every semantic key must resolve to an existing token path.
- Semantic keys are stable and versioned by contract version.

Consumer behavior:
- Consumers may add local aliases but must not mutate canonical semantic keys.

## Compatibility Rules

- Major contract changes require a contract version bump.
- Minor changes may add new optional semantic keys.
- Patch changes are non-breaking clarifications or validation tightening that does not break valid packages.

## Fallback Rules

- Missing semantic key: use consumer default value and log diagnostic.
- Unknown theme id: use documented default theme id and log diagnostic.
- Invalid package payload: reject apply, keep current valid theme active.

## Error Handling Expectations

- No partial apply states for runtime theme activation.
- Validation failures must include actionable reason text.
- Consumers must prefer last-known-good theme state on failures.

## Spaces Consumer Notes

- Spaces runtime may keep Win32-specific implementation details internally.
- Internal key names are implementation details and are not part of this public contract unless explicitly listed above.
