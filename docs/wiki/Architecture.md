# Architecture

## High-Level Model

Spaces is a Win32 host application with a protected core and extension-capable platform services.

Core responsibilities:

- Space lifecycle
- file safety and restore semantics
- persistence and startup recovery
- host diagnostics

Platform responsibilities:

- command dispatch
- plugin loading and capability wiring
- settings page registration
- theme application bridge

## Primary Components

- `App` and `AppKernel`: startup/lifecycle and service composition
- `SpaceManager`: canonical space state and orchestration
- `SpaceWindow`: per-space UI host
- `SpaceStorage`: file move/restore/delete behavior
- `PluginHost` and registries: extension contracts and contribution surfaces
- `ThemePlatform`: token/palette/materialized UI rendering values

## Key Documentation

- [../architecture.md](../architecture.md)
- [../EXTENSIBILITY.md](../EXTENSIBILITY.md)
- [../plugin-system.md](../plugin-system.md)
- [../THEME_SYSTEM_DESIGN.md](../THEME_SYSTEM_DESIGN.md)
- [../THEME_CONTRACT.md](../THEME_CONTRACT.md)

## Design Rules

- Keep core behavior deterministic and recoverable.
- Avoid exposing advanced internals as default user settings.
- Maintain compatibility shims for legacy settings keys during simplification.
- Prefer explicit diagnostics over silent failure.
