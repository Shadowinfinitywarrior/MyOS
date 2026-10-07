#ifndef GUI_APPS_H
#define GUI_APPS_H

#include "wm.h"

/* Simple in-kernel applications. Each opens a window with a paint callback
 * and, where relevant, a click handler. */

void app_open_files(void);      /* browses the ramfs/devfs tree */
void app_open_about(void);      /* build and system information   */
void app_open_help(void);       /* keyboard and mouse reference   */
void app_open_sysinfo(void);    /* live process/memory readout    */
void app_open_calculator(void); /* interactive arithmetic calculator */
void app_open_editor(void);     /* text editor and notepad        */
void app_open_player(void);     /* sound player & audio studio    */
void app_open_settings(void);   /* control center & system settings */

#endif
