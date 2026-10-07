// Menu - Animated start menu with app list

package main

import (
	"time"

	"myos/shell/graphics"
)

const (
	MenuWidth   = 260
	MenuItemH   = 44
	MenuPadding = 16
	AnimDuration = 200 * time.Millisecond
)

var (
	menuAnimStart time.Time
	menuAnimating bool
	menuProgress  float64 // 0.0 to 1.0
)

// ToggleMenu opens/closes the start menu with animation
func ToggleMenu() {
	shell.MenuOpen = !shell.MenuOpen
	menuAnimStart = time.Now()
	menuAnimating = true
}

// UpdateMenuAnimation updates the menu animation progress
func UpdateMenuAnimation() {
	if !menuAnimating {
		return
	}

	elapsed := time.Since(menuAnimStart)
	progress := float64(elapsed) / float64(AnimDuration)

	if progress >= 1.0 {
		progress = 1.0
		menuAnimating = false
	}

	if shell.MenuOpen {
		menuProgress = progress
	} else {
		menuProgress = 1.0 - progress
	}
}

// DrawMenu renders the animated start menu
func DrawMenu(surf *graphics.DrawSurface) {
	if !shell.MenuOpen && menuProgress == 0 {
		return
	}

	UpdateMenuAnimation()

	apps := getApps()
	menuH := len(apps)*MenuItemH + MenuPadding
	mw := MenuWidth
	mx := 8
	my := shell.ScreenH - TaskbarHeight - menuH - 6

	// Apply animation transform (slide up + fade)
	animY := my + int(float64(menuH)*(1.0-menuProgress))
	animAlpha := uint8(255 * menuProgress)

	// Drop shadow
	shadowColor := graphics.Color(0xFF000000 | uint32(animAlpha/4<<24))
	surf.FillRoundedRect(mx+3, animY+5, mw, menuH, 12, shadowColor)

	// Menu background card
	bgColor := graphics.Color(0xFAFFFFFF | uint32(animAlpha<<24))
	surf.FillRoundedRect(mx, animY, mw, menuH, 12, bgColor)

	// Menu border
	borderColor := graphics.Color(0xFF444466)
	surf.DrawRoundedRect(mx, animY, mw, menuH, 12, borderColor, 1)

	// App list
	for i, app := range apps {
		itemY := animY + 8 + i*MenuItemH
		itemX := mx + 8
		itemW := mw - 16

		// Hover highlight
		if isMouseOver(itemX, itemY, itemW, MenuItemH-8) {
			surf.FillRoundedRect(itemX, itemY, itemW, MenuItemH-8, 8, graphics.Color(0xFF2A2A4E))
		}

		// App icon background
		icX := itemX + 16
		icY := itemY + 4
		surf.FillRoundedRect(icX, icY, 32, 32, 7, graphics.Color(0xFF3A3A6E))
		surf.DrawIcon(icX+8, icY+8, app.Icon[:], graphics.Color(0xFFFFFFFF), graphics.ColorTransparent)

		// App name
		surf.DrawText(itemX+58, itemY+4, app.Name, graphics.Color(0xFFFFFFFF))

		// App description
		surf.DrawText(itemX+58, itemY+21, app.Desc, graphics.Color(0xFF888888))
	}
}

// IsMenuOpen returns whether the menu is open or animating
func IsMenuOpen() bool {
	return shell.MenuOpen || menuAnimating
}

// GetMenuBounds returns the menu bounds for hit testing
func GetMenuBounds() (int, int, int, int) {
	apps := getApps()
	menuH := len(apps)*MenuItemH + MenuPadding
	mx := 8
	my := shell.ScreenH - TaskbarHeight - menuH - 6
	return mx, my, MenuWidth, menuH
}

// HitTestMenuItem returns the app index at the given position, or -1
func HitTestMenuItem(x, y int) int {
	if !IsMenuOpen() {
		return -1
	}

	apps := getApps()
	menuH := len(apps)*MenuItemH + MenuPadding
	mx := 8
	my := shell.ScreenH - TaskbarHeight - menuH - 6

	for i := range apps {
		itemY := my + 8 + i*MenuItemH
		itemX := mx + 8
		itemW := MenuWidth - 16

		if x >= itemX && x < itemX+itemW && y >= itemY && y < itemY+MenuItemH-8 {
			return i
		}
	}
	return -1
}

func getApps() []*AppEntry {
	// Return the same apps as desktop icons
	apps := []*AppEntry{
		{Name: "Terminal", Desc: "Shell session", Icon: iconTerminal[:], Launch: launchTerminal},
		{Name: "Files", Desc: "Browse the disks", Icon: iconFiles[:], Launch: launchFiles},
		{Name: "System", Desc: "Processes + memory", Icon: iconMonitor[:], Launch: launchSysinfo},
		{Name: "Help", Desc: "Keys and mouse", Icon: iconHelp[:], Launch: launchHelp},
		{Name: "About", Desc: "System information", Icon: iconAbout[:], Launch: launchAbout},
	}
	return apps
}

func isMouseOver(x, y, w, h int) bool {
	// Check if mouse is over the given rect
	// Would use global mouse position
	mouseX, mouseY := getMousePos()
	return mouseX >= x && mouseX < x+w && mouseY >= y && mouseY < y+h
}

// Animation easing functions
func easeOutCubic(t float64) float64 {
	return 1 - (1-t)*(1-t)*(1-t)
}

func easeInCubic(t float64) float64 {
	return t * t * t
}