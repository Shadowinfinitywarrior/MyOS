// Desktop - Icon grid with drag-and-drop support

package main

import "myos/shell/graphics"

const (
	IconSize      = 84
	IconSpacing   = 96
	IconLabelH    = 24
	IconGridCols  = 8
	IconMarginX   = 26
	IconMarginY   = 22
)

var (
	draggedIcon   *DesktopIcon
	dragOffsetX   int
	dragOffsetY   int
	dragging      bool
)

// DrawDesktopIcons renders all desktop icons
func DrawDesktopIcons(surf *graphics.DrawSurface) {
	for _, icon := range shell.DesktopIcons {
		if icon == draggedIcon && dragging {
			continue // Draw dragged icon separately
		}
		drawIconAt(surf, icon, icon.X, icon.Y)
	}

	// Draw dragged icon on top
	if dragging && draggedIcon != nil {
		mouseX, mouseY := getMousePos()
		drawIconAt(surf, draggedIcon, mouseX-dragOffsetX, mouseY-dragOffsetY)
	}
}

func drawIconAt(surf *graphics.DrawSurface, icon *DesktopIcon, x, y int) {
	app := icon.App

	// Icon plate (background on hover)
	if icon.Hovered {
		surf.FillRoundedRect(x, y, IconSize, IconSize+IconLabelH, 8, graphics.Color(0x8A2A2A4E))
	}

	// App icon (16x16 centered in 64x64 area)
	icX := x + (IconSize - 32) / 2
	icY := y + 8
	surf.FillRoundedRect(icX, icY, 32, 32, 7, graphics.Color(0xFF3A3A6E))
	surf.DrawIcon(icX+8, icY+8, app.Icon[:], graphics.Color(0xFFFFFFFF), graphics.ColorTransparent)

	// App name
	name := app.Name
	nameW := graphics.TextWidth(name)
	surf.DrawText(x+(IconSize-nameW)/2, y+IconSize+4, name, graphics.Color(0xFFFFFFFF))

	// App description (truncated)
	desc := app.Desc
	if graphics.TextWidth(desc) > IconSize {
		desc = truncate(desc, IconSize)
	}
	descW := graphics.TextWidth(desc)
	surf.DrawText(x+(IconSize-descW)/2, y+IconSize+20, desc, graphics.Color(0xFF888888))
}

// HandleMouseDown handles mouse down on desktop icons
func HandleMouseDown(x, y int) bool {
	for i := len(shell.DesktopIcons) - 1; i >= 0; i-- {
		icon := shell.DesktopIcons[i]
		if hitTestIcon(icon, x, y) {
			// Start drag
			draggedIcon = icon
			dragOffsetX = x - icon.X
			dragOffsetY = y - icon.Y
			dragging = true
			return true
		}
	}
	return false
}

// HandleMouseMove handles mouse move for icon dragging
func HandleMouseMove(x, y int) {
	// Update hover states
	for _, icon := range shell.DesktopIcons {
		icon.Hovered = hitTestIcon(icon, x, y)
	}

	// Update drag position
	if dragging && draggedIcon != nil {
		// Icon follows mouse during drag
	}
}

// HandleMouseUp handles mouse up - launch app or drop icon
func HandleMouseUp(x, y int) bool {
	if dragging {
		dragging = false
		draggedIcon = nil
		return true
	}

	// Check for double-click to launch
	for _, icon := range shell.DesktopIcons {
		if hitTestIcon(icon, x, y) {
			if icon.App.Launch != nil {
				icon.App.Launch()
			}
			return true
		}
	}
	return false
}

func hitTestIcon(icon *DesktopIcon, x, y int) bool {
	return x >= icon.X && x < icon.X+IconSize && y >= icon.Y && y < icon.Y+IconSize+IconLabelH
}

func getMousePos() (int, int) {
	// Get current mouse position from kernel
	// This would be implemented via syscall
	return 0, 0
}

func truncate(s string, maxW int) string {
	// Truncate string to fit width
	for len(s) > 0 && graphics.TextWidth(s) > maxW {
		s = s[:len(s)-1]
	}
	return s
}

// ArrangeIcons arranges icons in a grid
func ArrangeIcons() {
	for i, icon := range shell.DesktopIcons {
		col := i % IconGridCols
		row := i / IconGridCols
		icon.X = IconMarginX + col*IconSpacing
		icon.Y = IconMarginY + row*IconSpacing
	}
}