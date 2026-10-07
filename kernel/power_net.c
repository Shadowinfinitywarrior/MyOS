#include "power_net.h"
#include "../lib/string.h"
#include "../lib/printf.h"
#include "../drivers/driver.h"

static power_state_t sys_power = {
    .is_charging = true,
    .is_standby = false,
    .battery_percent = 100,
    .voltage_mv = 12600,
    .status_str = "AC Power Online (100% Fully Charged)"
};

static net_state_t sys_net = {
    .eth_connected = true,
    .eth_ip = "10.0.2.15",
    .eth_speed = "1000 Mbps",

    .wifi_enabled = true,
    .wifi_connected = true,
    .wifi_ssid = "MyOS-HyperNet-5G",
    .wifi_signal_pct = 94,
    .wifi_dbm = -48,

    .bt_enabled = true,
    .bt_device = "AirPods Pro",
    .bt_paired_count = 2
};

void power_net_init(void) {
    driver_register("battery", "power", "ready", "ACPI Battery & Charging Management System");
    driver_register("wlan0", "network", "ready", "Intel Dual Band Wireless-AC 8265 (802.11ac)");
    driver_register("bt0", "bluetooth", "ready", "Intel Wireless Bluetooth 5.3 LE Controller");
    kprintf("[POWER/NET] Subsystems initialized: Battery Charging, WiFi wlan0, Bluetooth bt0\n");
}

void power_get_state(power_state_t *out) {
    if (!out) return;
    *out = sys_power;
}

void power_set_charging(bool charging) {
    sys_power.is_charging = charging;
    if (charging) {
        sys_power.battery_percent = 100;
        sys_power.voltage_mv = 12600;
        strcpy(sys_power.status_str, "AC Power Online (100% Fully Charged)");
    } else {
        sys_power.battery_percent = 92;
        sys_power.voltage_mv = 11850;
        strcpy(sys_power.status_str, "Discharging on Battery (92% Remaining, ~5h 12m)");
    }
}

void power_toggle_charging(void) {
    power_set_charging(!sys_power.is_charging);
}

void power_set_standby(bool standby) {
    sys_power.is_standby = standby;
}

void power_toggle_standby(void) {
    power_set_standby(!sys_power.is_standby);
}

void net_get_state(net_state_t *out) {
    if (!out) return;
    *out = sys_net;
}

void net_toggle_wifi(void) {
    sys_net.wifi_enabled = !sys_net.wifi_enabled;
    if (sys_net.wifi_enabled) {
        sys_net.wifi_connected = true;
        strcpy(sys_net.wifi_ssid, "MyOS-HyperNet-5G");
        sys_net.wifi_signal_pct = 94;
        sys_net.wifi_dbm = -48;
    } else {
        sys_net.wifi_connected = false;
        strcpy(sys_net.wifi_ssid, "Disconnected");
        sys_net.wifi_signal_pct = 0;
        sys_net.wifi_dbm = -100;
    }
}

void net_toggle_bt(void) {
    sys_net.bt_enabled = !sys_net.bt_enabled;
}

void net_toggle_eth(void) {
    sys_net.eth_connected = !sys_net.eth_connected;
}
