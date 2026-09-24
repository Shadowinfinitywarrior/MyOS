#include "libc.h"
#include "../gui/wm.h"
#include "../gui/scene.h"
#include "../drivers/framebuffer.h"
#include "../drivers/keyboard.h"
#include "../drivers/mouse.h"
#include "../lib/string.h"
#include "../lib/printf.h"
#include "../kernel/timer.h"
#include "../kernel/heap.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

#define MAX_CATEGORIES 12
#define MAX_SETTINGS 64

typedef enum {
    SET_TYPE_TOGGLE = 0,
    SET_TYPE_SLIDER,
    SET_TYPE_DROPDOWN,
    SET_TYPE_BUTTON,
    SET_TYPE_TEXT,
    SET_TYPE_SECTION
} setting_type_t;

typedef struct {
    char label[64];
    setting_type_t type;
    union {
        struct { bool value; } toggle;
        struct { int value; int min; int max; } slider;
        struct { char options[8][32]; int count; int selected; } dropdown;
        struct { void (*callback)(void); } button;
        struct { char value[128]; int cursor; } text;
    } data;
    char description[256];
} setting_item_t;

typedef struct {
    char name[32];
    char icon[8];
    uint32_t color;
    setting_item_t settings[MAX_SETTINGS];
    int setting_count;
} settings_category_t;

typedef struct {
    settings_category_t categories[MAX_CATEGORIES];
    int category_count;
    int selected_category;
    int selected_setting;
    int scroll_offset;
    char search_query[64];
    bool search_active;
    int search_cursor;
} settings_state_t;

static settings_state_t g_settings = {0};
static window_t *g_win = NULL;

static const uint32_t COL_BG = 0x1A1A2E;
static const uint32_t COL_SIDEBAR = 0x161625;
static const uint32_t COL_CARD = 0x1F1F35;
static const uint32_t COL_TEXT = 0xFFFFFF;
static const uint32_t COL_TEXT_MUTED = 0x8888AA;
static const uint32_t COL_ACCENT = 0x00A4EF;
static const uint32_t COL_ACCENT_HOVER = 0x0078D7;
static const uint32_t COL_BORDER = 0x333355;
static const uint32_t COL_HOVER = 0x2A2A4A;
static const uint32_t COL_TOGGLE_ON = 0x00A4EF;
static const uint32_t COL_TOGGLE_OFF = 0x444466;

static void draw_rect_fb(int x, int y, int w, int h, uint32_t color) {
    for (int i = 0; i < w; i++) {
        fb_draw_pixel(x + i, y, color);
        fb_draw_pixel(x + i, y + h - 1, color);
    }
    for (int i = 0; i < h; i++) {
        fb_draw_pixel(x, y + i, color);
        fb_draw_pixel(x + w - 1, y + i, color);
    }
}

static void fill_rect_fb(int x, int y, int w, int h, uint32_t color) {
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            fb_draw_pixel(x + i, y + j, color);
        }
    }
}

static void fill_round_rect_fb(int x, int y, int w, int h, int r, uint32_t color) {
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;
    fill_rect_fb(x + r, y, w - 2 * r, h, color);
    fill_rect_fb(x, y + r, r, h - 2 * r, color);
    fill_rect_fb(x + w - r, y + r, r, h - 2 * r, color);
    for (int dy = 0; dy < r; dy++) {
        for (int dx = 0; dx < r; dx++) {
            if ((r - dx) * (r - dx) + (r - dy) * (r - dy) <= r * r) {
                fb_draw_pixel(x + dx, y + dy, color);
                fb_draw_pixel(x + w - 1 - dx, y + dy, color);
                fb_draw_pixel(x + dx, y + h - 1 - dy, color);
                fb_draw_pixel(x + w - 1 - dx, y + h - 1 - dy, color);
            }
        }
    }
}

static void draw_round_rect_fb(int x, int y, int w, int h, int r, uint32_t color) {
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;
    for (int i = r; i < w - r; i++) {
        fb_draw_pixel(x + i, y, color);
        fb_draw_pixel(x + i, y + h - 1, color);
    }
    for (int i = r; i < h - r; i++) {
        fb_draw_pixel(x, y + i, color);
        fb_draw_pixel(x + w - 1, y + i, color);
    }
    for (int dy = 0; dy < r; dy++) {
        for (int dx = 0; dx < r; dx++) {
            if ((r - dx) * (r - dx) + (r - dy) * (r - dy) <= r * r) {
                fb_draw_pixel(x + dx, y + dy, color);
                fb_draw_pixel(x + w - 1 - dx, y + dy, color);
                fb_draw_pixel(x + dx, y + h - 1 - dy, color);
                fb_draw_pixel(x + w - 1 - dx, y + h - 1 - dy, color);
            }
        }
    }
}

static void draw_char_fb(int x, int y, char c, uint32_t color, int scale) {
    extern const uint8_t font_8x8[96][8];
    unsigned char uc = (unsigned char)c;
    if (uc < 32 || uc > 127) uc = '?';
    const uint8_t *glyph = font_8x8[uc - 32];
    for (int row = 0; row < 8; row++) {
        uint8_t bits = glyph[row];
        for (int col = 0; col < 8; col++) {
            if (bits & (0x80 >> col)) {
                if (scale == 1) fb_draw_pixel(x + col, y + row, color);
                else fill_rect_fb(x + col * scale, y + row * scale, scale, scale, color);
            }
        }
    }
}

static void draw_string_fb(int x, int y, const char *str, uint32_t color, int scale) {
    int cx = x;
    while (*str) {
        draw_char_fb(cx, y, *str, color, scale);
        cx += 9 * scale;
        str++;
    }
}

static int str_width_fb(const char *str, int scale) {
    int w = 0;
    while (*str) { w += 9 * scale; str++; }
    return w;
}

static void add_toggle(setting_item_t *s, const char *label, bool value, const char *desc) {
    strncpy(s->label, label, 63);
    s->type = SET_TYPE_TOGGLE;
    s->data.toggle.value = value;
    strncpy(s->description, desc, 255);
}

