#ifndef _WIFI_MANAGER_H
#define _WIFI_MANAGER_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WIFI_MANAGER_MAX_NETWORKS 5
#define WIFI_MANAGER_SSID_MAX 33
#define WIFI_MANAGER_PASSWORD_MAX 65
#define WIFI_MANAGER_MAX_SCAN_RESULTS 12

typedef struct {
    char ssid[WIFI_MANAGER_SSID_MAX];
    bool preferred;
} wifi_manager_network_t;

/* Persistent network management. */
void wifi_manager_init(void);
bool wifi_manager_has_saved_network(void);
int wifi_manager_saved_count(void);
bool wifi_manager_get_saved(int index, wifi_manager_network_t *out);
bool wifi_manager_save_network(const char *ssid, const char *password,
                               bool make_preferred);
bool wifi_manager_forget_network(const char *ssid);
bool wifi_manager_set_preferred(const char *ssid);
void wifi_manager_clear_all(void);

/* On-demand station connection. No background scans or polling. */
bool wifi_manager_connect(void);
void wifi_manager_disconnect(void);
bool wifi_manager_is_connected(void);
bool wifi_manager_is_active(void);
int wifi_manager_rssi(void);
bool wifi_manager_current_ssid(char *out, size_t out_size);

/* Manual scan only; never runs in the background. */
int wifi_manager_scan(void);
int wifi_manager_scan_count(void);
bool wifi_manager_scan_ssid(int index, char *out, size_t out_size, int *rssi);

#ifdef __cplusplus
}
#endif

#endif
