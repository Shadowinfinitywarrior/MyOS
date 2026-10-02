#ifndef DRIVER_H
#define DRIVER_H

#include "../include/types.h"
#include "../include/myosinfo.h"

/*
 * Driver registry.
 *
 * Every driver announces itself here once, with a one-line plain-English
 * description and a status. The "drivers" shell command then just prints the
 * table, so adding a driver automatically adds it to that listing.
 *
 * Status values used in this kernel:
 *   "ready"   - initialised and usable
 *   "absent"  - no such hardware on this machine (safe, not an error)
 *   "stub"    - API exists but the hardware path is not implemented yet
 */

#define DRIVER_MAX 64

/* Add one entry. Extra entries past DRIVER_MAX are ignored. */
void driver_register(const char *name, const char *category,
                     const char *status, const char *description);

/* How many drivers are registered, and how to read them back. */
int  driver_count(void);
const myos_driver_info_t *driver_get(int index);

/* Register the always-present core drivers (screen, keyboard, ...). Called
 * once during boot, before the individual device probes register theirs. */
void driver_seed_core(void);

#endif