static void add_slider(setting_item_t *s, const char *label, int value, int min, int max, const char *desc) {
    strncpy(s->label, label, 63);
    s->type = SET_TYPE_SLIDER;
    s->data.slider.value = value;
    s->data.slider.min = min;
    s->data.slider.max = max;
    strncpy(s->description, desc, 255);
}

static void add_dropdown(setting_item_t *s, const char *label, const char options[][32], int count, int selected, const char *desc) {
    strncpy(s->label, label, 63);
    s->type = SET_TYPE_DROPDOWN;
    s->data.dropdown.count = count;
    s->data.dropdown.selected = selected;
    for (int i = 0; i < count; i++) {
        strncpy(s->data.dropdown.options[i], options[i], 31);
    }
    strncpy(s->description, desc, 255);
}

static void add_button(setting_item_t *s, const char *label, void (*callback)(void), const char *desc) {
    strncpy(s->label, label, 63);
    s->type = SET_TYPE_BUTTON;
    s->data.button.callback = callback;
    strncpy(s->description, desc, 255);
}

static void add_section(setting_item_t *s, const char *label) {
    strncpy(s->label, label, 63);
    s->type = SET_TYPE_SECTION;
    s->description[0] = '\0';
}

static void init_settings() {
    g_settings.category_count = 0;

    settings_category_t *cat;

    cat = &g_settings.categories[g_settings.category_count++];
    strcpy(cat->name, "System");
    strcpy(cat->icon, "S");
    cat->color = 0x00A4EF;
    cat->setting_count = 0;
    add_section(&cat->settings[cat->setting_count++], "Display");
    add_slider(&cat->settings[cat->setting_count++], "Brightness", 75, 0, 100, "Adjust screen brightness");
    add_slider(&cat->settings[cat->setting_count++], "Night Light Strength", 50, 0, 100, "Reduce blue light at night");
    add_dropdown(&cat->settings[cat->setting_count++], "Scale", (char[4][32]){"100%", "125%", "150%", "175%"}, 4, 0, "Change the size of text, apps, and other items");
    add_dropdown(&cat->settings[cat->setting_count++], "Resolution", (char[4][32]){"1024x768", "1280x720", "1920x1080", "3840x2160"}, 4, 0, "Display resolution");
    add_toggle(&cat->settings[cat->setting_count++], "HDR", false, "Use HDR for supported content");
    add_section(&cat->settings[cat->setting_count++], "Sound");
    add_slider(&cat->settings[cat->setting_count++], "Master Volume", 80, 0, 100, "System master volume");
    add_toggle(&cat->settings[cat->setting_count++], "Spatial Sound", false, "Immersive audio experience");
    add_section(&cat->settings[cat->setting_count++], "Notifications");
    add_toggle(&cat->settings[cat->setting_count++], "Notifications", true, "Show notifications from apps and system");
    add_toggle(&cat->settings[cat->setting_count++], "Focus Assist", false, "Suppress notifications when busy");
    add_slider(&cat->settings[cat->setting_count++], "Notification Duration", 5, 3, 30, "Seconds to show notifications");

    cat = &g_settings.categories[g_settings.category_count++];
    strcpy(cat->name, "Bluetooth & devices");
    strcpy(cat->icon, "B");
    cat->color = 0x663399;
    cat->setting_count = 0;
    add_toggle(&cat->settings[cat->setting_count++], "Bluetooth", true, "Turn Bluetooth on or off");
    add_button(&cat->settings[cat->setting_count++], "Add Device", NULL, "Pair a new Bluetooth device");
    add_section(&cat->settings[cat->setting_count++], "Devices");
    add_button(&cat->settings[cat->setting_count++], "Mouse", NULL, "Configure mouse settings");
    add_button(&cat->settings[cat->setting_count++], "Keyboard", NULL, "Configure keyboard settings");
    add_button(&cat->settings[cat->setting_count++], "Touchpad", NULL, "Configure touchpad gestures");
    add_button(&cat->settings[cat->setting_count++], "Pen", NULL, "Configure pen and ink settings");
    add_button(&cat->settings[cat->setting_count++], "Printers", NULL, "Manage printers and scanners");
    add_button(&cat->settings[cat->setting_count++], "Cameras", NULL, "Manage camera devices");

    cat = &g_settings.categories[g_settings.category_count++];
    strcpy(cat->name, "Network & internet");
    strcpy(cat->icon, "N");
    cat->color = 0x336699;
    cat->setting_count = 0;
    add_toggle(&cat->settings[cat->setting_count++], "Wi-Fi", true, "Connect to wireless networks");
    add_button(&cat->settings[cat->setting_count++], "Show Available Networks", NULL, "View and connect to Wi-Fi networks");
    add_section(&cat->settings[cat->setting_count++], "Ethernet");
    add_toggle(&cat->settings[cat->setting_count++], "Ethernet", true, "Wired network connection");
    add_section(&cat->settings[cat->setting_count++], "VPN");
    add_button(&cat->settings[cat->setting_count++], "Add VPN", NULL, "Add a VPN connection");
    add_section(&cat->settings[cat->setting_count++], "Mobile Hotspot");
    add_toggle(&cat->settings[cat->setting_count++], "Mobile Hotspot", false, "Share internet connection");
    add_button(&cat->settings[cat->setting_count++], "Properties", NULL, "Configure hotspot name and password");
    add_section(&cat->settings[cat->setting_count++], "Data Usage");
    add_button(&cat->settings[cat->setting_count++], "View Data Usage", NULL, "See network data usage per app");

    cat = &g_settings.categories[g_settings.category_count++];
    strcpy(cat->name, "Personalization");
    strcpy(cat->icon, "P");
    cat->color = 0x339933;
    cat->setting_count = 0;
    add_section(&cat->settings[cat->setting_count++], "Background");
    add_dropdown(&cat->settings[cat->setting_count++], "Background", (char[3][32]){"Picture", "Solid Color", "Slideshow"}, 3, 0, "Choose your background");
    add_button(&cat->settings[cat->setting_count++], "Browse", NULL, "Select a picture for background");
    add_dropdown(&cat->settings[cat->setting_count++], "Fit", (char[6][32]){"Fill", "Fit", "Stretch", "Tile", "Center", "Span"}, 6, 0, "Choose how the picture fits");
    add_section(&cat->settings[cat->setting_count++], "Colors");
    add_dropdown(&cat->settings[cat->setting_count++], "Mode", (char[3][32]){"Light", "Dark", "Custom"}, 3, 1, "Choose your app mode");
    add_dropdown(&cat->settings[cat->setting_count++], "Accent Color", (char[8][32]){"Blue", "Green", "Red", "Purple", "Orange", "Yellow", "Pink", "Teal"}, 8, 0, "Choose an accent color");
    add_toggle(&cat->settings[cat->setting_count++], "Transparency Effects", true, "Enable acrylic blur effects");
    add_toggle(&cat->settings[cat->setting_count++], "Show Accent on Title Bars", true, "Show accent color on window title bars");
    add_toggle(&cat->settings[cat->setting_count++], "Show Accent on Start/Taskbar", true, "Show accent color on Start and taskbar");
    add_section(&cat->settings[cat->setting_count++], "Themes");
    add_button(&cat->settings[cat->setting_count++], "Get More Themes", NULL, "Download themes from store");

    cat = &g_settings.categories[g_settings.category_count++];
    strcpy(cat->name, "Apps");
    strcpy(cat->icon, "A");
    cat->color = 0x996633;
    cat->setting_count = 0;
    add_section(&cat->settings[cat->setting_count++], "Installed Apps");
    add_button(&cat->settings[cat->setting_count++], "Sort by Name", NULL, "Sort apps alphabetically");
    add_button(&cat->settings[cat->setting_count++], "Filter by Drive", NULL, "Show apps on specific drive");
    add_section(&cat->settings[cat->setting_count++], "Default Apps");
    add_button(&cat->settings[cat->setting_count++], "Email", NULL, "Choose default email app");
    add_button(&cat->settings[cat->setting_count++], "Maps", NULL, "Choose default maps app");
    add_button(&cat->settings[cat->setting_count++], "Music Player", NULL, "Choose default music player");
    add_button(&cat->settings[cat->setting_count++], "Photo Viewer", NULL, "Choose default photo viewer");
    add_button(&cat->settings[cat->setting_count++], "Video Player", NULL, "Choose default video player");
    add_button(&cat->settings[cat->setting_count++], "Web Browser", NULL, "Choose default web browser");
    add_section(&cat->settings[cat->setting_count++], "Startup");
    add_toggle(&cat->settings[cat->setting_count++], "Startup Apps", true, "Allow apps to run at startup");
    add_button(&cat->settings[cat->setting_count++], "Manage Startup Apps", NULL, "Configure which apps start automatically");

    cat = &g_settings.categories[g_settings.category_count++];
    strcpy(cat->name, "Accounts");
    strcpy(cat->icon, "U");
    cat->color = 0x663399;
    cat->setting_count = 0;
    add_section(&cat->settings[cat->setting_count++], "Your Info");
    add_button(&cat->settings[cat->setting_count++], "Manage My Account", NULL, "Open account settings online");
    add_button(&cat->settings[cat->setting_count++], "Sign in with Local Account", NULL, "Switch to local account");
    add_section(&cat->settings[cat->setting_count++], "Sign-in Options");
    add_button(&cat->settings[cat->setting_count++], "Password", NULL, "Change your password");
    add_button(&cat->settings[cat->setting_count++], "PIN", NULL, "Set up a PIN for quick sign-in");
    add_button(&cat->settings[cat->setting_count++], "Fingerprint", NULL, "Set up fingerprint recognition");
    add_button(&cat->settings[cat->setting_count++], "Facial Recognition", NULL, "Set up Windows Hello Face");
    add_section(&cat->settings[cat->setting_count++], "Family & Other Users");
    add_button(&cat->settings[cat->setting_count++], "Add Account", NULL, "Add a family member or other user");
    add_section(&cat->settings[cat->setting_count++], "Sync Your Settings");
    add_toggle(&cat->settings[cat->setting_count++], "Sync Settings", true, "Sync theme, passwords, and other settings");
    add_toggle(&cat->settings[cat->setting_count++], "Theme", true, "Sync theme across devices");
    add_toggle(&cat->settings[cat->setting_count++], "Passwords", true, "Sync saved passwords");
    add_toggle(&cat->settings[cat->setting_count++], "Language Preferences", true, "Sync language settings");
    add_toggle(&cat->settings[cat->setting_count++], "Other Windows Settings", true, "Sync other Windows settings");

    cat = &g_settings.categories[g_settings.category_count++];
    strcpy(cat->name, "Time & Language");
    strcpy(cat->icon, "T");
    cat->color = 0x993366;
    cat->setting_count = 0;
    add_section(&cat->settings[cat->setting_count++], "Date & Time");
    add_toggle(&cat->settings[cat->setting_count++], "Set Time Automatically", true, "Synchronize with time server");
    add_toggle(&cat->settings[cat->setting_count++], "Set Time Zone Automatically", true, "Use location for time zone");
    add_dropdown(&cat->settings[cat->setting_count++], "Time Zone", (char[5][32]){"UTC", "EST", "PST", "CET", "JST"}, 5, 0, "Select your time zone");
    add_button(&cat->settings[cat->setting_count++], "Sync Now", NULL, "Synchronize clock with time server");
    add_section(&cat->settings[cat->setting_count++], "Language & Region");
    add_dropdown(&cat->settings[cat->setting_count++], "Windows Display Language", (char[4][32]){"English (US)", "English (UK)", "Spanish", "French"}, 4, 0, "Display language for Windows");
    add_button(&cat->settings[cat->setting_count++], "Add Language", NULL, "Install a new display language");
    add_dropdown(&cat->settings[cat->setting_count++], "Country/Region", (char[4][32]){"United States", "United Kingdom", "Canada", "Australia"}, 4, 0, "Your country or region");
    add_section(&cat->settings[cat->setting_count++], "Typing");
    add_toggle(&cat->settings[cat->setting_count++], "Autocorrect", true, "Autocorrect misspelled words");
    add_toggle(&cat->settings[cat->setting_count++], "Highlight Misspelled", true, "Show red squiggly under misspelled words");
    add_toggle(&cat->settings[cat->setting_count++], "Text Suggestions", true, "Show text suggestions as you type");

    cat = &g_settings.categories[g_settings.category_count++];
    strcpy(cat->name, "Gaming");
    strcpy(cat->icon, "G");
    cat->color = 0xCC3366;
    cat->setting_count = 0;
    add_section(&cat->settings[cat->setting_count++], "Game Mode");
    add_toggle(&cat->settings[cat->setting_count++], "Game Mode", true, "Optimize PC for gaming");
    add_section(&cat->settings[cat->setting_count++], "Captures");
    add_toggle(&cat->settings[cat->setting_count++], "Record in Background", false, "Record gameplay in background");
    add_slider(&cat->settings[cat->setting_count++], "Max Recording Length", 120, 30, 300, "Maximum recording time (seconds)");
    add_dropdown(&cat->settings[cat->setting_count++], "Video Quality", (char[3][32]){"Standard", "High", "Best"}, 3, 1, "Recording video quality");
    add_toggle(&cat->settings[cat->setting_count++], "Capture Audio", true, "Record audio with game clips");
    add_section(&cat->settings[cat->setting_count++], "Xbox Game Bar");
    add_toggle(&cat->settings[cat->setting_count++], "Game Bar", true, "Open Game Bar with Win+G");
    add_section(&cat->settings[cat->setting_count++], "Game Controller");
    add_button(&cat->settings[cat->setting_count++], "Test Controller", NULL, "Test game controller input");

    cat = &g_settings.categories[g_settings.category_count++];
    strcpy(cat->name, "Accessibility");
    strcpy(cat->icon, "A");
    cat->color = 0x339966;
    cat->setting_count = 0;
    add_section(&cat->settings[cat->setting_count++], "Vision");
    add_toggle(&cat->settings[cat->setting_count++], "Text Size", true, "Make text larger");
    add_slider(&cat->settings[cat->setting_count++], "Text Scaling", 100, 100, 225, "Adjust text size percentage");
    add_toggle(&cat->settings[cat->setting_count++], "Magnifier", false, "Turn on Magnifier (Win+Plus)");
    add_dropdown(&cat->settings[cat->setting_count++], "Magnifier View", (char[3][32]){"Full Screen", "Lens", "Docked"}, 3, 0, "How Magnifier appears");
    add_toggle(&cat->settings[cat->setting_count++], "Color Filters", false, "Apply color filter for color blindness");
    add_dropdown(&cat->settings[cat->setting_count++], "Color Filter", (char[6][32]){"Red-Green (Deuteranopia)", "Red-Green (Protanopia)", "Blue-Yellow (Tritanopia)", "Grayscale", "Grayscale Inverted", "Inverted"}, 6, 0, "Select color filter type");
    add_toggle(&cat->settings[cat->setting_count++], "High Contrast", false, "Use high contrast colors");
    add_section(&cat->settings[cat->setting_count++], "Hearing");
    add_toggle(&cat->settings[cat->setting_count++], "Mono Audio", false, "Combine left and right audio channels");
    add_toggle(&cat->settings[cat->setting_count++], "Flash Screen", false, "Flash screen for audio notifications");
    add_section(&cat->settings[cat->setting_count++], "Interaction");
    add_toggle(&cat->settings[cat->setting_count++], "Sticky Keys", false, "Press modifier keys one at a time");
    add_toggle(&cat->settings[cat->setting_count++], "Filter Keys", false, "Ignore brief or repeated keystrokes");
    add_toggle(&cat->settings[cat->setting_count++], "Toggle Keys", false, "Hear a tone when pressing Caps/Num/Scroll Lock");
    add_toggle(&cat->settings[cat->setting_count++], "Mouse Keys", false, "Control mouse with numeric keypad");

    cat = &g_settings.categories[g_settings.category_count++];
    strcpy(cat->name, "Privacy & Security");
    strcpy(cat->icon, "S");
    cat->color = 0xCC6633;
    cat->setting_count = 0;
    add_section(&cat->settings[cat->setting_count++], "Windows Security");
    add_button(&cat->settings[cat->setting_count++], "Open Windows Security", NULL, "View virus & threat protection");
    add_section(&cat->settings[cat->setting_count++], "General");
    add_toggle(&cat->settings[cat->setting_count++], "Advertising ID", false, "Let apps use advertising ID");
    add_toggle(&cat->settings[cat->setting_count++], "Website Tracking", false, "Let websites show locally relevant content");
    add_toggle(&cat->settings[cat->setting_count++], "App Launch Tracking", true, "Let Windows improve Start suggestions");
    add_toggle(&cat->settings[cat->setting_count++], "Suggested Content", true, "Show suggested content in Settings");
    add_section(&cat->settings[cat->setting_count++], "Speech");
    add_toggle(&cat->settings[cat->setting_count++], "Online Speech Recognition", false, "Use cloud-based speech recognition");
    add_section(&cat->settings[cat->setting_count++], "Inking & Typing");
    add_toggle(&cat->settings[cat->setting_count++], "Personalization", false, "Improve inking and typing recognition");
    add_button(&cat->settings[cat->setting_count++], "View Data", NULL, "View your inking and typing data");
    add_section(&cat->settings[cat->setting_count++], "Diagnostics & Feedback");
    add_dropdown(&cat->settings[cat->setting_count++], "Diagnostic Data", (char[2][32]){"Required", "Optional"}, 2, 0, "Send device data to Microsoft");
    add_toggle(&cat->settings[cat->setting_count++], "Tailored Experiences", true, "Get tips and recommendations");
    add_toggle(&cat->settings[cat->setting_count++], "Improve Inking/Typing", true, "Help improve recognition");
    add_button(&cat->settings[cat->setting_count++], "Delete Data", NULL, "Delete diagnostic data");
    add_section(&cat->settings[cat->setting_count++], "Activity History");
    add_toggle(&cat->settings[cat->setting_count++], "Store Activity History", true, "Store activity on this device");
    add_toggle(&cat->settings[cat->setting_count++], "Send Activity to Cloud", false, "Send activity history to Microsoft");

    cat = &g_settings.categories[g_settings.category_count++];
    strcpy(cat->name, "Windows Update");
    strcpy(cat->icon, "W");
    cat->color = 0x0078D7;
    cat->setting_count = 0;
    add_section(&cat->settings[cat->setting_count++], "Update Status");
    add_button(&cat->settings[cat->setting_count++], "Check for Updates", NULL, "Check for available updates now");
    add_button(&cat->settings[cat->setting_count++], "Pause Updates", NULL, "Pause updates for 7 days");
    add_section(&cat->settings[cat->setting_count++], "Advanced Options");
    add_toggle(&cat->settings[cat->setting_count++], "Receive Updates for Other Products", false, "Get updates for Microsoft products");
    add_toggle(&cat->settings[cat->setting_count++], "Download Over Metered Connections", false, "Download updates over metered connections");
    add_toggle(&cat->settings[cat->setting_count++], "Restart Notification", true, "Get notified when restart is needed");
    add_section(&cat->settings[cat->setting_count++], "Delivery Optimization");
    add_toggle(&cat->settings[cat->setting_count++], "Allow Downloads from Other PCs", true, "Get updates from other PCs on network");
    add_dropdown(&cat->settings[cat->setting_count++], "Download Mode", (char[3][32]){"Local Network", "Internet + Local", "Off"}, 3, 1, "Where to download updates from");
    add_section(&cat->settings[cat->setting_count++], "Update History");
    add_button(&cat->settings[cat->setting_count++], "View Update History", NULL, "See installed updates");
    add_button(&cat->settings[cat->setting_count++], "Uninstall Updates", NULL, "Remove problematic updates");
    add_section(&cat->settings[cat->setting_count++], "Active Hours");
    add_button(&cat->settings[cat->setting_count++], "Change Active Hours", NULL, "Set when you typically use this device");

    g_settings.selected_category = 0;
    g_settings.selected_setting = 0;
    g_settings.scroll_offset = 0;
    g_settings.search_query[0] = '\0';
    g_settings.search_active = false;
    g_settings.search_cursor = 0;
}

