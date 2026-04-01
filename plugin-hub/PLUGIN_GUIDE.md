# Plugin Development Guide

This guide explains how to write a SimpleFences plugin from scratch.

## Minimum requirements

- Visual Studio 2019+ or VS Build Tools with the C++ workload  
- SimpleFences source code checked out locally  
- C++17

---

## 1  Create `plugin.json`

Every plugin must ship a `plugin.json` manifest:

```json
{
  "id": "community.my_awesome_plugin",
  "displayName": "My Awesome Plugin",
  "version": "1.0.0",
  "description": "Short one-line description visible in the Settings Hub.",
  "author": "Your Name",
  "minHostVersion": "0.0.010",
  "capabilities": ["fence_content_provider"]
}
```

### Required keys

| Key | Type | Description |
|---|---|---|
| `id` | string | Unique reverse-domain id. Prefix with `community.` for hub plugins. |
| `displayName` | string | Name shown in the settings left rail. |
| `version` | string | Semantic version string. |
| `minHostVersion` | string | Minimum SimpleFences version the plugin works with. |
| `capabilities` | array | One or more capability strings (see table in README). |

---

## 2  Implement `IPlugin`

```cpp
// MyPlugin.h
#pragma once
#include "extensions/PluginContracts.h"

class MyPlugin final : public IPlugin
{
public:
    PluginManifest GetManifest() const override;
    bool Initialize(const PluginContext& context) override;
    void Shutdown() override;
};
```

```cpp
// MyPlugin.cpp
#include "MyPlugin.h"
#include "extensions/PluginSettingsRegistry.h"
#include "extensions/SettingsSchema.h"

PluginManifest MyPlugin::GetManifest() const
{
    PluginManifest m;
    m.id          = L"community.my_awesome_plugin";
    m.displayName = L"My Awesome Plugin";
    m.version     = L"1.0.0";
    m.capabilities = {L"fence_content_provider"};
    return m;
}

bool MyPlugin::Initialize(const PluginContext& context)
{
    // Register a settings page with interactive controls
    if (context.settingsRegistry)
    {
        PluginSettingsPage page;
        page.pluginId = L"community.my_awesome_plugin";
        page.pageId   = L"my_plugin.general";
        page.title    = L"General";
        page.order    = 10;

        // Bool toggle
        SettingsFieldDescriptor enableCoolFeature;
        enableCoolFeature.key          = L"my_plugin.general.enable_cool_feature";
        enableCoolFeature.label        = L"Enable cool feature";
        enableCoolFeature.description  = L"Activates the really cool feature.";
        enableCoolFeature.type         = SettingsFieldType::Bool;
        enableCoolFeature.defaultValue = L"true";
        enableCoolFeature.order        = 10;
        page.fields.push_back(std::move(enableCoolFeature));

        // Enum dropdown
        SettingsFieldDescriptor mode;
        mode.key          = L"my_plugin.general.mode";
        mode.label        = L"Operating mode";
        mode.type         = SettingsFieldType::Enum;
        mode.defaultValue = L"fast";
        mode.options      = {
            {L"fast",  L"Fast"},
            {L"safe",  L"Safe"},
            {L"debug", L"Debug"},
        };
        mode.order = 20;
        page.fields.push_back(std::move(mode));

        context.settingsRegistry->RegisterPage(std::move(page));
    }

    // Read back a persisted value
    if (context.settingsRegistry)
    {
        const bool coolEnabled = (context.settingsRegistry->GetValue(
            L"my_plugin.general.enable_cool_feature", L"true") == L"true");
        // use coolEnabled …
    }

    return true;
}

void MyPlugin::Shutdown() {}
```

---

## 3  Register the plugin with the host

In `BuiltinPlugins.cpp` (or a future loader), add:

```cpp
#include "plugins/community/my_awesome_plugin/MyPlugin.h"

// Inside CreateBuiltinPlugins():
plugins.push_back(std::make_unique<MyPlugin>());
```

---

## 4  Supported field types

| `SettingsFieldType` | Control rendered | `defaultValue` format |
|---|---|---|
| `Bool` | Checkbox | `"true"` or `"false"` |
| `Int` | Numeric edit box | Decimal integer string, e.g. `"60"` |
| `String` | Text edit box | Any string |
| `Enum` | Drop-down combobox | One of the option `value` strings |

Values are persisted automatically to  
`%LOCALAPPDATA%\SimpleFences\settings.json` whenever a control changes.

---

## 5  Naming conventions

| Thing | Convention |
|---|---|
| Plugin id | `community.<snake_case>` |
| Field key | `<plugin_short_name>.<page>.<field>` |
| Source files | `PascalCasePlugin.h` / `.cpp` |
| `plugin.json` folder | `plugins/<kebab-case-plugin-name>/` |

---

## 6  Testing your plugin

```powershell
# From the repo root
cmake --build build --config Debug
.\build\bin\Debug\SimpleFences.exe
```

Right-click the tray icon → **Settings** → find your plugin in the left rail.  
Toggle a control; exit; relaunch — the value should persist.
