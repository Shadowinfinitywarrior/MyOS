#ifndef GUI_LOGIN_H
#define GUI_LOGIN_H

#include "surface.h"
#include "rect.h"
#include "input.h"

#define AUTH_MAX_USERS 16
#define AUTH_NAME_MAX  32
#define AUTH_PASS_MAX  32

typedef struct {
    char username[AUTH_NAME_MAX];
    char password[AUTH_PASS_MAX];
    bool active;
} auth_user_t;

/* Initialize authentication with default credentials (username: myos, password: myos) */
void auth_init(void);

/* Check if username/password are valid */
bool auth_validate(const char *username, const char *password);

/* Create or update user account dynamically inside the OS */
bool auth_add_user(const char *username, const char *password);

/* Change password for an existing user */
bool auth_change_password(const char *username, const char *new_password);

/* Get number of registered users and copy their names */
int auth_get_users(char names[][AUTH_NAME_MAX], int max_users);

/* Export/import full credentials for persistent storage */
int auth_export_records(auth_user_t *dest, int max);
void auth_import_record(const auth_user_t *rec);

/* Currently authenticated user */
const char *auth_get_current_user(void);
void auth_set_current_user(const char *username);

/* Login screen state and control */
bool login_is_locked(void);
void login_lock(void);
void login_unlock(void);

/* Render the login screen onto the backbuffer */
void login_render(uint32_t *bb, int stride, int scr_w, int scr_h);

/* Handle input events when locked; returns true if handled */
bool login_handle_event(const gui_event_t *ev);

#endif /* GUI_LOGIN_H */
