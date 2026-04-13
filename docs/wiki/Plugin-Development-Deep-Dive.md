# Plugin Development Deep Dive

This page explains advanced plugin engineering patterns for robust production plugins.

## Contract-First Development

Start from host contracts, not implementation assumptions.

Primary references:

- [../../src/extensions/PluginContracts.h](../../src/extensions/PluginContracts.h)
- [../../src/extensions/PluginSettingsRegistry.h](../../src/extensions/PluginSettingsRegistry.h)
- [../../src/extensions/SettingsSchema.h](../../src/extensions/SettingsSchema.h)
- [../EXTENSIBILITY.md](../EXTENSIBILITY.md)

## Capability Design

Declare only required capabilities.

Examples:

- commands only plugin: avoid settings_pages capability if not used
- settings-only plugin: avoid menu capabilities if no UI entry point
- content provider plugin: include space_content_provider and implement refresh/state paths

Why: smaller blast radius and easier compatibility analysis.

## Command Design Guidelines

- Use deterministic, namespaced command ids.
- Treat command handlers as failure boundaries.
- Never assume active space context exists.
- Validate all external input before state mutation.

Good pattern:

1. Read context from appCommands
2. Validate state
3. Execute
4. Emit diagnostics for warnings/failures

## Settings Design Guidelines

For every setting key:

- define default that is safe
- define clear key namespace
- avoid exposing implementation internals to consumers
- use enums for bounded choices

Migration pattern:

1. new key read first
2. fallback to legacy key
3. mirror value during transition
4. remove legacy path after validation period

## Diagnostics and Observability

Use host diagnostics intentionally:

- startup success/failure summaries
- invalid configuration warnings
- command failure context with key identifiers

Never crash host due to plugin issues.

## Failure Isolation Expectations

Host behavior includes plugin fault isolation, but plugin code must still:

- guard external operations
- avoid throwing uncaught exceptions across boundaries
- fail closed for risky actions

## Content Provider Plugins

If plugin supplies custom space content:

- keep provider id stable
- implement enumerate, drop, and delete semantics predictably
- map provider state to user-facing recoverable states
- support refresh without forcing host restart

## Security and Safety

- Treat file operations as high-risk.
- Prefer non-destructive defaults.
- Validate paths and ownership assumptions.
- Avoid hidden side effects in command handlers.

## Performance Guidance

- avoid heavy work on UI paths
- debounce polling/refresh operations where needed
- cache expensive results with clear invalidation strategy
- measure before and after when changing refresh cadence

## Review Checklist

Before merge:

1. Manifest compatibility verified
2. Settings keys reviewed for naming and defaults
3. Commands validated against all menu entry points
4. Startup and shutdown paths tested
5. Persistence behavior tested across restart
6. Regression tests or manual proof added
