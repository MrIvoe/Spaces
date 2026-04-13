# New Solution Playbook

Use this when creating a new plugin solution, companion tool, or extension package in the Spaces ecosystem.

## Scope Decision

Before creating a new solution, choose the correct home:

- belongs in Spaces host: core lifecycle or platform contract work
- belongs in Spaces-Plugins: optional user-facing extension behavior
- belongs in Themes: token/theme authoring and distribution

If feature can be implemented as plugin, prefer plugin first.

## New Plugin Solution Workflow

1. Start from plugin template in Spaces-Plugins
2. Define manifest and compatibility ranges
3. Implement plugin class and capabilities
4. Add settings page only for user-facing controls
5. Validate via Spaces host integration
6. Package for marketplace flow

## New Host Feature Workflow

1. Confirm feature cannot be pluginized safely
2. Add or update host contract boundaries first
3. Keep consumer settings minimal and preset-based
4. Add diagnostics hooks
5. Update manual checklists and wiki docs

## Repository Integration Checklist

- Spaces changes link to plugin/theme repositories when relevant
- Plugin repository docs reference host API requirements
- Theme repository changes preserve token contract compatibility
- Release notes across repos stay consistent

## Build and Validation Matrix

Minimum validation for cross-solution work:

1. Spaces Debug build passes
2. HostCoreTests pass
3. Manual runtime smoke checklist passes
4. Settings persistence checklist passes
5. Relevant plugin/theme validation scripts pass

## Definition of Ready (Before Coding)

- clear user outcome
- selected repository and ownership
- API compatibility impact assessed
- rollout and fallback plan defined

## Definition of Done (Before Release)

- build and tests green
- docs updated in README and wiki
- migration paths documented for settings or contracts
- release notes updated with user-visible impact
