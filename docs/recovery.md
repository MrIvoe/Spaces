# Recovery

## Core Recovery Rules

- broken plugin should not prevent startup
- broken theme should not make UI unusable
- malformed persistence should recover safely

## Current Recovery Paths

- theme fallback to graphite-office for unknown/invalid IDs
- conflict guard disables non-canonical appearance selector write paths
- persistence and migration behavior are validated in HostCoreTests

## Planned Safe-Mode Extensions

- startup with plugins disabled
- startup with custom themes disabled
- forced diagnostics mode
- last-known-good plugin/theme state selection