static void draw_sidebar(int x, int y, int w, int h) {
    fill_rect_fb(x, y, w, h, COL_SIDEBAR);
    draw_rect_fb(x + w - 1, y, 1, h, COL_BORDER);

    draw_string_fb(x + 24, y + 20, "Settings", COL_TEXT, 2);
    draw_string_fb(x + 24, y + 50, "Find a setting", COL_TEXT_MUTED, 1);

    int search_y = y + 70;
    fill_round_rect_fb(x + 16, search_y, w - 32, 32, 6, COL_BG);
    draw_round_rect_fb(x + 16, search_y, w - 32, 32, 6, COL_BORDER);
    draw_string_fb(x + 24, search_y + 8, "Search", COL_TEXT_MUTED, 1);

    int cy = search_y + 48;
    for (int i = 0; i < g_settings.category_count; i++) {
        settings_category_t *cat = &g_settings.categories[i];
        bool selected = (i == g_settings.selected_category);

        if (selected) {
            fill_rect_fb(x, cy, w, 40, COL_HOVER);
            fill_rect_fb(x, cy, 3, 40, cat->color);
        }

        fill_round_rect_fb(x + 16, cy + 8, 24, 24, 4, cat->color);
        draw_string_fb(x + 20, cy + 12, cat->icon, 0xFFFFFF, 1);
        draw_string_fb(x + 48, cy + 12, cat->name, selected ? COL_ACCENT : COL_TEXT, 1);

        cy += 44;
    }
}

