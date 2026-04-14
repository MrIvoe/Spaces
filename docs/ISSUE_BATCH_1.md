# Issue Batch 1 (High Priority)

Use these as copy/paste starters for the first execution batch.

## 1) Settings UI Audit With Annotated Screenshots

Title:
`Settings UI audit with annotated screenshots`

Labels:
`area:settings`, `priority:high`, `difficulty:easy`

Body:

```markdown
## Goal
Create a complete visual and usability audit of the settings window.

## Scope
- Capture screenshots of every settings page and major state.
- Annotate rough/unpolished areas directly on screenshots.
- Separate visual issues from functional issues.

## Deliverables
- A screenshot set covering all settings pages.
- A categorized issue list:
  - layout
  - spacing
  - hierarchy
  - control styling
  - navigation
  - responsiveness
  - clarity

## Acceptance Criteria
- Every page/state has at least one screenshot.
- Each screenshot has clear annotations for pain points.
- Findings are grouped and prioritized for Milestone M1.
```

## 2) Redesign Settings Header And Hierarchy

Title:
`Redesign settings header, subtitle, and section hierarchy`

Labels:
`area:settings`, `priority:high`, `difficulty:medium`

Body:

```markdown
## Goal
Make settings feel like a polished product page with clear hierarchy.

## Scope
- Improve header title/subtitle structure.
- Improve visual distinction of page sections.
- Standardize heading sizes and spacing.

## Acceptance Criteria
- Header area is visually consistent across settings pages.
- Section hierarchy is immediately scannable.
- No overlapping/clipping on common window sizes.
```

## 3) Improve Settings Left Navigation Polish

Title:
`Improve settings left navigation layout and scanability`

Labels:
`area:settings`, `priority:high`, `difficulty:medium`

Body:

```markdown
## Goal
Make settings navigation clearer and easier to scan.

## Scope
- Improve nav spacing, grouping, and item alignment.
- Improve active-state visual clarity.
- Reduce rough default-control appearance where possible.

## Acceptance Criteria
- Active page is always visually obvious.
- Navigation remains readable at smaller window sizes.
- Keyboard focus is visible and consistent.
```

## 4) Standardize Settings Spacing And Row Layout

Title:
`Standardize settings page spacing, padding, and row layout`

Labels:
`area:settings`, `priority:high`, `difficulty:medium`

Body:

```markdown
## Goal
Create consistent visual rhythm across all settings pages.

## Scope
- Standardize section spacing and row gap.
- Align labels, control columns, and descriptions.
- Ensure long pages scroll cleanly.

## Acceptance Criteria
- Spacing values are consistent across pages.
- No crowded or overly sparse sections.
- Scroll behavior is stable and smooth.
```

## 5) Core Space Lifecycle Verification Sweep

Title:
`Core Space lifecycle verification sweep (create/move/resize/rename/delete)`

Labels:
`area:core`, `priority:high`, `difficulty:medium`

Body:

```markdown
## Goal
Verify core space behavior is dependable and matches expected behavior.

## Scope
- Create single/multiple spaces.
- Move, resize, rename, and delete spaces.
- Verify startup reload and geometry restore.

## Acceptance Criteria
- All lifecycle actions pass manual checks.
- Any failures are logged as follow-up bugs with repro steps.
- No high-severity regressions remain open.
```

## 6) File Move/Restore Safety Verification Sweep

Title:
`File move and restore safety verification sweep`

Labels:
`area:filesafety`, `priority:high`, `difficulty:medium`

Body:

```markdown
## Goal
Validate non-destructive file safety and restore behavior.

## Scope
- Move files/folders into spaces.
- Restore to original path.
- Test name conflicts on restore.
- Validate partial restore behavior and space deletion safeguards.

## Acceptance Criteria
- No destructive overwrite behavior.
- Conflict naming works consistently.
- Partial restore blocks destructive space deletion.
```

## 7) Improve User-Facing Error Messages For File Operations

Title:
`Improve user-facing file move/restore failure messaging`

Labels:
`area:filesafety`, `area:diagnostics`, `priority:high`, `difficulty:medium`

Body:

```markdown
## Goal
Make failure states understandable for non-technical users.

## Scope
- Improve messages for move failures.
- Improve messages for restore failures/conflicts.
- Include next-step guidance where practical.

## Acceptance Criteria
- Common file operation failures are no longer logs-only.
- Messages explain what happened and what user can do next.
```

## 8) Plugin Failure Visibility In Settings

Title:
`Improve plugin load/compatibility failure visibility in settings`

Labels:
`area:plugins`, `area:diagnostics`, `priority:high`, `difficulty:medium`

Body:

```markdown
## Goal
Make plugin failure states clear and actionable in settings.

## Scope
- Surface compatibility and load state clearly.
- Improve wording for failed/disabled/incompatible statuses.
- Add quick guidance for recovery steps where possible.

## Acceptance Criteria
- Plugin status is understandable without reading logs.
- Users can distinguish failed vs incompatible vs disabled states.
```
