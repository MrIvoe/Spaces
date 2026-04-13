# Plugin Quickstart

This page is for first-time plugin authors.

Outcome: you will create a plugin skeleton, wire it into Spaces, build, and verify settings persistence.

## Prerequisites

- Spaces repo cloned and buildable
- Spaces-Plugins repo cloned
- Visual Studio 2022 Build Tools
- CMake

## Step 1: Copy the Plugin Template

Template source:

- Spaces-Plugins repository
- folder: plugin-template

Copy:

- from: Spaces-Plugins/plugin-template
- to: Spaces-Plugins/plugins/<your-plugin-name>

## Step 2: Rename Template Symbols

Follow this checklist:

1. Rename TemplatePlugin.h and TemplatePlugin.cpp
2. Rename class TemplatePlugin to your class name
3. Replace manifest id community.template_plugin with a globally unique id
4. Replace template page and key names like template.general
5. Update plugin.json metadata

Reference:

- [https://github.com/MrIvoe/Spaces-Plugins/tree/main/plugin-template](https://github.com/MrIvoe/Spaces-Plugins/tree/main/plugin-template)

## Step 3: Set Manifest Correctly

Minimum manifest quality bar:

- id: unique and stable
- displayName: user-friendly
- version: semantic version
- minHostApiVersion and maxHostApiVersion: valid host compatibility range
- capabilities: only what plugin actually uses

Common capability examples:

- commands
- settings_pages
- menu_contributions
- space_content_provider

## Step 4: Implement Plugin Class

Your class must implement:

- GetManifest
- Initialize
- Shutdown

The host contract is in:

- [../../src/extensions/PluginContracts.h](../../src/extensions/PluginContracts.h)

Key context services available during Initialize:

- commandDispatcher
- settingsRegistry
- menuRegistry
- spaceExtensionRegistry
- appCommands

## Step 5: Add Settings Page (Recommended)

Use host schema types from:

- [../../src/extensions/SettingsSchema.h](../../src/extensions/SettingsSchema.h)

Field types:

- Bool
- Int
- String
- Enum

Persistence is automatic when using the settings registry.

## Step 6: Register Commands and Menu Contributions

Typical flow:

1. Register command handlers with commandDispatcher
2. Add tray/space menu items with menuRegistry
3. Ensure command ids are stable and namespaced

Example namespace pattern:

- pluginid.action.name

## Step 7: Wire Plugin into Host for Local Validation

During active development, ensure plugin is included in host startup wiring and build definitions.

Core references:

- [../../src/plugins/builtins/BuiltinPlugins.cpp](../../src/plugins/builtins/BuiltinPlugins.cpp)
- [../../docs/EXTENSIBILITY.md](../../docs/EXTENSIBILITY.md)

## Step 8: Build and Test

Build Spaces (Debug):

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
```

Run tests:

```powershell
.\build\Debug\HostCoreTests.exe
```

Manual checks:

- plugin loads without host crash
- plugin settings page renders
- setting changes persist after restart
- plugin command executes from intended menu surface

Manual checklist reference:

- [../MANUAL_SETTINGS_PERSISTENCE_CHECKLIST.md](../MANUAL_SETTINGS_PERSISTENCE_CHECKLIST.md)

## Step 9: Package and Publish Direction

For distribution workflows and marketplace lifecycle:

- [https://github.com/MrIvoe/Spaces-Plugins/blob/main/docs/MARKETPLACE_PUBLISHING.md](https://github.com/MrIvoe/Spaces-Plugins/blob/main/docs/MARKETPLACE_PUBLISHING.md)

## Frequent Mistakes

- Duplicate plugin id
- Unsupported host API range
- Missing enum options for Enum fields
- Command id mismatch between registration and menu contribution
- Writing internal-only implementation settings into consumer pages

## Definition of Done

A plugin is ready when:

1. Host loads it without faults
2. Commands and menus function correctly
3. Settings persist through restart
4. Compatibility metadata is accurate
5. Basic docs exist for usage and troubleshooting
