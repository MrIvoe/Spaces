# Spaces UI Modernization TODO
VSCode-inspired appearance improvements. Pure Win32, theme-aware.

## Phase 1: Core Visual Polish (Current)
- [x] TODO.md created

## Phase 2: SettingsWindow Enhancements (VSCode Layout)
- [x] 1. Add Menu Bar (File/Edit/View) - ownerdraw toolbar
- [x] 2. Sidebar icons polish (Segoe glyphs via resolver)
- [x] 3. Content search bar + filter chips
- [x] 4. Marketplace sub-tabs (Discover/Installed)
- [x] 5. Plugin tree view (expandable)

## Phase 3: SpaceWindow Explorer Polish
- [x] 6. Title bar toolbar (gear settings, view toggle)
- [x] 7. Responsive grid cols + drag reorder
- [x] 8. Breadcrumbs + density slider

## Phase 4: Global UX
- [x] 9. Animations (fade/slide via layered + timers)
- [x] 10. Command Palette (Ctrl+Shift+P fuzzy)
- [x] 11. Loading spinners/badges
- [x] 12. Keyboard nav + accessibility


## Phase 5: Performance (Priority NOW)
- [ ] 1. Virtual NavList (paint-only, no HWNDs/item)
- [ ] 2. Double buffered pane WM_PAINT

## Phase 6: Advanced Perf
- [ ] 3. Virtual fields (rendered, not HWND)
- [ ] 4. Async layout/resize (timer coalesce)

**Next**: Implement Virtual NavList in SettingsWindow

