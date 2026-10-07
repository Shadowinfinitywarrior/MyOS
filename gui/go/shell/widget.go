// Widget - Notifications, widgets, and system indicators

package main

import (
	"time"

	"myos/shell/graphics"
)

const (
	NotificationWidth  = 340
	NotificationHeight = 80
	NotificationMargin = 16
	MaxNotifications   = 5
	NotifDuration      = 5 * time.Second
	NotifAnimDuration  = 300 * time.Millisecond
)

type Notification struct {
	Title       string
	Message     string
	Icon        []byte
	CreatedAt   time.Time
	Progress    float64 // Animation progress 0.0 to 1.0
	Dismissing  bool
}

var notifications []*Notification

// ShowNotification displays a new notification
func ShowNotification(title, message string, icon []byte) {
	notif := &Notification{
		Title:     title,
		Message:   message,
		Icon:      icon,
		CreatedAt: time.Now(),
		Progress:  0.0,
	}

	notifications = append([]*Notification{notif}, notifications...)
	if len(notifications) > MaxNotifications {
		notifications = notifications[:MaxNotifications]
	}
}

// UpdateNotifications updates notification animations and removes expired ones
func UpdateNotifications() {
	now := time.Now()

	for i := len(notifications) - 1; i >= 0; i-- {
		n := notifications[i]

		if n.Dismissing {
			// Fade out
			n.Progress -= 0.05
			if n.Progress <= 0 {
				removeNotification(i)
			}
		} else if now.Sub(n.CreatedAt) > NotifDuration {
			// Start dismissing
			n.Dismissing = true
		} else if n.Progress < 1.0 {
			// Fade in
			n.Progress += 0.05
			if n.Progress > 1.0 {
				n.Progress = 1.0
			}
		}
	}
}

func removeNotification(index int) {
	notifications = append(notifications[:index], notifications[index+1:]...)
}

// DrawNotifications renders all active notifications
func DrawNotifications(surf *graphics.DrawSurface) {
	UpdateNotifications()

	screenW := shell.ScreenW
	screenH := shell.ScreenH

	for i, n := range notifications {
		// Position from top-right, below taskbar
		x := screenW - NotificationWidth - NotificationMargin
		y := screenH - TaskbarHeight - NotificationMargin - (i+1)*(NotificationHeight+8)

		// Animation transform (slide in from right)
		animX := x + int(float64(NotificationWidth)*(1.0-n.Progress))
		alpha := uint8(255 * n.Progress)

		drawNotification(surf, animX, y, n, alpha)
	}
}

func drawNotification(surf *graphics.DrawSurface, x, y int, n *Notification, alpha uint8) {
	// Background card with rounded corners
	bgColor := graphics.Color(0xFA1A1A2E | uint32(alpha<<24))
	surf.FillRoundedRect(x, y, NotificationWidth, NotificationHeight, 10, bgColor)

	borderColor := graphics.Color(0xFF3A3A6E | uint32(alpha<<24))
	surf.DrawRoundedRect(x, y, NotificationWidth, NotificationHeight, 10, borderColor, 1)

	// Icon
	if n.Icon != nil {
		icX := x + 16
		icY := y + 16
		surf.FillRoundedRect(icX, icY, 48, 48, 8, graphics.Color(0xFF3A3A6E))
		surf.DrawIcon(icX+16, icY+16, n.Icon, graphics.Color(0xFFFFFFFF|uint32(alpha<<24)), graphics.ColorTransparent)
	}

	// Title
	surf.DrawText(x+80, y+16, n.Title, graphics.Color(0xFFFFFFFF|uint32(alpha<<24)))

	// Message
	surf.DrawText(x+80, y+40, n.Message, graphics.Color(0xFFCCCCCC|uint32(alpha<<24)))

	// Progress bar (time remaining)
	if !n.Dismissing {
		elapsed := time.Since(n.CreatedAt)
		progress := 1.0 - float64(elapsed)/float64(NotifDuration)
		if progress < 0 {
			progress = 0
		}
		barW := int(float64(NotificationWidth-32) * progress)
		surf.FillRoundedRect(x+16, y+NotificationHeight-6, barW, 4, 2, graphics.Color(0xFF3A3A6E|uint32(alpha<<24)))
	}
}

// SystemWidget represents a persistent widget on the desktop
type SystemWidget struct {
	Title      string
	X, Y       int
	W, H       int
	Visible    bool
	UpdateFunc func() string
	LastUpdate time.Time
}

var widgets []*SystemWidget

// RegisterWidget adds a system widget (e.g., clock, CPU meter, weather)
func RegisterWidget(title string, x, y, width, height int, updateFunc func() string) *SystemWidget {
	widget := &SystemWidget{
		Title:      title,
		X:          x,
		Y:          y,
		W:          width,
		H:          height,
		Visible:    true,
		UpdateFunc: updateFunc,
	}
	widgets = append(widgets, widget)
	return widget
}

// DrawWidgets renders all registered widgets
func DrawWidgets(surf *graphics.DrawSurface) {
	for _, w := range widgets {
		if !w.Visible {
			continue
		}

		// Widget background
		surf.FillRoundedRect(w.X, w.Y, w.W, w.H, 10, graphics.Color(0xE01A1A2E))
		surf.DrawRoundedRect(w.X, w.Y, w.W, w.H, 10, graphics.Color(0xFF3A3A6E), 1)

		// Title bar
		surf.FillRoundedRect(w.X, w.Y, w.W, 30, 10, graphics.Color(0xFF2A2A4E))
		surf.DrawText(w.X+12, w.Y+8, w.Title, graphics.Color(0xFFFFFFFF))

		// Content
		content := ""
		if w.UpdateFunc != nil {
			content = w.UpdateFunc()
		}
		surf.DrawText(w.X+16, w.Y+40, content, graphics.Color(0xFFCCCCCC))
	}
}

// Example widget update functions
func ClockWidgetUpdate() string {
	// Return current time
	return "12:34:56"
}

func CPUWidgetUpdate() string {
	// Return CPU usage
	return "CPU: 12%"
}

func MemWidgetUpdate() string {
	// Return memory usage
	return "RAM: 256/512 MB"
}