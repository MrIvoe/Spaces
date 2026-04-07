# Public Theme Authoring Guide for SimpleFences

**Version**: 1.0.0  
**Status**: Production  
**Updated**: April 2026

## Overview

SimpleFences now supports public theme authoring through a **stable, versioned, open contract**. Third-party authors can create and distribute custom themes without modifying the SimpleFences host code.

## Before You Start

- **Compatibility**: Your theme package must target Win32ThemeSystem (the only valid system).
- **Stability**: Once published, maintain backward compatibility. Themes are immutable once released.
- **Validation**: Your theme will be validated against the contract before acceptance by users.
- **Support**: Guidelines, examples, and feedback are available at https://github.com/MrIvoe/IVOESimpleFences.

## Theme Package Structure

Create a `.zip` archive with the following structure:

```
my-theme-package.zip
├── theme-metadata.json        (required)
├── theme/
│   ├── tokens/
│   │   └── default.json       (required)
│   ├── assets/
│   │   ├── preview.png        (optional)
│   │   └── readme.md          (optional)
│   └── tokens/
│       ├── dark.json          (optional override)
│       └── light.json         (optional override)
```

### File: theme-metadata.json

Metadata about your theme. **Required.**

```json
{
  "themeId": "my-custom-theme",
  "displayName": "My Custom Theme",
  "version": "1.0.0",
  "author": "Your Name",
  "description": "A beautiful custom theme for SimpleFences.",
  "website": "https://example.com",
  "tokenNamespace": "win32_theme_system",
  "minimumHostVersion": "1.0.0",
  "maximumHostVersion": "2.0.0"
}
```

**Field Reference**:

| Field | Type | Required | Notes |
|-------|------|----------|-------|
| `themeId` | string | **Yes** | Unique identifier: lowercase, hyphens only, 1-100 chars. Example: `my-custom-theme` |
| `displayName` | string | **Yes** | Human-readable name shown in UI. Max 100 chars. |
| `version` | string | **Yes** | Semantic version (X.Y.Z). Examples: `1.0.0`, `2.1.3` |
| `author` | string | No | Your name or organization. Max 200 chars. |
| `description` | string | No | Brief description. Max 500 chars. |
| `website` | string | No | URL to your website or documentation. |
| `tokenNamespace` | string | **Yes** | Must be `win32_theme_system`. No other values are valid. |
| `minimumHostVersion` | string | No | Minimum SimpleFences version required. Defaults to `1.0.0` |
| `maximumHostVersion` | string | No | Maximum SimpleFences version supported. Defaults to `99.0.0` |

### File: theme/tokens/default.json

The token map defining your theme colors. **Required.**

This JSON maps semantic token names to hex color values. SimpleFences uses these tokens to render all UI surfaces.

**Minimal Example**:

```json
{
  "win32.base.window_color": "#202124",
  "win32.base.surface_color": "#2A2B2E",
  "win32.base.nav_color": "#1C2128",
  "win32.base.text_color": "#F5F7FA",
  "win32.base.subtle_text_color": "#8B9DC3",
  "win32.base.accent_color": "#5090F6",
  "win32.base.border_color": "#44484E",
  "win32.fence.title_bar_color": "#272D35",
  "win32.fence.title_text_color": "#C7D2DF",
  "win32.fence.item_text_color": "#ADBAC7",
  "win32.fence.item_hover_color": "#373F48"
}
```

**Full Token List**:

All tokens in the `win32.*` namespace are recognized by SimpleFences:

```
win32.base.window_color          — Main app window background
win32.base.surface_color         — Secondary surface (cards, panels)
win32.base.nav_color             — Navigation bar / sidebar background
win32.base.text_color            — Primary text color
win32.base.subtle_text_color     — Secondary/disabled text color
win32.base.accent_color          — Highlight / focus color
win32.base.border_color          — Divider / border color

win32.fence.title_bar_color      — Fence title bar background
win32.fence.title_text_color     — Fence title bar text
win32.fence.item_text_color      — Fence item text color
win32.fence.item_hover_color     — Fence item hover state background
```

