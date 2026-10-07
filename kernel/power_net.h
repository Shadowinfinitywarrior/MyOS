#ifndef POWER_NET_H
#define POWER_NET_H

#include "../include/types.h"

typedef struct power_state {
    bool     is_charging;      /* true = AC power connected (charging), false = battery */
    bool     is_standby;       /* true = low power standby mode */
    uint32_t battery_percent;  /* 0 - 100 % */
    uint32_t voltage_mv;       /* e.g. 12600 mV */
    char     status_str[64];
} power_state_t;

typedef struct net_state {
    bool     eth_connected;
    char     eth_ip[32];
    char     eth_speed[16];

    bool     wifi_enabled;
    bool     wifi_connected;
    char     wifi_ssid[32];
    int      wifi_signal_pct;
    int      wifi_dbm;

    bool     bt_enabled;
    char     bt_device[32];
    int      bt_paired_count;
} net_state_t;

void power_net_init(void);
void power_get_state(power_state_t *out);
void power_set_charging(bool charging);
void power_toggle_charging(void);
void power_set_standby(bool standby);
void power_toggle_standby(void);

void net_get_state(net_state_t *out);
void net_toggle_wifi(void);
void net_toggle_bt(void);
void net_toggle_eth(void);

#endif
