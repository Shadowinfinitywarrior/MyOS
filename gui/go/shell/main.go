// MyOS Go Shell - Phase 3 Desktop Shell
// Provides taskbar, start menu, desktop icons, and notifications.

package main

import (
	"fmt"

	"myos/runtime"
	"myos/shell/graphics"
)

// Window represents a GUI window
type Window struct {
	ID     uint64
	Title  string
	X, Y   int
	W, H   int
	Surface uintptr
}

// ShellState holds the shell's global state
type ShellState struct {
	ScreenW, ScreenH int
	TaskbarH         int
	Windows          []*Window
	FocusedWindow    *Window
	MenuOpen         bool
	DesktopIcons     []*DesktopIcon
}

type DesktopIcon struct {
	App     *AppEntry
	X, Y    int
	Hovered bool
}

type AppEntry struct {
	Name string
	Desc string
	Icon []byte
	Launch func()
}

var shell ShellState

func main() {
	// Get framebuffer info
	var w, h, pitch int
	ret := runtime.GetFramebufferInfo(&w, &h, &pitch)
	if ret != 0 {
		// Fallback
		w, h = 1024, 768
	}
	shell.ScreenW = w
	shell.ScreenH = h
	shell.TaskbarH = 44

	fmt.Printf("Go Shell starting at %dx%d\n", w, h)

	// Initialize desktop icons
	initDesktopIcons()

	// Create initial windows
	createTerminalWindow()
	createAboutWindow()

	// Main event loop
	for {
		processEvents()
		renderFrame()
		runtime.InvalidateWindow(0, 0, 0, shell.ScreenW, shell.ScreenH)
	}
}

func initDesktopIcons() {
	apps := []*AppEntry{
		{Name: "Terminal", Desc: "Shell session", Icon: iconTerminal[:], Launch: launchTerminal},
		{Name: "Files", Desc: "Browse the disks", Icon: iconFiles[:], Launch: launchFiles},
		{Name: "System", Desc: "Processes + memory", Icon: iconMonitor[:], Launch: launchSysinfo},
		{Name: "Help", Desc: "Keys and mouse", Icon: iconHelp[:], Launch: launchHelp},
		{Name: "About", Desc: "System information", Icon: iconAbout[:], Launch: launchAbout},
	}

	shell.DesktopIcons = make([]*DesktopIcon, len(apps))
	for i, app := range apps {
		shell.DesktopIcons[i] = &DesktopIcon{
			App: app,
			X:   26,
			Y:   22 + i*96,
		}
	}
}

func createTerminalWindow() {
	surf := runtime.CreateSurface(800, 600)
	if surf == 0 {
		fmt.Println("Failed to create terminal surface")
		return
	}

	win := &Window{
		ID:      1,
		Title:   "MyOS Terminal",
		X:       50, Y: 50,
		W:       800, H: 600,
		Surface: surf,
	}
	shell.Windows = append(shell.Windows, win)
	shell.FocusedWindow = win

	drawWindowFrame(win)
	drawTerminalContent(win)
	runtime.BlitSurface(surf, win.X, win.Y)
}

func createAboutWindow() {
	surf := runtime.CreateSurface(500, 400)
	if surf == 0 {
		fmt.Println("Failed to create about surface")
		return
	}

	win := &Window{
		ID:      2,
		Title:   "About MyOS",
		X:       200, Y: 150,
		W:       500, H: 400,
		Surface: surf,
	}
	shell.Windows = append(shell.Windows, win)

	drawWindowFrame(win)
	drawAboutContent(win)
	runtime.BlitSurface(surf, win.X, win.Y)
}

func drawWindowFrame(win *Window) {
	surf := graphics.GetDrawSurface(win.Surface)
	if surf == nil {
		return
	}

	// Draw window background
	surf.FillRect(0, 0, win.W, win.H, graphics.Color(0xFF1A1A2E))

	// Draw title bar
	titleBarH := 30
	surf.FillRect(0, 0, win.W, titleBarH, graphics.Color(0xFF2A2A4E))

	// Draw title text
	surf.DrawText(10, 8, win.Title, graphics.Color(0xFFFFFFFF))

	// Draw close button
	closeX := win.W - 30
	surf.FillRoundedRect(closeX, 5, 20, 20, 4, graphics.Color(0xFFCC0000))
	surf.DrawText(closeX+5, 8, "X", graphics.Color(0xFFFFFFFF))

	// Draw border
	surf.DrawRect(0, 0, win.W, win.H, graphics.Color(0xFF3A3A6E), 1)
}

func drawTerminalContent(win *Window) {
	surf := graphics.GetDrawSurface(win.Surface)
	if surf == nil {
		return
	}

	titleBarH := 30
	border := 1

	// Draw terminal background
	surf.FillRect(border, titleBarH+border, win.W-2*border, win.H-titleBarH-2*border, graphics.Color(0xFF0D1117))

	// Draw prompt
	surf.DrawText(12, titleBarH+12, "user@myos:~$ ", graphics.Color(0xFF00FF00))
	surf.DrawText(12, titleBarH+30, "Welcome to MyOS Go Shell!", graphics.Color(0xFFFFFFFF))
	surf.DrawText(12, titleBarH+48, "Type 'help' for available commands.", graphics.Color(0xFF888888))
}

