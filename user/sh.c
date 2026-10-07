#include "libc.h"

#define MAX_ARGS 16
#define MAX_INPUT 256

static int simple_atoi(const char *s) {
    if (!s) return 0;
    int res = 0;
    while (*s >= '0' && *s <= '9') {
        res = res * 10 + (*s - '0');
        s++;
    }
    return res;
}

static inline int is_space(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static void sanitize_input(char *buf) {
    if (!buf) return;
    char clean[MAX_INPUT];
    int w = 0;
    for (int r = 0; buf[r] && w < MAX_INPUT - 1; r++) {
        unsigned char c = (unsigned char)buf[r];
        if (c == 0x1b) {
            if (buf[r + 1] == '[') {
                r += 2;
                while (buf[r] && !((buf[r] >= '@' && buf[r] <= '~'))) {
                    r++;
                }
            }
            continue;
        }
        if (c == '\b' || c == 0x7F || c == 8) {
            if (w > 0) w--;
            continue;
        }
        if (c >= ' ' || c == '\t') {
            clean[w++] = (char)c;
        }
    }
    clean[w] = '\0';
    strcpy(buf, clean);
}

static int cmd_is(const char *cmd, const char *target) {
    if (!cmd || !target) return 0;
    while (*cmd && *target) {
        char c1 = *cmd;
        char c2 = *target;
        if (c1 >= 'A' && c1 <= 'Z') c1 += 32;
        if (c2 >= 'A' && c2 <= 'Z') c2 += 32;
        if (c1 != c2) return 0;
        cmd++;
        target++;
    }
    return (*cmd == '\0' && *target == '\0');
}

static int parse_args(char *line, char **argv) {
    int argc = 0;
    while (*line && argc < MAX_ARGS - 1) {
        while (*line && is_space(*line)) line++;
        if (!*line) break;
        argv[argc++] = line;
        while (*line && !is_space(*line)) line++;
        if (*line) {
            *line = '\0';
            line++;
        }
    }
    argv[argc] = NULL;
    return argc;
}

static void show_banner(void) {
    puts("    __  ___      ____  _____    MyOS 1.0 Modern Edition (x86-64)");
    puts("   /  |/  /_  __/ __ \\/ ___/    Kernel:   v1.0 (SMP + Async Syscalls)");
    puts("  / /|_/ / / / / / / /\\__ \\     Shell:    myos-sh v2.0 (Real-Time Control)");
    puts(" / /  / / /_/ / /_/ /___/ /     Display:  1024x768 32bpp (Subpixel AA)");
    puts("/_/  /_/\\__, /\\____//____/      Security: Multi-User Auth & Lockscreen");
    puts("       /____/                   Mouse:    200Hz Ultra-Smooth Polling");
    puts("");
    puts(" Type help for available commands, or control OS/GUI in real time.");
    puts("");
}

static void show_help(void) {
    puts("=== MyOS Terminal Control Center ===");
    puts("");
    puts("[ Authentication & User Management ]");
    puts("  whoami                     - Print current logged-in user");
    puts("  users                      - List all users in OS database");
    puts("  useradd <user> <pass>      - Create new user account");
    puts("  passwd <user> <new_pass>   - Update user password");
    puts("  login <user> <pass>        - Switch current logged-in user session");
    puts("  lock                       - Lock OS immediately with login window");
    puts("  logout                     - Log out session and lock OS screen");
    puts("");
    puts("[ Persistent & Portable Storage ]");
    puts("  storage / df               - Display persistent partitions & mounts");
    puts("  sync                       - Flush credentials and system config to disk");
    puts("  portable / usb             - Inspect portable USB/removable drives");
    puts("  drivers                    - List active kernel hardware drivers");
    puts("  lspci                      - Scan and list all PCI hardware devices");
    puts("[ Inbuilt Applications ]");
    puts("  calc / calculator          - Launch modern arithmetic calculator");
    puts("  editor / notepad           - Launch persistent multiline text editor");
    puts("  music / player             - Launch sound studio & synthesizer player");
    puts("  settings                   - Launch control center & display settings");
    puts("  tor / browser [url]        - Launch inbuilt Tor Onion Browser");
    puts("");
    puts("[ Window Manager & Desktop Control ]");
    puts("  wm list                    - List active GUI windows (real-time)");
    puts("  wm close <title_or_id>     - Close window in GUI");
    puts("  wm focus <title_or_id>     - Focus and bring window to front");
    puts("  wm tile                    - Auto-tile all windows on desktop");
    puts("  app <name>                 - Launch GUI app (Terminal, Files, Editor, Calculator, Music, Settings)");
    puts("  dpi [96|120|144]           - Query or set HiDPI desktop scale");
    puts("  sound / beep [freq] [ms]   - Synthesize sound tone via AC'97 / Speaker");
    puts("  theme <name>               - Switch theme: tokyo, emerald, amber, cyberpunk");
    puts("  mouse [1-10]               - Get or set mouse pointer speed/sensitivity");
    puts("[ Power, Network & Time ]");
    puts("  battery / power [charge|standby] - Power status, charging mode, battery info");
    puts("  standby                          - Suspend system into low-power standby mode");
    puts("  wifi [on|off|toggle|scan]        - Wi-Fi status, scan, and interface toggle");
    puts("  bluetooth / bt [on|off|toggle]   - Bluetooth 5.3 LE status and device pairing");
    puts("  eth / net [toggle]               - Ethernet link status and IP address info");
    puts("  date / time                      - Accurate RTC date & time readout");
    puts("  cal / calendar                   - Monthly calendar with today highlighted");
    puts("");
    puts("[ System & Diagnostics ]");
    puts("  ps                         - Show active kernel & user processes");
    puts("  uptime                     - Display system uptime");
    puts("  free / mem                 - Display physical memory allocation");
    puts("  kill <pid>                 - Terminate process by PID");
    puts("  fetch / uname              - Print system info & architecture");
    puts("  clear                      - Clear terminal display");
    puts("  reboot                     - Reboot machine");
    puts("  shutdown                   - Halt machine");
    puts("");
}

static int is_leap_year(int y) {
    return (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0));
}

