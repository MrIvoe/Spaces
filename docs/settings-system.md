# Settings System

## Principles

- Backward-compatible key evolution with explicit migration.
- Host-rendered settings UI for consistency and safety.
- Safe defaults and validation for all persisted values.

## Current Capabilities

- Plugin settings page registration via host contracts.
- Field descriptors for typed rendering and persistence.
- Theme key migration and canonical bridge keys.

## Roadmap Alignment

Planned incremental expansion:

- grouped categories (basic/advanced)
- min/max and string length validation
- restart-required indicators
- conditional visibility
- search/filter and import/export/reset-all

Implementation references:

- src/extensions/PluginSettingsRegistry.h
- src/core/SettingsStore.h
