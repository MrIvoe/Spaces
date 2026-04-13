# Plugins and Themes

## Ecosystem Repositories

- Spaces host app: https://github.com/MrIvoe/Spaces
- Plugins: https://github.com/MrIvoe/Spaces-Plugins
- Themes: https://github.com/MrIvoe/Themes

## Plugin Model

Plugins contribute behavior through host contracts:

- commands
- settings pages
- menu/tray contributions
- content provider integrations

Start here for plugin development:

- [Plugin Quickstart](Plugin-Quickstart.md)
- [Plugin Development Deep Dive](Plugin-Development-Deep-Dive.md)
- [Debugging and Troubleshooting](Debugging-and-Troubleshooting.md)

Reference docs:

- [../plugin-system.md](../plugin-system.md)
- [../PLUGIN_MARKETPLACE_ARCHITECTURE.md](../PLUGIN_MARKETPLACE_ARCHITECTURE.md)

## Theme Model

Themes are token-driven with Win32-compatible application in host runtime.

Reference docs:

- [../themes.md](../themes.md)
- [../THEME_SYSTEM_DESIGN.md](../THEME_SYSTEM_DESIGN.md)
- [../THEME_CONTRACT.md](../THEME_CONTRACT.md)
- [../PUBLIC_THEME_AUTHORING.md](../PUBLIC_THEME_AUTHORING.md)

## Integration Guidelines

- Keep plugin defaults conservative.
- Validate external package inputs before apply.
- Use fallback paths for token/theme resolution.
- Gate risky actions behind explicit user action.

## First Plugin Path (Recommended)

1. Create plugin from template in Spaces-Plugins
2. Implement manifest and initialize path
3. Add one command and one settings page
4. Integrate into host build/startup path
5. Validate persistence and command routing
6. Package and document behavior

## Cross-Repo Development Rules

- Host contracts evolve in Spaces first
- Plugin behavior and packaging live in Spaces-Plugins
- Token and theme package authoring live in Themes
- Avoid hardcoding theme literals in plugin UI when semantic tokens are available