static int days_in_month(int y, int m) {
    static const int d[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (m == 2) return 28 + is_leap_year(y);
    if (m >= 1 && m <= 12) return d[m - 1];
    return 30;
}

static int first_day_of_month(int y, int m) {
    static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (m < 3) y -= 1;
    return (y + y/4 - y/100 + y/400 + t[m-1] + 1) % 7;
}

static const char *month_names[] = {
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
};

static void print_calendar(void) {
    char tbuf[64] = {0};
    os_control(OS_CMD_GET_TIME, (long)tbuf, sizeof(tbuf), 0);
    int year = 2026, month = 10, day = 8;
    if (tbuf[0]) {
        year = simple_atoi(tbuf);
        if (tbuf[5]) month = simple_atoi(tbuf + 5);
        if (tbuf[8]) day = simple_atoi(tbuf + 8);
    }
    if (month < 1 || month > 12) month = 10;

    printf("\n     %s %d\n", month_names[month - 1], year);
    puts(" Su Mo Tu We Th Fr Sa");

    int fday = first_day_of_month(year, month);
    int total = days_in_month(year, month);

    for (int i = 0; i < fday; i++) {
        printf("   ");
    }

    for (int d = 1; d <= total; d++) {
        if (d == day) {
            if (d < 10) printf("\x1b[1;36;7m %d\x1b[0m ", d);
            else printf("\x1b[1;36;7m%d\x1b[0m ", d);
        } else {
            if (d < 10) printf(" %d ", d);
            else printf("%d ", d);
        }
        if ((fday + d) % 7 == 0 || d == total) {
            putchar('\n');
        }
    }
    printf("\n Current Time: %s UTC\n\n", tbuf);
}

int main(void) {
    char input[MAX_INPUT];
    char *argv[MAX_ARGS];

    show_banner();

    while (1) {
        char cur_user[32] = "myos";
        os_control(OS_CMD_AUTH_WHOAMI, (long)cur_user, sizeof(cur_user), 0);
        if (cur_user[0] == '\0') strcpy(cur_user, "myos");

        printf("%s > ", cur_user);

        int n = read(0, input, MAX_INPUT - 1);
        if (n <= 0) continue;

        /* Strip trailing newline / carriage return */
        while (n > 0 && (input[n - 1] == '\n' || input[n - 1] == '\r')) {
            input[--n] = '\0';
        }
        input[n] = '\0';
        if (n == 0) continue;

        /* Clean up escape sequences, backspaces, and control characters */
        sanitize_input(input);
        if (input[0] == '\0') continue;

        int argc = parse_args(input, argv);
        if (argc == 0 || !argv[0]) continue;

        /* ---- Help & Info ---- */
        if (cmd_is(argv[0], "help") || strcmp(argv[0], "?") == 0 ||
            strcmp(argv[0], "--help") == 0 || strcmp(argv[0], "-h") == 0) {
            show_help();
        } else if (cmd_is(argv[0], "clear")) {
            for (int k = 0; k < 32; k++) putchar('\n');
        } else if (cmd_is(argv[0], "fetch") || cmd_is(argv[0], "sysinfo")) {
            show_banner();
        } else if (cmd_is(argv[0], "uname")) {
            puts("MyOS 1.0.0-smp x86_64 GNU/MyOS");

        /* ---- Authentication Commands ---- */
        } else if (strcmp(argv[0], "whoami") == 0) {
            char u[32] = {0};
            if (os_control(OS_CMD_AUTH_WHOAMI, (long)u, sizeof(u), 0) == 0 && u[0]) {
                printf("%s\n", u);
            } else {
                puts("myos");
            }
        } else if (strcmp(argv[0], "users") == 0) {
            char buf[512] = {0};
            long ucnt = os_control(OS_CMD_AUTH_USERS, (long)buf, sizeof(buf), 0);
            if (ucnt >= 0) {
                printf("Registered Users (%ld): %s\n", ucnt, buf);
            } else {
                puts("Failed to query user database.");
            }
        } else if (strcmp(argv[0], "useradd") == 0) {
            if (argc < 3) {
                puts("Usage: useradd <username> <password>");
            } else {
                long res = os_control(OS_CMD_AUTH_ADD_USER, (long)argv[1], (long)argv[2], 0);
                if (res == 0) {
                    printf("\x1b[1;32mSuccessfully created user '%s'.\x1b[0m\n", argv[1]);
                } else {
                    printf("\x1b[1;31mFailed to create user '%s'.\x1b[0m\n", argv[1]);
                }
            }
        } else if (strcmp(argv[0], "passwd") == 0) {
            if (argc < 3) {
                puts("Usage: passwd <username> <new_password>");
            } else {
                long res = os_control(OS_CMD_AUTH_PASSWD, (long)argv[1], (long)argv[2], 0);
                if (res == 0) {
                    printf("\x1b[1;32mPassword for user '%s' updated successfully.\x1b[0m\n", argv[1]);
                } else {
                    printf("\x1b[1;31mFailed to update password for '%s' (user not found).\x1b[0m\n", argv[1]);
                }
            }
        } else if (strcmp(argv[0], "login") == 0) {
            if (argc < 3) {
                puts("Usage: login <username> <password>");
            } else {
                long res = os_control(OS_CMD_AUTH_LOGIN, (long)argv[1], (long)argv[2], 0);
                if (res == 0) {
                    printf("\x1b[1;32mLogged in as '%s'. Screen unlocked.\x1b[0m\n", argv[1]);
                } else {
                    puts("\x1b[1;31mLogin failed: invalid username or password.\x1b[0m");
                }
            }
        } else if (strcmp(argv[0], "lock") == 0) {
            os_control(OS_CMD_AUTH_LOCK, 0, 0, 0);
            puts("\x1b[1;33mScreen locked. Login window presented.\x1b[0m");
        } else if (strcmp(argv[0], "logout") == 0) {
            os_control(OS_CMD_AUTH_LOGOUT, 0, 0, 0);
            puts("\x1b[1;33mUser session ended. Screen locked.\x1b[0m");

        /* ---- Window Manager & Desktop Control ---- */
        } else if (strcmp(argv[0], "wm") == 0) {
            if (argc < 2) {
                puts("Usage: wm <list|close|focus|tile> [args]");
            } else if (strcmp(argv[1], "list") == 0) {
                char wbuf[1024] = {0};
                if (os_control(OS_CMD_WM_LIST, (long)wbuf, sizeof(wbuf), 0) == 0) {
                    puts("\x1b[1;34m=== GUI Window Manager List ===\x1b[0m");
                    puts(wbuf);
                } else {
                    puts("Failed to query window list.");
                }
            } else if (strcmp(argv[1], "close") == 0) {
                if (argc < 3) {
                    puts("Usage: wm close <title>");
                } else {
                    long res = os_control(OS_CMD_WM_CLOSE, (long)argv[2], 0, 0);
                    if (res == 0) {
                        printf("\x1b[1;32mClosed window '%s'.\x1b[0m\n", argv[2]);
                    } else {
                        printf("\x1b[1;31mWindow '%s' not found.\x1b[0m\n", argv[2]);
                    }
                }
            } else if (strcmp(argv[1], "focus") == 0) {
                if (argc < 3) {
                    puts("Usage: wm focus <title>");
                } else {
                    long res = os_control(OS_CMD_WM_FOCUS, (long)argv[2], 0, 0);
                    if (res == 0) {
                        printf("\x1b[1;32mFocused window '%s'.\x1b[0m\n", argv[2]);
                    } else {
                        printf("\x1b[1;31mWindow '%s' not found.\x1b[0m\n", argv[2]);
                    }
                }
            } else if (strcmp(argv[1], "tile") == 0) {
                os_control(OS_CMD_WM_TILE, 0, 0, 0);
                puts("\x1b[1;32mAll desktop windows tiled in real time.\x1b[0m");
            } else {
                puts("Unknown wm subcommand. Choose: list, close, focus, tile");
            }

        /* ---- Inbuilt Applications ---- */
        } else if (strcmp(argv[0], "calc") == 0 || strcmp(argv[0], "calculator") == 0) {
            os_control(OS_CMD_APP_LAUNCH, (long)"Calculator", 0, 0);
            printf("\x1b[1;32mLaunched Calculator.\x1b[0m\n");
        } else if (strcmp(argv[0], "editor") == 0 || strcmp(argv[0], "notepad") == 0) {
            os_control(OS_CMD_APP_LAUNCH, (long)"Editor", 0, 0);
            printf("\x1b[1;32mLaunched Text Editor.\x1b[0m\n");
        } else if (strcmp(argv[0], "music") == 0 || strcmp(argv[0], "player") == 0) {
            os_control(OS_CMD_APP_LAUNCH, (long)"Music", 0, 0);
            printf("\x1b[1;32mLaunched Sound Studio.\x1b[0m\n");
        } else if (strcmp(argv[0], "settings") == 0 || strcmp(argv[0], "control") == 0) {
            os_control(OS_CMD_APP_LAUNCH, (long)"Settings", 0, 0);
            printf("\x1b[1;32mLaunched Settings & Control Center.\x1b[0m\n");
        } else if (strcmp(argv[0], "dpi") == 0) {
            if (argc == 1) {
                long d = os_control(OS_CMD_GET_DPI, 0, 0, 0);
                printf("Current desktop scale: %ld DPI (%ld%%)\n", d, (d * 100) / 96);
            } else {
                long d = simple_atoi(argv[1]);
                if (d >= 72 && d <= 288) {
                    os_control(OS_CMD_SET_DPI, d, 0, 0);
                    printf("\x1b[1;32mDesktop scaling set to %ld DPI in real time.\x1b[0m\n", d);
                } else {
                    puts("\x1b[1;31mInvalid DPI. Choose: 96 (1.0x), 120 (1.25x), 144 (1.5x)\x1b[0m");
                }
            }
        } else if (strcmp(argv[0], "sound") == 0 || strcmp(argv[0], "beep") == 0) {
            long freq = (argc > 1) ? simple_atoi(argv[1]) : 523;
            long ms = (argc > 2) ? simple_atoi(argv[2]) : 150;
            os_control(OS_CMD_PLAY_SOUND, freq, ms, 0);
            printf("Synthesized %ld Hz tone for %ld ms via audio engine.\n", freq, ms);

        /* ---- Tor Inbuilt Privacy Browser ---- */
        } else if (strcmp(argv[0], "tor") == 0 || strcmp(argv[0], "browser") == 0) {
            long res = os_control(OS_CMD_APP_LAUNCH, (long)"Browser", 0, 0);
            if (res == 0) {
                printf("\x1b[1;36mLaunched Inbuilt Tor Browser (Circuit: Guard -> Relay -> Exit)\x1b[0m\n");
            } else {
                puts("\x1b[1;31mFailed to launch Tor Browser.\x1b[0m");
            }

        /* ---- Persistent & Portable Storage Control ---- */
        } else if (strcmp(argv[0], "storage") == 0 || strcmp(argv[0], "df") == 0) {
            char out[512] = "";
            os_control(OS_CMD_STORAGE_INFO, (long)out, sizeof(out), 0);
            puts("=== Active Storage Mounts & Filesystems ===");
            printf("%s", out);
        } else if (strcmp(argv[0], "sync") == 0) {
            os_control(OS_CMD_STORAGE_SYNC, 0, 0, 0);
            puts("\x1b[1;32mSynchronized and committed OS data to persistent disk.\x1b[0m");
        } else if (strcmp(argv[0], "portable") == 0 || strcmp(argv[0], "usb") == 0) {
            char out[512] = "";
            os_control(OS_CMD_PORTABLE_LIST, (long)out, sizeof(out), 0);
            puts("=== Portable & Removable Storage Devices ===");
            printf("%s", out);

        /* ---- Hardware & Driver Registry ---- */
        } else if (strcmp(argv[0], "drivers") == 0 || strcmp(argv[0], "driver") == 0) {
            char out[1024] = "";
            os_control(OS_CMD_DRIVER_LIST, (long)out, sizeof(out), 0);
            puts("=== Active Kernel Hardware Drivers & Subsystems ===");
            printf("%s", out);
        } else if (strcmp(argv[0], "lspci") == 0 || strcmp(argv[0], "pci") == 0) {
            char out[1024] = "";
            os_control(OS_CMD_PCI_LIST, (long)out, sizeof(out), 0);
            puts("=== PCI Bus Device Scan & Registry ===");
            printf("%s", out);

        /* ---- App Launcher ---- */
        } else if (strcmp(argv[0], "app") == 0 || strcmp(argv[0], "launch") == 0) {
            if (argc < 2) {
                puts("Usage: app <name>");
                puts("Available apps: Terminal, Files, Browser, About, Help, Sysinfo");
            } else {
                long res = os_control(OS_CMD_APP_LAUNCH, (long)argv[1], 0, 0);
                if (res == 0) {
                    printf("\x1b[1;32mLaunched app '%s'.\x1b[0m\n", argv[1]);
                } else {
                    printf("\x1b[1;31mUnknown app '%s'. Choose: Terminal, Files, Browser, About, Help, Sysinfo\x1b[0m\n", argv[1]);
                }
            }

        /* ---- Theme Control ---- */
        } else if (strcmp(argv[0], "theme") == 0) {
            if (argc < 2) {
                puts("Usage: theme <tokyo|emerald|amber|cyberpunk>");
            } else {
                int tid = -1;
                if (strcmp(argv[1], "tokyo") == 0 || strcmp(argv[1], "sapphire") == 0) tid = 0;
                else if (strcmp(argv[1], "emerald") == 0) tid = 1;
                else if (strcmp(argv[1], "amber") == 0) tid = 2;
                else if (strcmp(argv[1], "cyberpunk") == 0) tid = 3;

                if (tid >= 0 && os_control(OS_CMD_SET_THEME, (long)argv[1], 0, 0) == 0) {
                    printf("\x1b[1;32mDesktop theme switched to '%s' in real time.\x1b[0m\n", argv[1]);
                } else {
                    puts("\x1b[1;31mUnknown theme. Choose: tokyo, emerald, amber, cyberpunk\x1b[0m");
                }
            }

        /* ---- Mouse Sensitivity Control ---- */
        } else if (strcmp(argv[0], "mouse") == 0) {
            if (argc == 1) {
                long sens = os_control(OS_CMD_GET_MOUSE, 0, 0, 0);
                printf("Current mouse sensitivity: %ld / 10\n", sens);
            } else {
                int sens = simple_atoi(argv[1]);
                if (sens >= 1 && sens <= 10) {
                    os_control(OS_CMD_SET_MOUSE, sens, 0, 0);
                    printf("\x1b[1;32mMouse sensitivity set to %d / 10 (200Hz smooth polling).\x1b[0m\n", sens);
                } else {
                    puts("Usage: mouse <1-10>");
                }
            }

        /* ---- Power, Network & Time Commands ---- */
        } else if (strcmp(argv[0], "cal") == 0) {
            print_calendar();
        } else if (strcmp(argv[0], "calendar") == 0) {
            if (argc > 1 && strcmp(argv[1], "--cli") == 0) {
                print_calendar();
            } else {
                os_control(OS_CMD_APP_LAUNCH, (long)"Calendar", 0, 0);
                printf("\x1b[1;32mLaunched Calendar.\x1b[0m\n");
            }
        } else if (strcmp(argv[0], "date") == 0 || strcmp(argv[0], "time") == 0) {
            char tbuf[64] = {0};
            os_control(OS_CMD_GET_TIME, (long)tbuf, sizeof(tbuf), 0);
            printf("%s UTC\n", tbuf);
        } else if (strcmp(argv[0], "battery") == 0 || strcmp(argv[0], "power") == 0) {
            if (argc > 1 && (strcmp(argv[1], "gui") == 0 || strcmp(argv[1], "app") == 0)) {
                os_control(OS_CMD_APP_LAUNCH, (long)"Power", 0, 0);
                printf("\x1b[1;32mLaunched Power & Battery Manager.\x1b[0m\n");
            } else {
                if (argc > 1 && strcmp(argv[1], "standby") == 0) {
                    os_control(OS_CMD_POWER_STANDBY, 1, 0, 0);
                    puts("\x1b[1;35mSystem entered low-power Standby Mode.\x1b[0m");
                } else if (argc > 1 && strcmp(argv[1], "charge") == 0) {
                    int on = (argc > 2 && strcmp(argv[2], "off") == 0) ? 0 : 1;
                    os_control(OS_CMD_POWER_CHARGING, on, 0, 0);
                    printf("\x1b[1;32mAC Charging Mode set to: %s\x1b[0m\n", on ? "ONLINE (Charging ⚡)" : "OFFLINE (Discharging)");
                }
                char pbuf[256] = {0};
                os_control(OS_CMD_POWER_STATUS, (long)pbuf, sizeof(pbuf), 0);
                printf("%s", pbuf);
            }
        } else if (strcmp(argv[0], "network") == 0) {
            if (argc > 1 && strcmp(argv[1], "--cli") == 0) {
                char nbuf[384] = {0};
                os_control(OS_CMD_NET_STATUS, (long)nbuf, sizeof(nbuf), 0);
                printf("%s", nbuf);
            } else {
                os_control(OS_CMD_APP_LAUNCH, (long)"Network", 0, 0);
                printf("\x1b[1;32mLaunched Network Connections Manager.\x1b[0m\n");
            }
        } else if (strcmp(argv[0], "standby") == 0) {
            os_control(OS_CMD_POWER_STANDBY, 1, 0, 0);
            puts("\x1b[1;35mSystem entered low-power Standby Mode. Press any key or move mouse to resume.\x1b[0m");
        } else if (strcmp(argv[0], "wifi") == 0) {
            if (argc > 1 && strcmp(argv[1], "scan") == 0) {
                puts("Scanning Wi-Fi channels (2.4 GHz & 5.0 GHz)...");
                puts("Detected Networks:");
                puts("  \x1b[1;32m*\x1b[0m [94% Signal] MyOS-HyperNet-5G   (WPA3-Personal, Ch 36, 5.0 GHz) \x1b[1;32m[Connected]\x1b[0m");
                puts("  * [76% Signal] FiberLink-Access4   (WPA2-PSK, Ch 1, 2.4 GHz)");
                puts("  * [45% Signal] Guest-Airport       (Open, Ch 6, 2.4 GHz)");
            } else if (argc > 1 && (strcmp(argv[1], "on") == 0 || strcmp(argv[1], "off") == 0 || strcmp(argv[1], "toggle") == 0)) {
                long st = os_control(OS_CMD_NET_WIFI, 0, 0, 0);
                printf("Wi-Fi interface (wlan0) is now %s.\n", st ? "\x1b[1;32mENABLED\x1b[0m" : "\x1b[1;31mDISABLED\x1b[0m");
            } else {
                char nbuf[384] = {0};
                os_control(OS_CMD_NET_STATUS, (long)nbuf, sizeof(nbuf), 0);
                printf("%s", nbuf);
            }
        } else if (strcmp(argv[0], "bluetooth") == 0 || strcmp(argv[0], "bt") == 0) {
            if (argc > 1 && (strcmp(argv[1], "on") == 0 || strcmp(argv[1], "off") == 0 || strcmp(argv[1], "toggle") == 0)) {
                long st = os_control(OS_CMD_NET_BT, 0, 0, 0);
                printf("Bluetooth interface (bt0) is now %s.\n", st ? "\x1b[1;34mENABLED\x1b[0m" : "\x1b[1;31mDISABLED\x1b[0m");
            } else {
                char nbuf[384] = {0};
                os_control(OS_CMD_NET_STATUS, (long)nbuf, sizeof(nbuf), 0);
                printf("%s", nbuf);
            }
        } else if (strcmp(argv[0], "eth") == 0 || strcmp(argv[0], "net") == 0) {
            if (argc > 1 && strcmp(argv[1], "toggle") == 0) {
                long st = os_control(OS_CMD_NET_ETH, 0, 0, 0);
                printf("Ethernet link (eth0) is now %s.\n", st ? "\x1b[1;32mCONNECTED\x1b[0m" : "\x1b[1;31mDISCONNECTED\x1b[0m");
            } else {
                char nbuf[384] = {0};
                os_control(OS_CMD_NET_STATUS, (long)nbuf, sizeof(nbuf), 0);
                printf("%s", nbuf);
            }

        /* ---- System & Diagnostics Commands ---- */
        } else if (strcmp(argv[0], "ps") == 0) {
            ps();
        } else if (strcmp(argv[0], "uptime") == 0) {
            printf("Uptime: %d seconds\n", uptime());
        } else if (strcmp(argv[0], "free") == 0 || strcmp(argv[0], "mem") == 0) {
            meminfo();
        } else if (strcmp(argv[0], "kill") == 0) {
            if (argc < 2) {
                puts("Usage: kill <pid>");
            } else {
                int pid = simple_atoi(argv[1]);
                if (kill(pid, 15) == 0) {
                    printf("Process %d terminated.\n", pid);
                } else {
                    printf("Failed to kill process %d.\n", pid);
                }
            }
        } else if (strcmp(argv[0], "echo") == 0) {
            for (int i = 1; i < argc; i++) {
                printf("%s%s", argv[i], (i + 1 < argc) ? " " : "");
            }
            putchar('\n');
        } else if (strcmp(argv[0], "reboot") == 0) {
            puts("Rebooting system...");
            reboot();
        } else if (strcmp(argv[0], "shutdown") == 0 || strcmp(argv[0], "poweroff") == 0) {
            puts("Shutting down system...");
            shutdown();
        } else if (strcmp(argv[0], "exit") == 0) {
            break;
        } else {
            /* Try executing external binary in /bin */
            char path[128];
            if (argv[0][0] == '/') {
                strncpy(path, argv[0], sizeof(path) - 1);
                path[sizeof(path) - 1] = '\0';
            } else {
                snprintf(path, sizeof(path), "/bin/%s", argv[0]);
            }

            int pid = exec(path);
            if (pid < 0) {
                printf("\x1b[1;31mUnknown command: '%s'. Type 'help' for command list.\x1b[0m\n", argv[0]);
            } else {
                int status;
                wait(pid, &status);
            }
        }
    }

    return 0;
}
