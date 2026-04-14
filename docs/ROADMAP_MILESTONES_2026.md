# Spaces Roadmap Milestones (2026)

This roadmap converts the improvement checklist into milestone-driven delivery.

## Priority Order

1. Settings window polish
2. Core behavior verification
3. User-facing error handling and recovery UX
4. Competitor comparison and feature benchmarking
5. User-demand research from forums/reviews
6. Standout feature planning
7. Standout feature implementation

## Milestone M1: Settings Polish

Goal: settings experience feels complete, readable, and intentional.

Scope:

- layout hierarchy (header, nav, content spacing)
- control polish (buttons, rows, labels, badges)
- feedback states (error/warning/success/info)
- usability (scanability, keyboard focus, reduced ambiguity)

Exit criteria:

- no obvious visual regressions at common window sizes
- consistent spacing/typography patterns across all settings pages
- settings pages can be navigated quickly without confusion

## Milestone M2: Core Reliability Verification

Goal: prove core behavior is dependable and repeatable.

Scope:

- Space lifecycle (create, move, resize, rename, delete)
- persistence/reload behavior
- drag/drop and restore safety behavior
- startup/shutdown and config recovery behavior

Exit criteria:

- all critical manual checks pass
- no high-severity reliability defects open
- startup and restore behavior match documented expectations

## Milestone M3: Error Handling and Recovery UX

Goal: user-visible errors are clear and actionable.

Scope:

- move/restore failure messages
- plugin load/compatibility failure surfaces
- blocked deletion explanations
- restart-required and validation messaging

Exit criteria:

- common failures have plain-language user feedback
- no critical failures are logs-only for normal user workflows

## Milestone M4: Competitor Analysis

Goal: extract practical lessons from fence-style apps.

Scope:

- comparison matrix for UX/functionality
- identify quick wins vs long-term opportunities
- define what not to copy

Exit criteria:

- comparison matrix completed
- top adoption opportunities documented with rationale

## Milestone M5: User Demand Research

Goal: prioritize features using real user signals.

Scope:

- collect recurring complaints and requests
- aggregate findings by category
- create top-requested and top-complaint lists

Exit criteria:

- research summary completed
- roadmap priorities updated with evidence

## Milestone M6: Standout Feature Design

Goal: define features that make Spaces better, not just similar.

Scope:

- first-impression upgrades
- organization and safety differentiators
- plugin/theme experience improvements

Exit criteria:

- shortlisted standout features with effort/impact estimates
- implementation order approved

## Milestone M7: Standout Feature Implementation

Goal: ship highest-impact differentiators with release quality.

Scope:

- implementation of approved standout features
- release validation and communication

Exit criteria:

- release candidate passes build/test/installer checklist
- release notes include clear user benefit statements

## First Batch (Immediate Start)

1. Audit settings window and annotate rough spots with screenshots
2. Build issue list for settings layout/spacing/hierarchy polish
3. Run core reliability verification checklist end-to-end
4. Create bug backlog for failures discovered
5. Begin competitor feature matrix and user request collection