**Color Values**:

- Format: Hex RGB, e.g., `#RRGGBB` (uppercase or lowercase)
- Examples: `#202124`, `#ffffff`, `#0078d4`
- All three channels required (no short form `#fff`)

**Validation Rules**:

- Every token is optional; missing tokens use host defaults
- Extra tokens outside `win32.*` are silently ignored
- Invalid colors are skipped; defaults apply
- Tokens must be valid hex colors; malformed entries are rejected

### Optional Overrides: theme/tokens/dark.json and theme/tokens/light.json

If your theme adapts to light/dark mode, provide mode-specific overrides:

```
theme/tokens/default.json    — Applied always
theme/tokens/dark.json       — Applied additional when in dark mode
theme/tokens/light.json      — Applied additional when in light mode
```

Dark/light tokens are merged with `default.json` (dark/light override defaults).

### Optional: theme/assets/preview.png

A 1280×720 PNG preview image shown in theme selection UI.

### Optional: theme/assets/readme.md

A Markdown file with extended documentation, usage tips, or attribution.

## Theme ID Guidelines

- **Format**: lowercase alphanumerics and hyphens only
- **Length**: 1–100 characters
- **Must be unique**: No two themes may have the same ID
- **Immutable**: Once published, never change your theme ID

Examples of valid IDs:

```
my-custom-theme
dark-mode-pro
glass-morphism
retro-80s
high-contrast-a11y
corporate-branding
```

Examples that are **invalid**:

```
My-Custom-Theme          (uppercase not allowed)
my_custom_theme          (underscores not allowed)
my custom theme          (spaces not allowed)
my-custom-theme---x      (consecutive hyphens, excess length)
```

## Distribution

1. **Create your theme package** following the structure above.
2. **Validate** your package: use SimpleFences' built-in validator or inspect manually.
3. **Host distribution**: Publish your `.zip` file on your own server or GitHub Releases.
4. **Document**: Provide installation instructions to users.
5. **Install in SimpleFences**: Users download your `.zip` and use the built-in package installer.

## Examples

### Example: "Midnight Studio" Theme

```json
// theme-metadata.json
{
  "themeId": "midnight-studio",
  "displayName": "Midnight Studio",
  "version": "1.0.0",
  "author": "Design Studio Co.",
  "description": "A dark, professional theme inspired by modern design studios.",
  "website": "https://midnight-studio.example.com",
  "tokenNamespace": "win32_theme_system"
}

// theme/tokens/default.json
{
  "win32.base.window_color": "#0f1419",
  "win32.base.surface_color": "#1a1f28",
  "win32.base.nav_color": "#131820",
  "win32.base.text_color": "#e4e6eb",
  "win32.base.subtle_text_color": "#8b95a5",
  "win32.base.accent_color": "#00d9ff",
  "win32.base.border_color": "#2a2f38",
  "win32.fence.title_bar_color": "#1a1f28",
  "win32.fence.title_text_color": "#ffffff",
  "win32.fence.item_text_color": "#d0d5dd",
  "win32.fence.item_hover_color": "#252d38"
}
```

### Example: "Light & Airy" Theme (Light-Mode Focused)

