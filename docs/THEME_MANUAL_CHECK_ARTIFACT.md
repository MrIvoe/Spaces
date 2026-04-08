# Theme Manual Verification Artifact

Date: 2026-04-07
Scope: Restart stability evidence for legacy-state and canonical migrated-state settings.

## Pass A: Legacy-State Double Restart

Settings path: `%LOCALAPPDATA%/Spaces/settings.json`

### Snapshot: Before

```json
{
  "version": 1,
  "values": {
    "appearance.theme.style": "aurora_light",
    "appearance.theme.mode": "dark"
  }
}
```

### Snapshot: After Restart 1

```json
{
  "version": 1,
  "values": {
    "appearance.theme.style": "aurora_light",
    "appearance.theme.mode": "dark"
  }
}
```

### Snapshot: After Restart 2

```json
{
  "version": 1,
  "values": {
    "appearance.theme.style": "aurora_light",
    "appearance.theme.mode": "dark"
  }
}
```

Result: stable across two restarts (no unintended rewrites in this legacy-state run).

## Pass B: Canonical Migrated-State Double Restart

Pre-seeded canonical settings used for run:

```json
{
  "version": 1,
  "values": {
    "theme.source": "win32_theme_system",
    "theme.win32.theme_id": "aurora-light",
    "theme.win32.display_name": "Aurora Light",
    "theme.win32.catalog_version": "2026.04.06",
    "theme.preset": "aurora-light",
    "theme.migration_v2_complete": "true",
    "appearance.theme.style": "aurora_light",
    "appearance.theme.mode": "dark"
  }
}
```

### Snapshot: Before

```json
{
  "version": 1,
  "values": {
    "theme.source": "win32_theme_system",
    "theme.win32.theme_id": "aurora-light",
    "theme.win32.display_name": "Aurora Light",
    "theme.win32.catalog_version": "2026.04.06",
    "theme.preset": "aurora-light",
    "theme.migration_v2_complete": "true",
    "appearance.theme.style": "aurora_light",
    "appearance.theme.mode": "dark"
  }
}
```

### Snapshot: After Restart 1

```json
{
  "version": 1,
  "values": {
    "theme.source": "win32_theme_system",
    "theme.win32.theme_id": "aurora-light",
    "theme.win32.display_name": "Aurora Light",
    "theme.win32.catalog_version": "2026.04.06",
    "theme.preset": "aurora-light",
    "theme.migration_v2_complete": "true",
    "appearance.theme.style": "aurora_light",
    "appearance.theme.mode": "dark"
  }
}
```

### Snapshot: After Restart 2

```json
{
  "version": 1,
  "values": {
    "theme.source": "win32_theme_system",
    "theme.win32.theme_id": "aurora-light",
    "theme.win32.display_name": "Aurora Light",
    "theme.win32.catalog_version": "2026.04.06",
    "theme.preset": "aurora-light",
    "theme.migration_v2_complete": "true",
    "appearance.theme.style": "aurora_light",
    "appearance.theme.mode": "dark"
  }
}
```

Result: stable across two restarts with canonical keys present (no rewrite regression detected).

## Safety Note

Original local settings file was restored after the canonical pre-seeded run.