static void draw_toggle(int x, int y, int w, int h, bool value, bool hover) {
    int track_w = 52;
    int track_h = 28;
    int track_x = x + w - track_w - 16;
    int track_y = y + (h - track_h) / 2;
    uint32_t track_color = value ? COL_TOGGLE_ON : COL_TOGGLE_OFF;
    if (hover) {
        track_color = value ? COL_ACCENT_HOVER : 0x555577;
    }
    fill_round_rect_fb(track_x, track_y, track_w, track_h, track_h / 2, track_color);

    int thumb_size = track_h - 4;
    int thumb_x = value ? track_x + track_w - thumb_size - 2 : track_x + 2;
    fill_round_rect_fb(thumb_x, track_y + 2, thumb_size, thumb_size, thumb_size / 2, 0xFFFFFF);
}

static void draw_slider(int x, int y, int w, int h, int value, int min, int max, bool hover) {
    int track_w = 200;
    int track_h = 4;
    int track_x = x + w - track_w - 16;
    int track_y = y + (h - track_h) / 2;
    fill_round_rect_fb(track_x, track_y, track_w, track_h, 2, COL_BORDER);

    int range = max - min;
    int fill_w = 0;
    if (range > 0) {
        fill_w = track_w * (value - min) / range;
    }
    fill_round_rect_fb(track_x, track_y, fill_w, track_h, 2, COL_ACCENT);

    int thumb_r = 10;
    int thumb_x = track_x + fill_w;
    int thumb_y = track_y + track_h / 2;
    uint32_t thumb_color = hover ? COL_ACCENT_HOVER : COL_ACCENT;
    fill_round_rect_fb(thumb_x - thumb_r, thumb_y - thumb_r, thumb_r * 2, thumb_r * 2, thumb_r, thumb_color);

    char val_str[16];
    snprintf(val_str, 16, "%d", value);
    draw_string_fb(track_x + track_w + 16, track_y - 4, val_str, COL_TEXT_MUTED, 1);
}