```json
// theme-metadata.json
{
  "themeId": "light-airy",
  "displayName": "Light & Airy",
  "version": "1.0.0",
  "author": "Designer XYZ",
  "description": "Clean, minimal light theme perfect for daytime use."
}

// theme/tokens/default.json (used in both light and dark modes by default)
{
  "win32.base.window_color": "#fafbfc",
  "win32.base.surface_color": "#ffffff",
  "win32.base.nav_color": "#f0f3f6",
  "win32.base.text_color": "#1f2937",
  "win32.base.subtle_text_color": "#6b7280",
  "win32.base.accent_color": "#3b82f6",
  "win32.base.border_color": "#e5e7eb",
  "win32.fence.title_bar_color": "#e5e7eb",
  "win32.fence.title_text_color": "#1f2937",
  "win32.fence.item_text_color": "#374151",
  "win32.fence.item_hover_color": "#f3f4f6"
}

// theme/tokens/dark.json (override for dark mode, merged with default.json)
{
  "win32.base.window_color": "#1f2937",
  "win32.base.surface_color": "#2d3748",
  "win32.base.nav_color": "#111827",
  "win32.base.text_color": "#f3f4f6",
  "win32.base.accent_color": "#93c5fd"
}
```

## Validation & Safety

### Automatic Validation

SimpleFences validates all theme packages before applying:

1. **File integrity**: Archive is valid and readable.
2. **Metadata completeness**: `theme-metadata.json` contains required fields.
3. **Size limits**: Package ≤ 10MB total.
4. **Token schema**: `default.json` JSON is well-formed.
5. **Security**: No executable files, scripts, or dynamic payloads.
6. **Color format**: All color tokens are valid hex RGB.

### Security Rules

Your theme package **may NOT contain**:

- Executable files (`.exe`, `.dll`, `.scr`, `.sys`)
- Scripts (`.bat`, `.ps1`, `.vbs`, `.js`)
- Compressed archives within the theme package
- Symbolic links or junction points
- Absolute file paths
- Any dynamic/template syntax (`${...}`, `{{...}}`, etc.)

### Rejection Reasons

Packages are rejected if they:

- Have an invalid or duplicate `themeId`
- Use a `tokenNamespace` other than `win32_theme_system`
- Exceed 10MB in size
- Contain forbidden file types
- Have malformed JSON in token files
- Specify impossible version requirements (e.g., min > max)

### Diagnostic Messages (Current Host)

When validation fails, SimpleFences surfaces these concrete diagnostics:

| Failure Path | Diagnostic Message |
|-------|-------|
| Package path empty | `Package path is empty` |
| Package file missing | `Package file not found` |
| File is not zip | `Theme package must be a .zip file` |
| Package exceeds size cap | `Package exceeds maximum size (10MB)` |
| Extraction or parse failed | `Package extraction/parsing failed` (or specific loader error) |
| Missing/invalid metadata | `Invalid or incomplete theme metadata` |
| Invalid theme id format | `theme_id must be kebab-case and 1-100 chars` |
| Forbidden payload present | `Package contains forbidden content (executables, scripts)` |
| Required token coverage missing | `Token map is missing required base tokens` |

Notes for authors:

- Required base coverage currently includes window, text, and accent tokens.
- Token values must be valid `#RRGGBB`; malformed values are ignored and can cause required-token rejection.
- Supported metadata keys are snake_case (`theme_id`, `display_name`, `version`, etc.); camelCase variants are rejected.

## Best Practices

1. **Test on multiple Windows versions**: Ensure your colors look good on Windows 10 and Windows 11.
2. **Accessibility**: Use sufficient contrast for text tokens (WCAG AA minimum).
3. **Document your palette** in `readme.md` with color names and use cases.
4. **Version semantically**: Use MAJOR.MINOR.PATCH format.
5. **Preview image**: Provide a helpful `preview.png` so users can see your theme before installing.

## Compatibility

- **Windows versions**: Windows 10 (build 19041) +
- **SimpleFences versions**: 1.0.0+ (always check `minimumHostVersion`/`maximumHostVersion`)
- **Token namespace**: Only `win32_theme_system` is valid now and in the foreseeable future.

## Support & Feedback

- **Issues**: Report validation failures or contract questions in GitHub Issues.
- **Discussions**: Share theme ideas and feedback in GitHub Discussions.
- **Documentation updates**: Help improve this guide with pull requests.

---

**Happy theme authoring! 🎨**
