# GitHub Issue Seed List

Use this file to create issues quickly. Each item includes suggested labels.

Label categories:

- area:settings, area:core, area:filesafety, area:plugins, area:themes, area:diagnostics, area:research, area:roadmap
- priority:high, priority:medium, priority:low
- difficulty:easy, difficulty:medium, difficulty:hard

## Milestone M1: Settings Polish

1. Settings UI audit with annotated screenshots
- Labels: area:settings, priority:high, difficulty:easy

2. Redesign settings header and subtitle hierarchy
- Labels: area:settings, priority:high, difficulty:medium

3. Improve settings left-nav spacing and visual grouping
- Labels: area:settings, priority:high, difficulty:medium

4. Standardize section padding and row spacing across pages
- Labels: area:settings, priority:high, difficulty:medium

5. Improve button hierarchy for primary/secondary actions
- Labels: area:settings, priority:medium, difficulty:medium

6. Add polished empty states for plugin/settings pages
- Labels: area:settings, area:plugins, priority:medium, difficulty:easy

7. Improve keyboard navigation and focus visibility in settings
- Labels: area:settings, priority:high, difficulty:hard

8. Add restart-required inline notices for relevant settings
- Labels: area:settings, area:diagnostics, priority:medium, difficulty:easy

## Milestone M2: Core Reliability Verification

9. Create master expected-behavior checklist for core features
- Labels: area:core, area:roadmap, priority:high, difficulty:easy

10. Validate full Space lifecycle behavior (create/move/resize/rename/delete)
- Labels: area:core, priority:high, difficulty:medium

11. Verify startup reload and persisted geometry correctness
- Labels: area:core, priority:high, difficulty:medium

12. Verify malformed config recovery behavior and messaging
- Labels: area:core, area:diagnostics, priority:high, difficulty:medium

13. Verify duplicate-instance behavior and user messaging
- Labels: area:core, area:diagnostics, priority:medium, difficulty:easy

## Milestone M3: Error and Recovery UX

14. Improve file-move failure messages for non-technical users
- Labels: area:filesafety, area:diagnostics, priority:high, difficulty:medium

15. Improve restore conflict explanation and next-step guidance
- Labels: area:filesafety, area:diagnostics, priority:high, difficulty:medium

16. Surface plugin load failures clearly in user-facing settings
- Labels: area:plugins, area:diagnostics, priority:high, difficulty:medium

17. Add actionable explanation when Space deletion is blocked
- Labels: area:filesafety, area:diagnostics, priority:medium, difficulty:easy

## Milestone M4: Competitor Analysis

18. Build competitor matrix for fence-style applications
- Labels: area:research, priority:medium, difficulty:medium

19. Compare settings UX against top competitors
- Labels: area:research, area:settings, priority:medium, difficulty:easy

20. Publish adoption recommendations from competitor analysis
- Labels: area:research, area:roadmap, priority:medium, difficulty:medium

## Milestone M5: User Demand Research

21. Collect top user complaints from forums/reviews
- Labels: area:research, priority:high, difficulty:medium

22. Collect top requested features from communities
- Labels: area:research, priority:high, difficulty:medium

23. Build prioritized top-requested feature list with evidence
- Labels: area:research, area:roadmap, priority:high, difficulty:medium

## Milestone M6-M7: Standout Features

24. Define standout organization feature candidates
- Labels: area:roadmap, priority:medium, difficulty:medium

25. Define standout safety/recovery feature candidates
- Labels: area:filesafety, area:roadmap, priority:high, difficulty:medium

26. Improve plugin manager discoverability and recommendation UX
- Labels: area:plugins, priority:medium, difficulty:hard

27. Add better first-run onboarding and defaults
- Labels: area:settings, area:core, priority:high, difficulty:hard

28. Implement first approved standout feature from roadmap
- Labels: area:roadmap, priority:high, difficulty:hard

## Operations and Quality

29. Track roadmap milestones in project board with weekly review
- Labels: area:roadmap, priority:high, difficulty:easy

30. Enforce release gate using automated release checklist script
- Labels: area:roadmap, area:diagnostics, priority:high, difficulty:easy
