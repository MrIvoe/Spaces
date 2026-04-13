# Release 1.01.004 - Build Output Workflow Release

## Overview

Spaces is now version **1.01.004**.

This release standardizes the local testing workflow by ensuring every build publishes the latest app executable to the installer output directory.

## Included in 1.01.004

- Added a post-build rule in `CMakeLists.txt` to always copy the built app executable to:
  - `installer/output/Spaces.exe`
- Bumped app and installer version to `1.01.004`
- Updated latest release references in user/developer documentation

## Expected Test Artifact

```text
installer/output/Spaces.exe
```

## Expected Installer Artifact

```text
installer/output/Spaces-Setup-1.01.004.exe
```