func drawAboutContent(win *Window) {
	surf := graphics.GetDrawSurface(win.Surface)
	if surf == nil {
		return
	}

	titleBarH := 30
	border := 1

	// Draw about background
	surf.FillRect(border, titleBarH+border, win.W-2*border, win.H-titleBarH-2*border, graphics.Color(0xFF1A1A2E))

	// Draw about content
	y := titleBarH + 20
	surf.DrawText(20, y, "MyOS - A Multi-Language Operating System", graphics.Color(0xFFFFFFFF))
	y += 30
	surf.DrawText(20, y, "Phase 3: Go Desktop Shell", graphics.Color(0xFF00FF00))
	y += 24
	surf.DrawText(20, y, "Version: 0.3.0", graphics.Color(0xFFCCCCCC))
	y += 24
	surf.DrawText(20, y, "Components:", graphics.Color(0xFFCCCCCC))
	y += 24
	items := []string{
		"  - Taskbar with glass effect",
		"  - Animated start menu",
		"  - Desktop icons with drag-drop",
		"  - Notification system",
		"  - System widgets",
		"  - Window management",
	}
	for _, item := range items {
		surf.DrawText(20, y, item, graphics.Color(0xFFAAAAAA))
		y += 20
	}
	y += 16
	surf.DrawText(20, y, "Built with TinyGo for bare metal", graphics.Color(0xFF888888))
}

func processEvents() {
	// Process input events from kernel
	// This would read from the kernel event queue
	// For now, just yield
	runtime.Gosched()
}

func renderFrame() {
	// Create a surface for the full screen
	screenSurf := runtime.CreateSurface(shell.ScreenW, shell.ScreenH)
	if screenSurf == 0 {
		return
	}
	surf := graphics.GetDrawSurface(screenSurf)
	if surf == nil {
		return
	}

	// Clear screen
	surf.FillRect(0, 0, shell.ScreenW, shell.ScreenH, graphics.Color(0xFF0D1117))

	// Draw wallpaper gradient
	surf.FillGradientV(0, 0, shell.ScreenW, shell.ScreenH-shell.TaskbarH,
		graphics.Color(0xFF1A1A2E), graphics.Color(0xFF0D1117))

	// Draw desktop icons
	DrawDesktopIcons(surf)

	// Draw taskbar
	DrawTaskbar(surf)

	// Draw start menu
	DrawMenu(surf)

	// Draw notifications
	DrawNotifications(surf)

	// Draw widgets
	DrawWidgets(surf)

	// Blit the composed frame to the framebuffer
	runtime.BlitSurface(screenSurf, 0, 0)
}

func launchTerminal() {
	// Launch terminal app
	fmt.Println("Launching Terminal...")
}

func launchFiles() {
	fmt.Println("Launching Files...")
}

func launchSysinfo() {
	fmt.Println("Launching System Monitor...")
}

func launchHelp() {
	fmt.Println("Launching Help...")
}

func launchAbout() {
	fmt.Println("Launching About...")
}

// Icon data (16x16 1bpp, 2 bytes per row = 32 bytes)
var iconTerminal = [32]byte{
	0x00,0x00, 0xfc,0x3f, 0x04,0x20, 0x04,0x20, 0x04,0x20, 0x04,0x10,
	0x04,0x10, 0x04,0x10, 0x04,0x10, 0xfc,0x3f, 0x00,0x00, 0x10,0x08,
	0x20,0x04, 0x40,0x02, 0x80,0x01, 0x00,0x00,
}

var iconFiles = [32]byte{
	0x00,0x00, 0x00,0x00, 0x78,0x00, 0x48,0x00, 0x48,0x08, 0xc8,0x07,
	0x08,0x04, 0x08,0x04, 0x08,0x04, 0x08,0x04, 0xff,0x0f, 0x08,0x04,
	0x08,0x04, 0x08,0x04, 0x08,0x04, 0xf8,0x07,
}

var iconAbout = [32]byte{
	0x00,0x00, 0xfc,0x3f, 0x04,0x20, 0x04,0x20, 0x04,0x20, 0x04,0x20,
	0x04,0x20, 0x84,0x21, 0xc4,0x23, 0x84,0x21, 0x04,0x20, 0x04,0x20,
	0x04,0x20, 0xfc,0x3f, 0x00,0x00, 0x00,0x00,
}

var iconHelp = [32]byte{
	0x00,0x00, 0x00,0x00, 0xc0,0x00, 0x20,0x01, 0x20,0x01, 0xc0,0x00,
	0x80,0x00, 0x80,0x00, 0x00,0x00, 0x80,0x00, 0x80,0x00, 0x00,0x00,
	0x00,0x00, 0x00,0x00, 0x00,0x00,
}

var iconMonitor = [32]byte{
	0x00,0x00, 0xfc,0x3f, 0x04,0x20, 0xf4,0x2f, 0x14,0x28, 0x14,0x28,
	0x14,0x28, 0x14,0x28, 0x14,0x28, 0xf4,0x2f, 0x04,0x20, 0xfc,0x3f,
	0xc0,0x03, 0x00,0x00, 0x00,0x00, 0x00,0x00,
}