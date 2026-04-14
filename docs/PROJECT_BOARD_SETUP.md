# GitHub Project Board Setup

Use this structure to track roadmap execution.

## Board Name

Spaces Roadmap 2026

## Columns

1. Backlog
2. Planned
3. In Progress
4. Testing
5. Done

## Field Suggestions

- Milestone (M1..M7)
- Area (settings/core/filesafety/plugins/themes/diagnostics/research/roadmap)
- Priority (high/medium/low)
- Difficulty (easy/medium/hard)
- Risk (high/medium/low)

## Workflow Rules

- New issues start in Backlog.
- Move to Planned only when acceptance criteria are clear.
- Move to In Progress only when owner is assigned.
- Move to Testing only when implementation is complete and build passes.
- Move to Done only after verification and documentation updates.

## Weekly Review Agenda

1. Review high-priority blocked items
2. Review milestone burn-down
3. Confirm test and release gate status
4. Re-rank backlog based on new user research

## Release Gate Requirement

Before any release PR/merge:

- run `scripts/release-checklist.ps1`
- ensure installer artifact path exists and matches release version
- ensure no non-runtime paths are included in publish changes

## Suggested Milestone Mapping

- M1: Settings polish
- M2: Core verification
- M3: Error/recovery UX
- M4: Competitor analysis
- M5: User research
- M6: Standout feature planning
- M7: Standout implementation
