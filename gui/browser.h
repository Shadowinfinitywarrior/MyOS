#ifndef GUI_BROWSER_H
#define GUI_BROWSER_H

#include "wm.h"

/* MyOS Inbuilt Tor Browser
 * Privacy-first Onion routing browser with multi-hop circuit display,
 * DuckDuckGo Onion search, Onion bookmark navigation, and leak-proof rendering. */

void app_open_browser(void);
void app_open_browser_url(const char *url);

#endif
