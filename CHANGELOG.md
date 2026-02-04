# Changelog

## [1.1.0] - 2026-02-03

### Added
- **Animated Backgrounds**:
  - `Space`: Features a warp-speed starfield effect.
  - `Matrix`: Digital rain effect with falling binary characters.
- **Hybrid UI System**:
  - Implemented a "Solid-State" rendering engine to eliminate flickering on transparent backgrounds.
  - Interactive inputs (Buttons, Edit boxes) are now rendered opaque for stability.
  - Decorative elements (Borders, Labels) remain transparent, preserving the animated backdrop.
- **Auto-Replication Layout**:
  - The application now dynamically scans the resource layout at startup to guarantee 100% pixel-perfect UI positioning.

### Changed
- **Performance**:
  - Increased animation frame rate to 30 FPS for smoother motion.
  - Optimized `WM_PAINT` loop with manual double-buffering.
  - Enabled `WS_CLIPCHILDREN` to prevent redraw conflicts.
- **Theme Improvements**:
  - Fixed `Blueprint` theme to correctly render the grid texture on the main window background.
  - Improved theme switching to force an immediate full-window repaint, correcting color bleeding artifacts.
  - Restored visibility of Radio Button labels in the Hybrid UI mode.

### Fixed
- Resolved major flickering issues caused by standard Windows controls overlapping with high-speed paint loops.
- Fixed text clipping issues where borders were drawn through group box titles.
