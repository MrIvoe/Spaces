# Spaces Developer Wiki

This wiki is the developer entry point for Spaces.

## Start Here

1. [Build and Test](Build-and-Test.md)
2. [Plugin Quickstart](Plugin-Quickstart.md)
3. [Plugin Development Deep Dive](Plugin-Development-Deep-Dive.md)
4. [Architecture](Architecture.md)
5. [Settings and Persistence](Settings-and-Persistence.md)
6. [Plugins and Themes](Plugins-and-Themes.md)
7. [New Solution Playbook](New-Solution-Playbook.md)
8. [Debugging and Troubleshooting](Debugging-and-Troubleshooting.md)
9. [Release Workflow](Release-Workflow.md)

## Purpose

Spaces has two audiences:

- Consumers who need a stable desktop organizer.
- Developers who need clear extension and maintenance workflows.

The README stays consumer-first. This wiki holds engineering detail.

## Engineering Principles

- Prefer safe defaults.
- Preserve backward compatibility for settings keys when possible.
- Keep plugin/theme integration isolated behind host contracts.
- Favor recoverable behavior over destructive behavior.

## Recommended Learning Path For New Developers

Week 1 fundamentals:

1. Build the host and run HostCoreTests
2. Read architecture and extensibility model
3. Complete Plugin Quickstart with a small plugin

Week 2 productivity:

1. Add settings and command/menu contributions
2. Validate persistence and reload behavior
3. Practice debugging checklist for common failures

Week 3 production readiness:

1. Apply deep-dive quality guidance
2. Package and validate a plugin release flow
3. Document changes and update release workflow artifacts

## Related Documentation

- [../overview.md](../overview.md)
- [../architecture.md](../architecture.md)
- [../plugin-system.md](../plugin-system.md)
- [../themes.md](../themes.md)
- [../settings-system.md](../settings-system.md)
