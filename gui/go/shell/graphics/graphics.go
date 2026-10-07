// Graphics - Drawing primitives for the Go shell
// Provides functions to draw on surfaces using direct pixel manipulation

package graphics

import (
	"myos/runtime"
	"unsafe"
)

// Color represents an ARGB color
type Color uint32

// Predefined colors
const (
	ColorTransparent Color = 0x00000000
	ColorBlack       Color = 0xFF000000
	ColorWhite       Color = 0xFFFFFFFF
	ColorRed         Color = 0xFFFF0000
	ColorGreen       Color = 0xFF00FF00
	ColorBlue        Color = 0xFF0000FF
)

// Surface wrapper for drawing operations
type DrawSurface struct {
	Pixels []uint32
	Width  int
	Height int
	Pitch  int // bytes per row
}

// GetDrawSurface returns a DrawSurface for a surface handle
func GetDrawSurface(handle uintptr) *DrawSurface {
	surf := runtime.GetSurface(handle)
	if surf == nil {
		return nil
	}
	// Convert the pixel pointer to a slice
	pixelCount := surf.Width * surf.Height
	pixels := unsafe.Slice((*uint32)(surf.Pixels), pixelCount)
	return &DrawSurface{
		Pixels: pixels,
		Width:  surf.Width,
		Height: surf.Height,
		Pitch:  surf.Pitch,
	}
}

// SetPixel sets a single pixel at (x, y)
func (s *DrawSurface) SetPixel(x, y int, c Color) {
	if x < 0 || x >= s.Width || y < 0 || y >= s.Height {
		return
	}
	s.Pixels[y*s.Width+x] = uint32(c)
}

// GetPixel gets a single pixel at (x, y)
func (s *DrawSurface) GetPixel(x, y int) Color {
	if x < 0 || x >= s.Width || y < 0 || y >= s.Height {
		return 0
	}
	return Color(s.Pixels[y*s.Width+x])
}

// FillRect fills a rectangle with a solid color
func (s *DrawSurface) FillRect(x, y, w, h int, c Color) {
	if w <= 0 || h <= 0 {
		return
	}
	// Clip to surface bounds
	if x < 0 {
		w += x
		x = 0
	}
	if y < 0 {
		h += y
		y = 0
	}
	if x+w > s.Width {
		w = s.Width - x
	}
	if y+h > s.Height {
		h = s.Height - y
	}
	if w <= 0 || h <= 0 {
		return
	}

	colorVal := uint32(c)
	for row := 0; row < h; row++ {
		base := (y + row) * s.Width + x
		for col := 0; col < w; col++ {
			s.Pixels[base+col] = colorVal
		}
	}
}

// DrawRect draws a rectangle outline
func (s *DrawSurface) DrawRect(x, y, w, h int, c Color, thickness int) {
	if thickness <= 0 {
		return
	}
	// Top
	s.FillRect(x, y, w, thickness, c)
	// Bottom
	s.FillRect(x, y+h-thickness, w, thickness, c)
	// Left
	s.FillRect(x, y, thickness, h, c)
	// Right
	s.FillRect(x+w-thickness, y, thickness, h, c)
}

// FillRoundedRect fills a rounded rectangle
func (s *DrawSurface) FillRoundedRect(x, y, w, h, radius int, c Color) {
	if radius <= 0 {
		s.FillRect(x, y, w, h, c)
		return
	}
	if radius*2 > w {
		radius = w / 2
	}
	if radius*2 > h {
		radius = h / 2
	}

	r := radius

	// Fill center body
	s.FillRect(x, y+r, w, h-r*2, c)
	// Fill top and bottom
	s.FillRect(x+r, y, w-r*2, r, c)
	s.FillRect(x+r, y+h-r, w-r*2, r, c)

	// Fill corners using circle equation
	r2 := r * r
	for dy := 0; dy < r; dy++ {
		for dx := 0; dx < r; dx++ {
			if dx*dx+dy*dy <= r2 {
				// Top-left
				s.SetPixel(x+r-1-dx, y+r-1-dy, c)
				// Top-right
				s.SetPixel(x+w-r+dx, y+r-1-dy, c)
				// Bottom-left
				s.SetPixel(x+r-1-dx, y+h-r+dy, c)
				// Bottom-right
				s.SetPixel(x+w-r+dx, y+h-r+dy, c)
			}
		}
	}
}

