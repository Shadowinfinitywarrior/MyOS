#ifndef PKG_H
#define PKG_H

#include "../include/types.h"
#include "../fs/vfs.h"

#define PKG_NAME_MAX    32
#define PKG_VER_MAX     16
#define PKG_DESC_MAX    128
#define PKG_MAX_COUNT   32

typedef enum {
    PKG_STATUS_AVAILABLE = 0,
    PKG_STATUS_INSTALLED = 1,
    PKG_STATUS_UNPACKED  = 2
} pkg_status_t;

typedef struct {
    char name[PKG_NAME_MAX];
    char version[PKG_VER_MAX];
    char arch[16];
    char section[24];
    char maintainer[48];
    char description[PKG_DESC_MAX];
    char binary_name[PKG_NAME_MAX];
    uint32_t installed_size;
    pkg_status_t status;
    const uint8_t *payload_data;
    uint32_t payload_len;
} deb_package_t;

void pkg_init(void);
int  pkg_update(void);
int  pkg_install(const char *name);
int  pkg_remove(const char *name);
int  pkg_install_deb_file(const char *filepath);
void pkg_list(bool only_installed);
void pkg_show(const char *name);
void pkg_search(const char *query);
void pkg_dpkg_list(void);
void pkg_dpkg_status(const char *name);
int  pkg_count(void);
deb_package_t *pkg_get(int index);
deb_package_t *pkg_find(const char *name);

#endif /* PKG_H */
