# MyOS Manual QA Checklist

Run after automated run_qa.sh passes. Use QEMU windowed mode.

## Visual & Interaction Checks

- [ ] **Window drag**
  - Open 3 windows from taskbar.
  - Drag each window by title bar across desktop.
  - Verify no tearing, no ghost artifacts, smooth motion.
  - Serial log shows no WARN/FAIL during drag.

- [ ] **Snap layouts**
  - Open an app window.
  - Use Win+Z or snap hotkey to snap to left/right/half.
  - Window resizes and repositions via animation.
  - Verify damage rects stay small (<25 avg) in PERF log.

- [ ] **Task View**
  - Press Win+Tab.
  - Task view opens with thumbnails for all open windows.
  - Thumbnails update live while dragging a window.
  - Switch windows via click; focus changes correctly.

- [ ] **4 virtual desktops**
  - Open 2 windows on Desktop 1.
  - Switch to Desktop 2 (Ctrl+Alt+Right).
  - Open 2 different windows.
  - Switch back to Desktop 1; original windows state preserved.
  - No cross-desktop bleed.

- [ ] **All 5 apps open simultaneously**
  - Launch: browser, file_explorer, settings, terminal, gui_hello.
  - Each app opens, renders, responds to input.
  - Taskbar shows 5 entries, no overlap.
  - Resize windows; layout remains correct.

- [ ] **Rapid typing**
  - Open terminal.
  - Type 1000 characters quickly.
  - Verify no character loss, scrollback smooth.
  - Input ring buffer does not overflow; no serial warnings.

## Perf Bar
- FPS >= 30 with all 5 apps open and one window being dragged.
- Damage rects < 25 avg during drag.
- Zero serial errors in 10-minute soak.

Notes:
- Record serial log excerpt for each check.
- If any item fails, capture qa.log and PERF output.