static void draw_dropdown(int x, int y, int w, int h, setting_item_t *s, bool hover) {
    int dd_w = 200;
    int dd_x = x + w - dd_w - 16;
    int dd_y = y + (h - 32) / 2;
    uint32_t bg = hover ? COL_HOVER : COL_CARD;
    fill_round_rect_fb(dd_x, dd_y, dd_w, 32, 4, bg);
    draw_round_rect_fb(dd_x, dd_y, dd_w, 32, 4, hover ? COL_ACCENT : COL_BORDER);

    draw_string_fb(dd_x + 12, dd_y + 8, s->data.dropdown.options[s->data.dropdown.selected], COL_TEXT, 1);
    draw_string_fb(dd_x + dd_w - 28, dd_y + 8, "v", COL_TEXT_MUTED, 1);
}

static void draw_button(int x, int y, int w, int h, const char *label, bool hover) {
    int btn_w = 140;
    int btn_h = 32;
    int btn_x = x + w - btn_w - 16;
    int btn_y = y + (h - btn_h) / 2;
    uint32_t bg = hover ? COL_ACCENT_HOVER : COL_ACCENT;
    fill_round_rect_fb(btn_x, btn_y, btn_w, btn_h, 4, bg);
    draw_string_fb(btn_x + (btn_w - str_width_fb(label, 1)) / 2, btn_y + 8, label, 0xFFFFFF, 1);
}

