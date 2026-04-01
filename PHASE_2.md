# Phase 2 - Reliability and Data Safety Hardening

Version range: 0.0.007 to 0.0.008.

## Goals

- Make file operations recoverable and transparent.
- Improve persistence correctness for special characters and malformed input.
- Strengthen shutdown and metadata consistency behavior.

## Implementations in 0.0.007

- Moved persistence to `nlohmann/json` for robust serialization/deserialization.
- Removed hardcoded user-specific logging paths.
- Added temp-file save flow for config and origins.
- Centralized restore/delete logic in `FenceStorage`.
- Added non-destructive restore naming (`(restored N)`).
- Added structured move results for drop operations.
- Improved geometry persistence and long-path drag/drop handling.
- Improved tray registration robustness and shutdown cleanup.

## Implementations in 0.0.008

- Origin metadata now writes only after successful move completion.
- Atomic file replacement now uses Win32-safe replace/write-through semantics.
- Fence deletion aborts on partial restore failure to preserve recoverability.
- Added keyboard-triggered context menu support with sensible anchors.
- Reused cached system image list in paint loop to avoid repeated shell lookups.
- Expanded logging detail for move/copy/delete/restore/atomic-save failures.

## Key Imports and Dependencies

- JSON: `nlohmann/json`.
- Win32 APIs: `MoveFileExW`, shell and common control APIs.
- C++ STL: `filesystem`, `system_error`, `fstream`, `vector`, `map`.

## Reliability Principles Applied

- Never overwrite user files during restore.
- Never remove fence model on partial restore failure.
- Never write stale origins for failed file moves.
- Prefer atomic replace over delete-then-rename.
- Log failures with enough context to diagnose real-world problems.

## Outcome

- The project is now safer for real desktop testing with stronger metadata integrity, safer restore/delete behavior, and improved operational diagnostics.