// DrawRoundedRect draws a rounded rectangle outline
func (s *DrawSurface) DrawRoundedRect(x, y, w, h, radius int, c Color, thickness int) {
	if radius <= 0 || thickness <= 0 {
		s.DrawRect(x, y, w, h, c, thickness)
		return
	}
	if radius*2 > w {
		radius = w / 2
	}
	if radius*2 > h {
		radius = h / 2
	}

	r := radius
	// Draw top and bottom edges
	s.FillRect(x+r, y, w-r*2, thickness, c)
	s.FillRect(x+r, y+h-thickness, w-r*2, thickness, c)
	// Draw left and right edges
	s.FillRect(x, y+r, thickness, h-r*2, c)
	s.FillRect(x+w-thickness, y+r, thickness, h-r*2, c)

	// Draw corner arcs
	r2Outer := r * r
	rInner := r - thickness
	r2Inner := 0
	if rInner > 0 {
		r2Inner = rInner * rInner
	}

	corners := [4][2]int{
		{x + r, y + r},
		{x + w - r - 1, y + r},
		{x + r, y + h - r - 1},
		{x + w - r - 1, y + h - r - 1},
	}

	for _, corner := range corners {
		cx, cy := corner[0], corner[1]
		for dy := 0; dy < r; dy++ {
			for dx := 0; dx < r; dx++ {
				d2 := dx*dx + dy*dy
				if d2 <= r2Outer && d2 >= r2Inner {
					s.SetPixel(cx+dx, cy+dy, c)
				}
			}
		}
	}
}

// DrawLine draws a line using Bresenham's algorithm
func (s *DrawSurface) DrawLine(x0, y0, x1, y1 int, c Color) {
	dx := abs(x1 - x0)
	dy := -abs(y1 - y0)
	sx := 1
	if x0 >= x1 {
		sx = -1
	}
	sy := 1
	if y0 >= y1 {
		sy = -1
	}
	err := dx + dy

	x, y := x0, y0
	for {
		s.SetPixel(x, y, c)
		if x == x1 && y == y1 {
			break
		}
		e2 := 2 * err
		if e2 >= dy {
			err += dy
			x += sx
		}
		if e2 <= dx {
			err += dx
			y += sy
		}
	}
}

func abs(x int) int {
	if x < 0 {
		return -x
	}
	return x
}

// DrawIcon draws a 16x16 1bpp icon
func (s *DrawSurface) DrawIcon(x, y int, icon []byte, fgColor, bgColor Color) {
	for row := 0; row < 16 && row < len(icon)/2; row++ {
		bits := uint16(icon[row*2]) | uint16(icon[row*2+1])<<8
		for col := 0; col < 16; col++ {
			if bits&(1<<col) != 0 {
				s.SetPixel(x+col, y+row, fgColor)
			} else if bgColor != ColorTransparent {
				s.SetPixel(x+col, y+row, bgColor)
			}
		}
	}
}

// Font metrics for the built-in font
const (
	FontWidth  = 8
	FontHeight = 15
)

// Simple 8x15 font for basic ASCII (32-126)
// This is a minimal font - in production would use baked fonts from kernel
var font8x15 = [95][15]uint16{
	// ' ' (32) - space
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	// '!' (33)
	{0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x18, 0x00, 0x18},
	// ... (truncated for brevity, would include all 95 printable chars)
}