static void draw_setting_item(int x, int y, int w, int h, setting_item_t *s, int index, bool selected) {
    (void)index;
    if (s->type == SET_TYPE_SECTION) {
        draw_string_fb(x, y + 8, s->label, COL_ACCENT, 1);
        draw_rect_fb(x, y + h / 2, w, 1, COL_BORDER);
        return;
    }

    uint32_t bg = selected ? COL_HOVER : COL_CARD;
    if (selected) {
        fill_round_rect_fb(x, y, w, h, 6, bg);
        draw_round_rect_fb(x, y, w, h, 6, COL_ACCENT);
    } else {
        fill_round_rect_fb(x, y, w, h, 6, bg);
        draw_round_rect_fb(x, y, w, h, 6, COL_BORDER);
    }

    draw_string_fb(x + 20, y + 8, s->label, COL_TEXT, 1);
    draw_string_fb(x + 20, y + 26, s->description, COL_TEXT_MUTED, 1);

    switch (s->type) {
        case SET_TYPE_TOGGLE:
            draw_toggle(x, y, w, h, s->data.toggle.value, selected);
            break;
        case SET_TYPE_SLIDER:
            draw_slider(x, y, w, h, s->data.slider.value, s->data.slider.min, s->data.slider.max, selected);
            break;
        case SET_TYPE_DROPDOWN:
            draw_dropdown(x, y, w, h, s, selected);
            break;
        case SET_TYPE_BUTTON:
            draw_button(x, y, w, h, s->label, selected);
            break;
        default:
            break;
    }
}

static void draw_content_area(int x, int y, int w, int h) {
    fill_rect_fb(x, y, w, h, COL_BG);

    if (g_settings.search_active) {
        int search_y = y + 20;
        fill_round_rect_fb(x + 32, search_y, w - 64, 40, 6, COL_CARD);
        draw_string_fb(x + 48, search_y + 12, "Search: ", COL_TEXT_MUTED, 1);
        draw_string_fb(x + 120, search_y + 12, g_settings.search_query, COL_TEXT, 1);
        if ((timer_get_ticks() / 500) % 2 == 0) {
            draw_rect_fb(x + 120 + str_width_fb(g_settings.search_query, 1), search_y + 12, 1, 14, COL_ACCENT);
        }
        y += 60;
        h -= 60;
    }

    settings_category_t *cat = &g_settings.categories[g_settings.selected_category];
    if (!cat) return;

    draw_string_fb(x + 32, y + 20, cat->name, COL_TEXT, 2);
    draw_rect_fb(x + 32, y + 52, w - 64, 1, COL_BORDER);

    int item_h = 60;
    int start_y = y + 64;
    int visible = (h - 64) / item_h;

    for (int i = 0; i < visible && (g_settings.scroll_offset + i) < cat->setting_count; i++) {
        int idx = g_settings.scroll_offset + i;
        setting_item_t *s = &cat->settings[idx];
        bool selected = (idx == g_settings.selected_setting);
        draw_setting_item(x + 24, start_y + i * item_h, w - 48, item_h - 8, s, idx, selected);
    }

    if (cat->setting_count > visible) {
        int sb_x = x + w - 20;
        int sb_h = (h - 64) * visible / cat->setting_count;
        int sb_y = start_y + (h - 64) * g_settings.scroll_offset / cat->setting_count;
        fill_round_rect_fb(sb_x, sb_y, 12, sb_h, 6, COL_ACCENT);
    }
}

static void draw_window_content(window_t *win) {
    (void)win;
    int sidebar_w = 280;
    int x = 0;
    int y = 0;
    int w = g_win->width;
    int h = g_win->height;

    draw_sidebar(x, y, sidebar_w, h);
    draw_content_area(x + sidebar_w, y, w - sidebar_w, h);
}

/* ============================================================
 * Window-Manager-driven interface
 * ============================================================ */

static void settings_update(void);
static void settings_draw(void);
static void settings_handle_key_down(int key);
static void settings_handle_key_up(int key);
static void settings_handle_mouse_move(int x, int y);
static void settings_handle_mouse_down(int x, int y, int button);
static void settings_handle_mouse_up(int x, int y, int button);
static void settings_handle_mouse_wheel(int delta);

static void settings_wm_draw(struct window *win) {
    (void)win;
    settings_update();
    settings_draw();
}

static void settings_wm_key_down(struct window *win, int key) {
    (void)win;
    settings_handle_key_down(key);
}

