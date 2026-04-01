# SimpleFences Plugin Hub

A community repository for SimpleFences plugins — inspired by the [RuneLite plugin hub](https://github.com/runelite/plugin-hub).

## What is a plugin?

A SimpleFences plugin is a C++ class that implements `IPlugin` and is compiled as part of the host executable (or as a future DLL extension). Plugins can:

| Capability | What it does |
|---|---|
| `commands` | Register keyboard/command-bar actions |
| `tray_contributions` | Add items to the system tray menu |
| `fence_content_provider` | Supply a new fence content type |
| `appearance` | Change colours, fonts, or visual style |
| `widgets` | Add embeddable widget panels to fences |
| `desktop_context` | Add context-menu entries to the desktop |
| `settings_pages` | Register settings pages with interactive controls |

## Installing a plugin

1. Clone this repository alongside your SimpleFences workspace.
2. Copy the plugin's `src/` files into `<SimpleFences>/src/plugins/community/<plugin-id>/`.  
3. Register the plugin by calling  `CreateBuiltinPlugins()` (or a future plugin-loader API).
4. Add the new `.h`/`.cpp` files to `CMakeLists.txt` under the `SimpleFences` target.
5. Rebuild.

> **Future:** A DLL-based plugin loader is planned so community plugins can be distributed pre-built without recompiling the host.

## Submitting a plugin

1. Fork this repository.  
2. Create `plugins/<your-plugin-id>/` containing `plugin.json` and your `src/` files.  
3. Open a pull request — see [PLUGIN_GUIDE.md](PLUGIN_GUIDE.md) for required fields.

## Included example plugins

| Plugin | Description |
|---|---|
| [`dark-glass-theme`](plugins/dark-glass-theme/) | Translucent dark theme for fence windows |
| [`network-drive-fence`](plugins/network-drive-fence/) | Fence that maps a UNC / network drive path |
| [`clock-widget`](plugins/clock-widget/) | Live digital clock widget inside a fence |
