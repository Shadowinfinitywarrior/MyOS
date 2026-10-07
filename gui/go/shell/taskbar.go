// Taskbar - Glass-effect taskbar with start button, window buttons, and system tray

package main

import "myos/shell/graphics"

const (
	TaskbarHeight = 44
	StartBtnWidth = 92
	TaskBtnHeight = 32
	TaskBtnMax    = 8
)

// DrawTaskbar renders the glass-effect taskbar
func DrawTaskbar(surf *graphics.DrawSurface) {
	if shell.ScreenW == 0 || shell.ScreenH == 0 {
		return
	}

	// Taskbar rectangle
	y := shell.ScreenH - TaskbarHeight

	// Glass effect: semi-transparent dark background
	surf.FillRect(0, y, shell.ScreenW, TaskbarHeight, graphics.Color(0x801A1A2E))

	// Top edge highlight
	surf.DrawLine(0, y, shell.ScreenW, y, graphics.Color(0xFF3A3A5E))

	// Start button
	drawStartButton(surf)

	// Task buttons (one per window)
	drawTaskButtons(surf)

	// System tray (clock, uptime)
	drawSystemTray(surf)
}

func drawStartButton(surf *graphics.DrawSurface) {
	x := 8
	y := shell.ScreenH - TaskbarHeight + 6
	w := StartBtnWidth - 8
	h := TaskbarHeight - 12

	bgColor := graphics.Color(0xFF2A2A4E) // Accent color
	if shell.MenuOpen {
		bgColor = graphics.Color(0xFF3A3A6E) // Brighter when open
	}

	// Rounded rectangle for start button
	surf.FillRoundedRect(x, y, w, h, 8, bgColor)

	// Start icon (window icon)
	iconX := x + 9
	iconY := y + (h - 16) / 2
	surf.DrawIcon(iconX, iconY, iconTerminal[:], graphics.Color(0xFFFFFFFF), graphics.ColorTransparent)

	// "Start" text
	textX := x + 32
	textY := y + (h - 15) / 2
	surf.DrawText(textX, textY, "Start", graphics.Color(0xFFFFFFFF))
}

func drawTaskButtons(surf *graphics.DrawSurface) {
	x := StartBtnWidth + 12
	y := shell.ScreenH - TaskbarHeight + (TaskbarHeight - TaskBtnHeight) / 2

	for i, win := range shell.Windows {
		if i >= TaskBtnMax {
			break
		}

		btnW := 130
		btnX := x + i*(TaskBtnHeight+34)
		btnY := y

		bgColor := graphics.Color(0xFF1A1A2E) // Taskbar color
		if shell.FocusedWindow == win {
			bgColor = graphics.Color(0xFF2A2A4E) // Accent soft
		}

		surf.FillRoundedRect(btnX, btnY, btnW, TaskBtnHeight, 7, bgColor)

		// Focus indicator
		if shell.FocusedWindow == win {
			surf.FillRoundedRect(btnX+3, btnY+6, 3, TaskBtnHeight-12, 2, graphics.Color(0xFF3A3A6E))
		}

		// Window title (truncated)
		title := win.Title
		if len(title) > 24 {
			title = title[:24]
		}
		if win.W == 0 { // Minimized
			title += " "
		}

		textColor := graphics.Color(0xFFFFFFFF)
		if shell.FocusedWindow != win {
			textColor = graphics.Color(0xFF888888) // Dim
		}
		surf.DrawText(btnX+12, btnY+(TaskBtnHeight-15)/2, title, textColor)
	}
}

func drawSystemTray(surf *graphics.DrawSurface) {
	// Clock
	clockStr := getClockString()
	clockW := graphics.TextWidth(clockStr)
	clockX := shell.ScreenW - 18 - clockW
	surf.DrawText(clockX, shell.ScreenH-TaskbarHeight+14, clockStr, graphics.Color(0xFFFFFFFF))

	// Uptime
	uptimeStr := getUptimeString()
	uptimeW := graphics.TextWidth(uptimeStr)
	uptimeX := clockX - 20 - uptimeW
	surf.DrawText(uptimeX, shell.ScreenH-TaskbarHeight+15, uptimeStr, graphics.Color(0xFF888888))
}

func getClockString() string {
	// Would read from RTC
	return "12:34:56"
}

func getUptimeString() string {
	// Would read from timer
	return "42m up"
}