static void settings_wm_key_up(struct window *win, int key) {
    (void)win;
    settings_handle_key_up(key);
}

static void settings_wm_mouse_down(struct window *win, int x, int y, int button) {
    (void)win;
    settings_handle_mouse_down(x, y, button);
}

static void settings_wm_mouse_up(struct window *win, int x, int y, int button) {
    (void)win;
    settings_handle_mouse_up(x, y, button);
}

static void settings_wm_mouse_move(struct window *win, int x, int y) {
    (void)win;
    settings_handle_mouse_move(x, y);
}

static void settings_wm_mouse_wheel(struct window *win, int delta) {
    (void)win;
    settings_handle_mouse_wheel(delta);
}

static void settings_init_window(void) {
    fb_info_t *fb = fb_get_info();
    int win_w = 1000;
    int win_h = 700;
    int win_x = (fb->width - win_w) / 2;
    int win_y = (fb->height - win_h) / 2;

    g_win = wm_get_window(wm_create_window("Settings", win_x, win_y, win_w, win_h, 0));
    if (!g_win) return;
    wm_focus_window(g_win->id);

    g_win->user_data = &g_settings;

    g_win->draw_content = settings_wm_draw;
    g_win->on_mouse_down = settings_wm_mouse_down;
    g_win->on_mouse_up = settings_wm_mouse_up;
    g_win->on_mouse_move = settings_wm_mouse_move;
    g_win->on_mouse_wheel = settings_wm_mouse_wheel;
    g_win->on_key_down = settings_wm_key_down;
    g_win->on_key_up = settings_wm_key_up;
}

static void settings_update(void) {
}

static void settings_draw(void) {
    if (!g_win) return;
    draw_window_content(g_win);
}

static void settings_handle_mouse_down(int mx, int my, int button) {
    int sidebar_w = 280;

    if (button == 1) {
        if (mx < sidebar_w) {
            int cy = 122;
            for (int i = 0; i < g_settings.category_count; i++) {
                if (my >= cy && my < cy + 40) {
                    g_settings.selected_category = i;
                    g_settings.selected_setting = 0;
                    g_settings.scroll_offset = 0;
                    break;
                }
                cy += 44;
            }

            if (my >= 70 && my < 102) {
                g_settings.search_active = true;
                g_settings.search_query[0] = '\0';
                g_settings.search_cursor = 0;
            }
        } else {
            g_settings.search_active = false;

            int content_x = sidebar_w;
            int content_y = g_settings.search_active ? 60 : 0;
            int content_w = g_win->width - sidebar_w;
            int start_y = content_y + 64;
            int item_h = 60;
            int visible = (g_win->height - 64 - (g_settings.search_active ? 60 : 0)) / item_h;

            settings_category_t *cat = &g_settings.categories[g_settings.selected_category];
            for (int i = 0; i < visible && (g_settings.scroll_offset + i) < cat->setting_count; i++) {
                int idx = g_settings.scroll_offset + i;
                int iy = start_y + i * item_h;
                if (my >= iy && my < iy + item_h - 8) {
                    g_settings.selected_setting = idx;
                    setting_item_t *s = &cat->settings[idx];

                    if (s->type == SET_TYPE_TOGGLE) {
                        s->data.toggle.value = !s->data.toggle.value;
                    } else if (s->type == SET_TYPE_SLIDER) {
                        int track_x = content_x + content_w - 200 - 40;
                        int rel_x = mx - track_x;
                        if (rel_x < 0) rel_x = 0;
                        if (rel_x > 200) rel_x = 200;
                        int range = s->data.slider.max - s->data.slider.min;
                        s->data.slider.value = s->data.slider.min + (rel_x * range) / 200;
                    } else if (s->type == SET_TYPE_BUTTON && s->data.button.callback) {
                        s->data.button.callback();
                    }
                    break;
                }
            }
        }
    }
}

static void settings_handle_mouse_up(int mx, int my, int button) {
    (void)mx; (void)my; (void)button;
}

static void settings_handle_mouse_wheel(int delta) {
    settings_category_t *cat = &g_settings.categories[g_settings.selected_category];
    if (!cat) return;

    if (delta > 0 && g_settings.scroll_offset > 0) {
        g_settings.scroll_offset--;
    } else if (delta < 0 && g_settings.scroll_offset < cat->setting_count - 1) {
        g_settings.scroll_offset++;
    }
}

