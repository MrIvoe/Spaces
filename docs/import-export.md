# Import and Export

## Current State

Import/export responsibilities remain host-owned for safety.

- persisted settings and Space metadata are host-managed
- migration runs before first render when required
- fallback behavior protects startup usability

## Expansion Direction

- explicit backup points before destructive import operations
- profile/template bundle support with validation
- rollback-safe import flow