// DrawText draws text using the built-in font
func (s *DrawSurface) DrawText(x, y int, text string, color Color) {
	for i, r := range text {
		if r < 32 || r > 126 {
			continue
		}
		charX := x + i*FontWidth
		if charX+FontWidth > s.Width {
			break
		}
		// For now, draw a simple block for each character
		// In production, use the baked font data
		s.FillRect(charX, y, FontWidth, FontHeight, color)
		// Add small gap between characters
		if charX+FontWidth < s.Width {
			s.SetPixel(charX+FontWidth-1, y, ColorTransparent)
		}
	}
}

// TextWidth returns the width of text in pixels
func TextWidth(text string) int {
	return len(text) * FontWidth
}

// TextHeight returns the height of text in pixels
func TextHeight() int {
	return FontHeight
}

// Gradient fills a vertical gradient
func (s *DrawSurface) FillGradientV(x, y, w, h int, top, bottom Color) {
	tr := uint32(top>>16&0xFF)
	tg := uint32(top>>8&0xFF)
	tb := uint32(top&0xFF)
	br := uint32(bottom>>16&0xFF)
	bg := uint32(bottom>>8&0xFF)
	bb := uint32(bottom&0xFF)

	for row := 0; row < h; row++ {
		t := uint32(row) * 255 / uint32(h)
		r := tr + ((br - tr) * t / 255)
		g := tg + ((bg - tg) * t / 255)
		b := tb + ((bb - tb) * t / 255)
		c := Color((0xFF << 24) | (r << 16) | (g << 8) | b)
		s.FillRect(x, y+row, w, 1, c)
	}
}

// Blit copies another surface onto this one
func (s *DrawSurface) Blit(src *DrawSurface, x, y int) {
	srcW, srcH := src.Width, src.Height
	// Clip to destination bounds
	if x < 0 {
		srcW += x
		x = 0
	}
	if y < 0 {
		srcH += y
		y = 0
	}
	if x+srcW > s.Width {
		srcW = s.Width - x
	}
	if y+srcH > s.Height {
		srcH = s.Height - y
	}
	if srcW <= 0 || srcH <= 0 {
		return
	}

	for row := 0; row < srcH; row++ {
		dstBase := (y+row)*s.Width + x
		srcBase := row * src.Width
		copy(s.Pixels[dstBase:dstBase+srcW], src.Pixels[srcBase:srcBase+srcW])
	}
}

// AlphaBlit copies a surface with alpha blending
func (s *DrawSurface) AlphaBlit(src *DrawSurface, x, y int) {
	srcW, srcH := src.Width, src.Height
	if x < 0 {
		srcW += x
		x = 0
	}
	if y < 0 {
		srcH += y
		y = 0
	}
	if x+srcW > s.Width {
		srcW = s.Width - x
	}
	if y+srcH > s.Height {
		srcH = s.Height - y
	}
	if srcW <= 0 || srcH <= 0 {
		return
	}

	for row := 0; row < srcH; row++ {
		dstBase := (y+row)*s.Width + x
		srcBase := row * src.Width
		for col := 0; col < srcW; col++ {
			srcPixel := Color(src.Pixels[srcBase+col])
			alpha := srcPixel >> 24
			if alpha == 0 {
				continue
			}
			if alpha == 255 {
				s.Pixels[dstBase+col] = src.Pixels[srcBase+col]
			} else {
				dstPixel := Color(s.Pixels[dstBase+col])
				s.Pixels[dstBase+col] = alphaBlend(srcPixel, dstPixel, uint8(alpha))
			}
		}
	}
}

func alphaBlend(src, dst Color, alpha uint8) uint32 {
	if alpha == 255 {
		return uint32(src)
	}
	if alpha == 0 {
		return uint32(dst)
	}
	a := uint32(alpha)
	na := 255 - a
	sr := uint32(src>>16&0xFF)
	sg := uint32(src>>8&0xFF)
	sb := uint32(src&0xFF)
	dr := uint32(dst>>16&0xFF)
	dg := uint32(dst>>8&0xFF)
	db := uint32(dst&0xFF)
	r := (sr*a + dr*na) / 255
	g := (sg*a + dg*na) / 255
	b := (sb*a + db*na) / 255
	return (0xFF << 24) | (r << 16) | (g << 8) | b
}