static void settings_handle_key_down(int key) {
    if (g_settings.search_active) {
        if (key == 0x1C) {
            g_settings.search_active = false;
            kprintf("[Settings] Search: %s\n", g_settings.search_query);
        } else if (key == 0x0E && g_settings.search_cursor > 0) {
            g_settings.search_cursor--;
            int len = strlen(g_settings.search_query);
            memmove(&g_settings.search_query[g_settings.search_cursor], &g_settings.search_query[g_settings.search_cursor + 1], len - g_settings.search_cursor);
            g_settings.search_query[len - 1] = '\0';
        } else {
            uint8_t mods = keyboard_get_modifiers();
            bool ctrl = mods & KMOD_CTRL;
            bool alt = mods & KMOD_ALT;
            if (!ctrl && !alt && key >= 0x20 && key <= 0x7E && g_settings.search_cursor < 63) {
                char c = keyboard_keycode_to_ascii(key, mods);
                if (c) {
                    int len = strlen(g_settings.search_query);
                    memmove(&g_settings.search_query[g_settings.search_cursor + 1], &g_settings.search_query[g_settings.search_cursor], len - g_settings.search_cursor + 1);
                    g_settings.search_query[g_settings.search_cursor] = c;
                    g_settings.search_cursor++;
                }
            }
        }
        return;
    }

    settings_category_t *cat = &g_settings.categories[g_settings.selected_category];
    if (!cat) return;

    switch (key) {
        case 0x48: // Up
            if (g_settings.selected_setting > 0) {
                g_settings.selected_setting--;
                if (g_settings.selected_setting < g_settings.scroll_offset) g_settings.scroll_offset = g_settings.selected_setting;
            } else if (g_settings.selected_category > 0) {
                g_settings.selected_category--;
                cat = &g_settings.categories[g_settings.selected_category];
                g_settings.selected_setting = cat->setting_count - 1;
                g_settings.scroll_offset = cat->setting_count > 10 ? cat->setting_count - 10 : 0;
            }
            break;
        case 0x50: // Down
            if (g_settings.selected_setting < cat->setting_count - 1) {
                g_settings.selected_setting++;
                if (g_settings.selected_setting >= g_settings.scroll_offset + 10) g_settings.scroll_offset++;
            } else if (g_settings.selected_category < g_settings.category_count - 1) {
                g_settings.selected_category++;
                g_settings.selected_setting = 0;
                g_settings.scroll_offset = 0;
            }
            break;
        case 0x4B: // Left
            if (cat->settings[g_settings.selected_setting].type == SET_TYPE_SLIDER) {
                setting_item_t *s = &cat->settings[g_settings.selected_setting];
                if (s->data.slider.value > s->data.slider.min) s->data.slider.value--;
            } else if (cat->settings[g_settings.selected_setting].type == SET_TYPE_DROPDOWN) {
                setting_item_t *s = &cat->settings[g_settings.selected_setting];
                if (s->data.dropdown.selected > 0) s->data.dropdown.selected--;
            }
            break;
        case 0x4D: // Right
            if (cat->settings[g_settings.selected_setting].type == SET_TYPE_SLIDER) {
                setting_item_t *s = &cat->settings[g_settings.selected_setting];
                if (s->data.slider.value < s->data.slider.max) s->data.slider.value++;
            } else if (cat->settings[g_settings.selected_setting].type == SET_TYPE_DROPDOWN) {
                setting_item_t *s = &cat->settings[g_settings.selected_setting];
                if (s->data.dropdown.selected < s->data.dropdown.count - 1) s->data.dropdown.selected++;
            } else if (cat->settings[g_settings.selected_setting].type == SET_TYPE_TOGGLE) {
                cat->settings[g_settings.selected_setting].data.toggle.value = !cat->settings[g_settings.selected_setting].data.toggle.value;
            }
            break;
        case 0x1C: // Enter
            if (cat->settings[g_settings.selected_setting].type == SET_TYPE_BUTTON && cat->settings[g_settings.selected_setting].data.button.callback) {
                cat->settings[g_settings.selected_setting].data.button.callback();
            } else if (cat->settings[g_settings.selected_setting].type == SET_TYPE_TOGGLE) {
                cat->settings[g_settings.selected_setting].data.toggle.value = !cat->settings[g_settings.selected_setting].data.toggle.value;
            }
            break;
        case 0x01: // Escape
            if (g_win) wm_destroy_window(g_win->id);
            g_win = NULL;
            break;
        case 0x3F: // F5
            break;
    }
}

static void settings_handle_key_up(int key) {
    (void)key;
}

static void settings_handle_mouse_move(int x, int y) {
    (void)x; (void)y;
}

static widget_type_t map_setting_type(setting_type_t t) {
    switch (t) {
        case SET_TYPE_TOGGLE:   return WIDGET_CHECKBOX;
        case SET_TYPE_SLIDER:   return WIDGET_SLIDER;
        case SET_TYPE_DROPDOWN: return WIDGET_BUTTON;
        case SET_TYPE_BUTTON:   return WIDGET_BUTTON;
        case SET_TYPE_TEXT:     return WIDGET_TEXT;
        case SET_TYPE_SECTION:  return WIDGET_LABEL;
        default:                return WIDGET_BOX;
    }
}

static widget_t *build_settings_scene(void) {
    widget_t *root = scene_create_root();
    if (!root) return NULL;
    root->rect.x = 0;
    root->rect.y = 0;
    root->rect.w = 1000;
    root->rect.h = 700;
    root->first_child = NULL;
    root->last_child = NULL;
    widget_t *prev_cat = NULL;
    for (int ci = 0; ci < g_settings.category_count; ++ci) {
        settings_category_t *cat = &g_settings.categories[ci];
        widget_t *cat_w = (widget_t *)kmalloc(sizeof(widget_t));
        if (!cat_w) continue;
        cat_w->type = WIDGET_BOX;
        cat_w->rect.x = 0;
        cat_w->rect.y = ci * 10;
        cat_w->rect.w = root->rect.w;
        cat_w->rect.h = 40;
        cat_w->parent = root;
        cat_w->first_child = NULL;
        cat_w->last_child = NULL;
        cat_w->next_sibling = NULL;
        cat_w->prev_sibling = prev_cat;
        if (prev_cat) prev_cat->next_sibling = cat_w;
        else root->first_child = cat_w;
        prev_cat = cat_w;
        root->last_child = cat_w;
        widget_t *prev_set = NULL;
        for (int si = 0; si < cat->setting_count; ++si) {
            setting_item_t *s = &cat->settings[si];
            if (s->type == SET_TYPE_SECTION) continue;
            widget_t *w = (widget_t *)kmalloc(sizeof(widget_t));
            if (!w) continue;
            w->type = map_setting_type(s->type);
            w->rect.x = 280;
            w->rect.y = cat_w->rect.y + si * 60;
            w->rect.w = root->rect.w - 280;
            w->rect.h = 52;
            w->parent = cat_w;
            w->first_child = NULL;
            w->last_child = NULL;
            w->next_sibling = NULL;
            w->prev_sibling = prev_set;
            if (prev_set) prev_set->next_sibling = w;
            else cat_w->first_child = w;
            prev_set = w;
            cat_w->last_child = w;
        }
    }
    layout_run(root);
    return root;
}

void settings_main(void) {
    static bool inited = false;
    if (inited) return;
    inited = true;

    init_settings();

    settings_init_window();

    widget_t *root = build_settings_scene();
    if (root) {
        widget_invalidate(root);
        /* scene retained for lifetime of app */
        (void)root;
    }

    kprintf("[Settings] Settings app started\n");
}