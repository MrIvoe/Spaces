# Plugin Marketplace Architecture

This document defines the migration from repository sync to a true in-app plugin marketplace.

## Goals

- Replace git-based plugin sync as the default end-user workflow.
- Let users browse and install plugins inside Spaces.
- Enable compatible auto-updates for installed plugins.
- Keep strong host safety validation before activation.

## Current vs Target

Current flow:

- `PluginHubSync` uses `git.exe` clone/fetch.
- Host mirrors all `plugins/*` folders from repo cache.

Target flow:

1. Host fetches marketplace `catalog.json`.
2. UI displays plugin entries (Discover/Installed/Updates/Disabled).
3. User installs selected plugins only.
4. Host downloads package ZIP from `downloadUrl`.
5. Host verifies hash + compatibility + manifest + capability policy.
6. Host extracts package safely to `%LOCALAPPDATA%/SimpleSpaces/plugins/<id>/`.
7. Host enables plugin now or on restart depending on `restartRequired`.

## Catalog Contract

The host consumes plugin entries containing:

- `id`
- `displayName`
- `author`
- `description`
- `category`
- `version`
- `channel`
- `downloadUrl`
- `hash`
- `compatibility.hostVersion.min/max`
- `compatibility.hostApiVersion.min/max`
- `capabilities`
- `supportsSettingsPage`
- `restartRequired`

Host parser: `src/core/PluginCatalogFetcher.h` and `src/core/PluginCatalogFetcher.cpp`.

## Marketplace UI Model

Settings plugin marketplace pages:

- Discover
- Installed
- Updates
- Disabled

Actions:

- search
- category filter
- install
- uninstall
- enable
- disable
- update
- view details

UI integration status:

- Settings keys are available now.
- Detailed page rendering and command wiring are next implementation step.

## Install and Update Behavior

Install:

- Manual user choice only.
- New plugin stays disabled if policy requires review.

Update:

- Only for installed plugins.
- Auto-update allowed when enabled and compatible.
- Backup previous version before replace.
- Apply immediately only when safe; otherwise queue for restart.

Rollback:

- Store prior package/install folder snapshot when `settings.plugins.keep_backup_versions=true`.

## Safety Rules

Host must validate before activation:

- manifest validity
- host compatibility
- host API compatibility
- package hash match
- safe extraction path
- capability policy checks

Required helper components:

- `src/core/PluginPackageInstaller.h`
- `src/core/PluginPackageInstaller.cpp`

## Settings Plan

Marketplace/update controls:

- `settings.plugins.marketplace_enabled`
- `settings.plugins.catalog_url`
- `settings.plugins.allow_preview`
- `settings.plugins.show_incompatible`
- `settings.plugins.auto_update_installed`
- `settings.plugins.update_channel`
- `settings.plugins.check_on_startup`
- `settings.plugins.apply_updates_on_restart`
- `settings.plugins.keep_backup_versions`

Configured via built-in settings pages in `src/plugins/builtins/BuiltinPlugins.cpp`.

## Replacement Plan for Git Sync

Phase 1 (now):

- Keep `PluginHubSync` for developer fallback.
- Introduce catalog + package installer contracts.

Phase 2:

- Route end-user plugin install/update actions to marketplace downloader.
- Hide git sync path behind developer/advanced switch.

Phase 3:

- Make marketplace path primary and git path optional legacy maintenance tool.

## Spaces-Plugins Publishing Dependency

Marketplace publishing output is produced by Spaces-Plugins CI:

- `catalog/catalog.json`
- `packages/*.zip`

Publishing flow reference:

- Spaces-Plugins `docs/MARKETPLACE_PUBLISHING.md`
