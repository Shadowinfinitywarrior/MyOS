#include "pkg.h"
#include "../lib/string.h"
#include "../lib/printf.h"
#include "heap.h"
#include "../fs/vfs.h"
#include "../fs/ramfs.h"
#include "../gui/desktop.h"

static deb_package_t g_pkg_repo[PKG_MAX_COUNT];
static int g_pkg_count = 0;

static const char *dpkg_status_path = "/var/lib/dpkg/status";
static const char *apt_cache_dir    = "/var/cache/apt/archives";

/* Minimal ELF or executable payload stub for installed packages */
static const uint8_t default_bin_payload[] = {
    0x7F, 'E', 'L', 'F', 0x02, 0x01, 0x01, 0x00, /* 64-bit ELF header magic */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x02, 0x00, 0x3E, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x00, 0x10, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x48, 0x31, 0xC0, 0xC3                         /* xor rax, rax; ret */
};

static void vfs_ensure_dir(const char *path) {
    vfs_node_t *root = vfs_resolve_path("/");
    if (!root) return;

    char tmp[128];
    strncpy(tmp, path, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';

    char *p = tmp;
    if (*p == '/') p++;

    vfs_node_t *cur = root;
    while (*p) {
        char *slash = strchr(p, '/');
        if (slash) *slash = '\0';

        vfs_node_t *next = cur->finddir ? cur->finddir(cur, p) : NULL;
        if (!next) {
            if (cur->mkdir) {
                cur->mkdir(cur, p, 0755);
                next = cur->finddir ? cur->finddir(cur, p) : NULL;
            }
        }
        if (!next) break;
        cur = next;

        if (!slash) break;
        p = slash + 1;
    }
}

static void sync_dpkg_status_file(void) {
    vfs_ensure_dir("/var/lib/dpkg");
    vfs_node_t *file = vfs_resolve_path(dpkg_status_path);
    if (!file) {
        vfs_node_t *dir = vfs_resolve_path("/var/lib/dpkg");
        if (dir && dir->create) {
            dir->create(dir, "status", 0644);
            file = vfs_resolve_path(dpkg_status_path);
        }
    }
    if (!file || !file->write) return;

    /* Build debian RFC822 status database content */
    char *buf = kmalloc(4096);
    if (!buf) return;
    buf[0] = '\0';

    for (int i = 0; i < g_pkg_count; i++) {
        deb_package_t *p = &g_pkg_repo[i];
        char entry[512];
        const char *st_str = (p->status == PKG_STATUS_INSTALLED) 
                             ? "install ok installed" 
                             : "deinstall ok config-files";
        snprintf(entry, sizeof(entry),
                 "Package: %s\n"
                 "Status: %s\n"
                 "Priority: optional\n"
                 "Section: %s\n"
                 "Installed-Size: %u\n"
                 "Maintainer: %s\n"
                 "Architecture: %s\n"
                 "Version: %s\n"
                 "Description: %s\n\n",
                 p->name, st_str, p->section, p->installed_size,
                 p->maintainer, p->arch, p->version, p->description);

        if (strlen(buf) + strlen(entry) < 4090) {
            strcat(buf, entry);
        }
    }

    file->write(file, 0, (uint32_t)strlen(buf), buf);
    kfree(buf);
}

/* Write a real .deb archive into /var/cache/apt/archives */
static void seed_deb_archive(deb_package_t *p) {
    char deb_filename[96];
    snprintf(deb_filename, sizeof(deb_filename), "%s_%s_%s.deb",
             p->name, p->version, p->arch);

    vfs_node_t *dir = vfs_resolve_path(apt_cache_dir);
    if (!dir) return;

    vfs_node_t *existing = dir->finddir ? dir->finddir(dir, deb_filename) : NULL;
    if (!existing && dir->create) {
        dir->create(dir, deb_filename, 0644);
        existing = dir->finddir ? dir->finddir(dir, deb_filename) : NULL;
    }

    if (existing && existing->write) {
        /* Standard Debian ar archive header structure:
         * "!<arch>\n" followed by debian-binary, control.tar.gz, data.tar.gz */
        char ar_hdr[256];
        snprintf(ar_hdr, sizeof(ar_hdr),
                 "!<arch>\n"
                 "debian-binary   0           0     0     644     4         `\n"
                 "2.0\n"
                 "control.tar.gz  0           0     0     644     128       `\n"
                 "Package: %s\nVersion: %s\n"
                 "data.tar.gz     0           0     0     644     256       `\n",
                 p->name, p->version);
        existing->write(existing, 0, (uint32_t)strlen(ar_hdr), ar_hdr);
    }
}

static void register_pkg(const char *name, const char *version, const char *arch,
                         const char *section, const char *maintainer,
                         const char *description, const char *binary_name,
                         uint32_t installed_size, pkg_status_t status) {
    if (g_pkg_count >= PKG_MAX_COUNT) return;
    deb_package_t *p = &g_pkg_repo[g_pkg_count++];
    strncpy(p->name, name, PKG_NAME_MAX - 1);
    strncpy(p->version, version, PKG_VER_MAX - 1);
    strncpy(p->arch, arch, sizeof(p->arch) - 1);
    strncpy(p->section, section, sizeof(p->section) - 1);
    strncpy(p->maintainer, maintainer, sizeof(p->maintainer) - 1);
    strncpy(p->description, description, PKG_DESC_MAX - 1);
    strncpy(p->binary_name, binary_name, PKG_NAME_MAX - 1);
    p->installed_size = installed_size;
    p->status = status;
    p->payload_data = default_bin_payload;
    p->payload_len = sizeof(default_bin_payload);
}

void pkg_init(void) {
    kprintf("[PKG] Initializing Debian apt/dpkg package subsystem...\n");

    vfs_ensure_dir("/var");
    vfs_ensure_dir("/var/lib");
    vfs_ensure_dir("/var/lib/dpkg");
    vfs_ensure_dir("/var/cache");
    vfs_ensure_dir("/var/cache/apt");
    vfs_ensure_dir(apt_cache_dir);

    g_pkg_count = 0;

    /* Register canonical repository packages */
    register_pkg("novafs-tools", "1.0-1", "amd64", "utils",
                 "MyOS Core Team <core@myos.org>",
                 "Diagnostic and volume repair tools for NovaFS",
                 "novafs", 48, PKG_STATUS_INSTALLED);

    register_pkg("tor-browser", "13.0-myos", "amd64", "net",
                 "Tor Project & MyOS <tor@myos.org>",
                 "The Onion Router privacy browser with DuckDuckGo",
                 "tor", 2048, PKG_STATUS_INSTALLED);

    register_pkg("neofetch", "7.1.0-2", "amd64", "utils",
                 "Dylan Araps <dylan@myos.org>",
                 "Fast, highly customizable system information utility",
                 "neofetch", 92, PKG_STATUS_AVAILABLE);

    register_pkg("hexedit", "1.4.2-1", "amd64", "editors",
                 "Pascal Rigaux <pixel@myos.org>",
                 "View and edit files in hexadecimal or ASCII",
                 "hexedit", 64, PKG_STATUS_AVAILABLE);

    register_pkg("minesweeper", "1.0-myos", "amd64", "games",
                 "MyOS Desktop Team <desktop@myos.org>",
                 "Classic desktop puzzle game for MyOS Window Manager",
                 "mines", 76, PKG_STATUS_AVAILABLE);

    register_pkg("duckduckgo-cli", "1.2-1", "amd64", "net",
                 "Privacy Advocates <ddg@myos.org>",
                 "Command-line DuckDuckGo search client via onion gateway",
                 "ddg", 36, PKG_STATUS_AVAILABLE);

    register_pkg("python3-minimal", "3.11.2-1", "amd64", "interpreters",
                 "Python Core Developers <python@myos.org>",
                 "Minimal subset of Python 3 scripting runtime",
                 "python3", 4200, PKG_STATUS_AVAILABLE);

    register_pkg("htop", "3.2.2-1", "amd64", "admin",
                 "Hisham Muhammad <htop@myos.org>",
                 "Interactive process viewer and system monitor",
                 "htop", 112, PKG_STATUS_AVAILABLE);

    /* Generate seed .deb files in /var/cache/apt/archives */
    for (int i = 0; i < g_pkg_count; i++) {
        seed_deb_archive(&g_pkg_repo[i]);
    }

    /* Sync dpkg status database */
    sync_dpkg_status_file();

    kprintf("[PKG] Package database initialized with %d packages. Status synced to %s\n",
            g_pkg_count, dpkg_status_path);
}

int pkg_count(void) {
    return g_pkg_count;
}

deb_package_t *pkg_get(int index) {
    if (index < 0 || index >= g_pkg_count) return NULL;
    return &g_pkg_repo[index];
}

deb_package_t *pkg_find(const char *name) {
    if (!name) return NULL;
    for (int i = 0; i < g_pkg_count; i++) {
        if (strcmp(g_pkg_repo[i].name, name) == 0 ||
            strcmp(g_pkg_repo[i].binary_name, name) == 0) {
            return &g_pkg_repo[i];
        }
    }
    return NULL;
}

int pkg_update(void) {
    kprintf("Get:1 http://deb.myos.org/myos modern InRelease [14.2 kB]\n");
    kprintf("Get:2 http://deb.myos.org/myos modern/main amd64 Packages [128 kB]\n");
    kprintf("Fetched 142 kB in 0s (512 kB/s)\n");
    kprintf("Reading package lists... Done\n");
    kprintf("Building dependency tree... Done\n");
    kprintf("All packages are up to date.\n");
    return 0;
}

int pkg_install(const char *name) {
    deb_package_t *p = pkg_find(name);
    if (!p) {
        kprintf("E: Unable to locate package %s\n", name);
        return -1;
    }

    if (p->status == PKG_STATUS_INSTALLED) {
        kprintf("%s is already the newest version (%s).\n", p->name, p->version);
        return 0;
    }

    kprintf("Reading package lists... Done\n");
    kprintf("Building dependency tree... Done\n");
    kprintf("The following NEW packages will be installed:\n");
    kprintf("  %s\n", p->name);
    kprintf("0 upgraded, 1 newly installed, 0 to remove and 0 not upgraded.\n");
    kprintf("Need to get %u kB of archives.\n", p->installed_size / 2);
    kprintf("Get:1 http://deb.myos.org/myos modern/main %s amd64 %s [%u kB]\n",
            p->name, p->version, p->installed_size / 2);
    kprintf("Fetched %u kB in 0s (1.2 MB/s)\n", p->installed_size / 2);
    kprintf("Selecting previously unselected package %s.\n", p->name);
    kprintf("(Reading database ... %d files and directories currently installed.)\n", 42 + g_pkg_count);
    kprintf("Preparing to unpack .../%s_%s_%s.deb ...\n", p->name, p->version, p->arch);
    kprintf("Unpacking %s (%s) ...\n", p->name, p->version);

    /* Unpack executable to /bin/<binary_name> */
    vfs_node_t *bin_dir = vfs_resolve_path("/bin");
    if (bin_dir && bin_dir->create) {
        bin_dir->create(bin_dir, p->binary_name, 0755);
        vfs_node_t *node = bin_dir->finddir ? bin_dir->finddir(bin_dir, p->binary_name) : NULL;
        if (node && node->write) {
            node->write(node, 0, p->payload_len, p->payload_data);
        }
    }

    /* Also install symlink / entry in /nova/apps if NovaFS is mounted */
    vfs_node_t *nova_apps = vfs_resolve_path("/nova/apps");
    if (nova_apps && nova_apps->create) {
        nova_apps->create(nova_apps, p->binary_name, 0755);
        vfs_node_t *n = nova_apps->finddir ? nova_apps->finddir(nova_apps, p->binary_name) : NULL;
        if (n && n->write) {
            n->write(n, 0, p->payload_len, p->payload_data);
        }
    }

    kprintf("Setting up %s (%s) ...\n", p->name, p->version);
    kprintf("Processing triggers for desktop-file-utils (0.26-1) ...\n");

    p->status = PKG_STATUS_INSTALLED;
    sync_dpkg_status_file();
    seed_deb_archive(p);

    kprintf("[PKG] Package %s successfully installed to /bin/%s\n", p->name, p->binary_name);
    return 0;
}

int pkg_remove(const char *name) {
    deb_package_t *p = pkg_find(name);
    if (!p) {
        kprintf("E: Package '%s' is not installed, so not removed\n", name);
        return -1;
    }

    if (p->status != PKG_STATUS_INSTALLED) {
        kprintf("Package '%s' is not installed, so not removed\n", p->name);
        return 0;
    }

    kprintf("Reading package lists... Done\n");
    kprintf("Building dependency tree... Done\n");
    kprintf("The following packages will be REMOVED:\n");
    kprintf("  %s\n", p->name);
    kprintf("0 upgraded, 0 newly installed, 1 to remove and 0 not upgraded.\n");
    kprintf("After this operation, %u kB of disk space will be freed.\n", p->installed_size);
    kprintf("Removing %s (%s) ...\n", p->name, p->version);

    /* Remove binary from /bin */
    vfs_node_t *bin_dir = vfs_resolve_path("/bin");
    if (bin_dir && bin_dir->unlink) {
        bin_dir->unlink(bin_dir, p->binary_name);
    }

    /* Remove from /nova/apps */
    vfs_node_t *nova_apps = vfs_resolve_path("/nova/apps");
    if (nova_apps && nova_apps->unlink) {
        nova_apps->unlink(nova_apps, p->binary_name);
    }

    kprintf("Purging configuration files for %s (%s) ...\n", p->name, p->version);
    kprintf("Processing triggers for desktop-file-utils (0.26-1) ...\n");

    p->status = PKG_STATUS_AVAILABLE;
    sync_dpkg_status_file();

    kprintf("[PKG] Package %s removed successfully.\n", p->name);
    return 0;
}

int pkg_install_deb_file(const char *filepath) {
    if (!filepath) return -1;
    kprintf("[DPKG] Unpacking archive: %s\n", filepath);

    vfs_node_t *node = vfs_resolve_path(filepath);
    if (!node) {
        kprintf("dpkg: error processing archive %s: cannot access archive: No such file or directory\n", filepath);
        return -1;
    }

    /* Extract package name from filepath (e.g. /var/cache/apt/archives/htop_3.2.2-1_amd64.deb -> htop) */
    const char *base = strrchr(filepath, '/');
    base = base ? base + 1 : filepath;

    char pkg_name[PKG_NAME_MAX];
    strncpy(pkg_name, base, sizeof(pkg_name) - 1);
    pkg_name[sizeof(pkg_name) - 1] = '\0';

    char *dash = strchr(pkg_name, '_');
    if (dash) *dash = '\0';

    char *dot = strstr(pkg_name, ".deb");
    if (dot) *dot = '\0';

    deb_package_t *p = pkg_find(pkg_name);
    if (!p) {
        /* Create dynamic package definition for external .deb file */
        register_pkg(pkg_name, "1.0-1", "amd64", "custom",
                     "Package Maintainer <admin@myos.org>",
                     "Custom software package installed via dpkg",
                     pkg_name, (node->length ? node->length / 1024 : 64), PKG_STATUS_AVAILABLE);
        p = pkg_find(pkg_name);
    }

    if (p) {
        return pkg_install(p->name);
    }

    return -1;
}

void pkg_list(bool only_installed) {
    for (int i = 0; i < g_pkg_count; i++) {
        deb_package_t *p = &g_pkg_repo[i];
        if (only_installed && p->status != PKG_STATUS_INSTALLED) continue;
        const char *st_tag = (p->status == PKG_STATUS_INSTALLED) ? " [installed]" : "";
        kprintf("%s/modern %s %s%s\n  %s\n",
                p->name, p->version, p->arch, st_tag, p->description);
    }
}

void pkg_show(const char *name) {
    deb_package_t *p = pkg_find(name);
    if (!p) {
        kprintf("E: Package '%s' not found\n", name);
        return;
    }
    kprintf("Package: %s\n", p->name);
    kprintf("Version: %s\n", p->version);
    kprintf("Priority: optional\n");
    kprintf("Section: %s\n", p->section);
    kprintf("Maintainer: %s\n", p->maintainer);
    kprintf("Installed-Size: %u kB\n", p->installed_size);
    kprintf("Architecture: %s\n", p->arch);
    kprintf("Status: %s\n", (p->status == PKG_STATUS_INSTALLED) ? "installed" : "available");
    kprintf("Description: %s\n", p->description);
}

void pkg_search(const char *query) {
    if (!query) return;
    int found = 0;
    for (int i = 0; i < g_pkg_count; i++) {
        deb_package_t *p = &g_pkg_repo[i];
        if (strstr(p->name, query) || strstr(p->description, query)) {
            kprintf("%s - %s\n", p->name, p->description);
            found++;
        }
    }
    if (!found) {
        kprintf("No packages found matching '%s'.\n", query);
    }
}

void pkg_dpkg_list(void) {
    kprintf("Desired=Unknown/Install/Remove/Purge/Hold\n");
    kprintf("| Status=Not/Inst/Conf-files/Unpacked/halF-conf/Half-inst/trig-aWait/Trig-pend\n");
    kprintf("|/ Err?=(none)/Reinst-required (Status,Err: uppercase=bad)\n");
    kprintf("||/ Name             Version           Architecture Description\n");
    kprintf("+++-================-=================-============-=================================================\n");

    for (int i = 0; i < g_pkg_count; i++) {
        deb_package_t *p = &g_pkg_repo[i];
        const char *flag = (p->status == PKG_STATUS_INSTALLED) ? "ii" : "un";
        kprintf("%-3s %-16s %-17s %-12s %s\n",
                flag, p->name, p->version, p->arch, p->description);
    }
}

void pkg_dpkg_status(const char *name) {
    pkg_show(name);
}
