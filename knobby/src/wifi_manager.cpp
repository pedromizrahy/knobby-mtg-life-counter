#include <Arduino.h>
#include <WiFi.h>
#include <nvs.h>
#include <string.h>

#include "wifi_manager.h"
#include "../knobby_net.h"

#define WIFI_NVS_NAMESPACE "wifi_mgr"
#define LEGACY_PG_NAMESPACE "playgroup"
#define WIFI_FIRST_TIMEOUT_MS 4000UL
#define WIFI_FALLBACK_TIMEOUT_MS 3000UL

typedef struct {
    char ssid[WIFI_MANAGER_SSID_MAX];
    char password[WIFI_MANAGER_PASSWORD_MAX];
} wifi_saved_entry_t;

static bool initialized = false;

typedef struct {
    char ssid[WIFI_MANAGER_SSID_MAX];
    int rssi;
} wifi_scan_entry_t;

static wifi_scan_entry_t scan_results[WIFI_MANAGER_MAX_SCAN_RESULTS];
static int scan_result_count = 0;

static bool read_string_ns(const char *ns, const char *key,
                           char *out, size_t out_size)
{
    nvs_handle_t handle;
    size_t required = out_size;
    esp_err_t err;

    if (out == NULL || out_size == 0)
        return false;
    out[0] = '\0';

    if (nvs_open(ns, NVS_READONLY, &handle) != ESP_OK)
        return false;

    err = nvs_get_str(handle, key, out, &required);
    nvs_close(handle);
    if (err != ESP_OK) {
        out[0] = '\0';
        return false;
    }

    out[out_size - 1] = '\0';
    return out[0] != '\0';
}

static bool write_string(const char *key, const char *value)
{
    nvs_handle_t handle;
    esp_err_t err;

    if (value == NULL)
        return false;
    if (nvs_open(WIFI_NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK)
        return false;

    err = nvs_set_str(handle, key, value);
    if (err == ESP_OK)
        err = nvs_commit(handle);
    nvs_close(handle);
    return err == ESP_OK;
}

static bool erase_key(const char *key)
{
    nvs_handle_t handle;
    esp_err_t err;

    if (nvs_open(WIFI_NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK)
        return false;

    err = nvs_erase_key(handle, key);
    if (err == ESP_ERR_NVS_NOT_FOUND)
        err = ESP_OK;
    if (err == ESP_OK)
        err = nvs_commit(handle);
    nvs_close(handle);
    return err == ESP_OK;
}

static void entry_keys(int index, char *ssid_key, size_t ssid_size,
                       char *pass_key, size_t pass_size)
{
    snprintf(ssid_key, ssid_size, "n%d_ssid", index);
    snprintf(pass_key, pass_size, "n%d_pass", index);
}

static bool load_entry(int index, wifi_saved_entry_t *out)
{
    char ssid_key[16];
    char pass_key[16];

    if (out == NULL || index < 0 || index >= WIFI_MANAGER_MAX_NETWORKS)
        return false;

    memset(out, 0, sizeof(*out));
    entry_keys(index, ssid_key, sizeof(ssid_key), pass_key, sizeof(pass_key));

    if (!read_string_ns(WIFI_NVS_NAMESPACE, ssid_key,
                        out->ssid, sizeof(out->ssid)))
        return false;

    read_string_ns(WIFI_NVS_NAMESPACE, pass_key,
                   out->password, sizeof(out->password));
    return true;
}

static bool save_entry(int index, const wifi_saved_entry_t *entry)
{
    char ssid_key[16];
    char pass_key[16];

    if (entry == NULL || index < 0 || index >= WIFI_MANAGER_MAX_NETWORKS)
        return false;

    entry_keys(index, ssid_key, sizeof(ssid_key), pass_key, sizeof(pass_key));
    return write_string(ssid_key, entry->ssid) &&
           write_string(pass_key, entry->password);
}

static void clear_entry(int index)
{
    char ssid_key[16];
    char pass_key[16];

    entry_keys(index, ssid_key, sizeof(ssid_key), pass_key, sizeof(pass_key));
    erase_key(ssid_key);
    erase_key(pass_key);
}

static int find_entry(const char *ssid)
{
    wifi_saved_entry_t entry;

    if (ssid == NULL || ssid[0] == '\0')
        return -1;

    for (int i = 0; i < WIFI_MANAGER_MAX_NETWORKS; i++) {
        if (load_entry(i, &entry) && strcmp(entry.ssid, ssid) == 0)
            return i;
    }
    return -1;
}

static int first_free_slot(void)
{
    wifi_saved_entry_t entry;

    for (int i = 0; i < WIFI_MANAGER_MAX_NETWORKS; i++) {
        if (!load_entry(i, &entry))
            return i;
    }
    return -1;
}

static void migrate_legacy_credentials(void)
{
    char ssid[WIFI_MANAGER_SSID_MAX];
    char password[WIFI_MANAGER_PASSWORD_MAX];

    if (wifi_manager_has_saved_network())
        return;

    if (!read_string_ns(LEGACY_PG_NAMESPACE, "ssid", ssid, sizeof(ssid)))
        return;

    password[0] = '\0';
    read_string_ns(LEGACY_PG_NAMESPACE, "wifi_pass",
                   password, sizeof(password));

    if (wifi_manager_save_network(ssid, password, true)) {
        Serial.print("[WiFi] Migrated legacy network: ");
        Serial.println(ssid);
    }
}

void wifi_manager_init(void)
{
    if (initialized)
        return;
    initialized = true;
    migrate_legacy_credentials();
}

bool wifi_manager_has_saved_network(void)
{
    wifi_saved_entry_t entry;

    for (int i = 0; i < WIFI_MANAGER_MAX_NETWORKS; i++) {
        if (load_entry(i, &entry))
            return true;
    }
    return false;
}

int wifi_manager_saved_count(void)
{
    wifi_saved_entry_t entry;
    int count = 0;

    wifi_manager_init();
    for (int i = 0; i < WIFI_MANAGER_MAX_NETWORKS; i++) {
        if (load_entry(i, &entry))
            count++;
    }
    return count;
}

bool wifi_manager_get_saved(int index, wifi_manager_network_t *out)
{
    wifi_saved_entry_t entry;
    char preferred[WIFI_MANAGER_SSID_MAX];
    int seen = 0;

    if (out == NULL || index < 0)
        return false;

    wifi_manager_init();
    preferred[0] = '\0';
    read_string_ns(WIFI_NVS_NAMESPACE, "preferred",
                   preferred, sizeof(preferred));

    for (int i = 0; i < WIFI_MANAGER_MAX_NETWORKS; i++) {
        if (!load_entry(i, &entry))
            continue;
        if (seen++ != index)
            continue;

        memset(out, 0, sizeof(*out));
        strlcpy(out->ssid, entry.ssid, sizeof(out->ssid));
        out->preferred = preferred[0] != '\0' &&
                         strcmp(preferred, entry.ssid) == 0;
        return true;
    }
    return false;
}

bool wifi_manager_save_network(const char *ssid, const char *password,
                               bool make_preferred)
{
    wifi_saved_entry_t entry;
    int slot;

    if (ssid == NULL || ssid[0] == '\0' ||
        strlen(ssid) >= WIFI_MANAGER_SSID_MAX ||
        password == NULL || strlen(password) >= WIFI_MANAGER_PASSWORD_MAX)
        return false;

    wifi_manager_init();
    slot = find_entry(ssid);
    if (slot < 0)
        slot = first_free_slot();
    if (slot < 0)
        return false;

    memset(&entry, 0, sizeof(entry));
    strlcpy(entry.ssid, ssid, sizeof(entry.ssid));
    strlcpy(entry.password, password, sizeof(entry.password));

    if (!save_entry(slot, &entry))
        return false;

    if (make_preferred)
        return write_string("preferred", ssid);

    /* First saved network becomes preferred automatically. */
    {
        char preferred[WIFI_MANAGER_SSID_MAX];
        if (!read_string_ns(WIFI_NVS_NAMESPACE, "preferred",
                            preferred, sizeof(preferred)))
            return write_string("preferred", ssid);
    }

    return true;
}

bool wifi_manager_forget_network(const char *ssid)
{
    char preferred[WIFI_MANAGER_SSID_MAX];
    int slot;

    wifi_manager_init();
    slot = find_entry(ssid);
    if (slot < 0)
        return false;

    preferred[0] = '\0';
    read_string_ns(WIFI_NVS_NAMESPACE, "preferred",
                   preferred, sizeof(preferred));
    clear_entry(slot);

    if (preferred[0] != '\0' && strcmp(preferred, ssid) == 0) {
        erase_key("preferred");
        wifi_manager_network_t first;
        if (wifi_manager_get_saved(0, &first))
            write_string("preferred", first.ssid);
    }
    return true;
}

bool wifi_manager_set_preferred(const char *ssid)
{
    wifi_manager_init();
    if (find_entry(ssid) < 0)
        return false;
    return write_string("preferred", ssid);
}

void wifi_manager_clear_all(void)
{
    nvs_handle_t handle;

    if (nvs_open(WIFI_NVS_NAMESPACE, NVS_READWRITE, &handle) == ESP_OK) {
        nvs_erase_all(handle);
        nvs_commit(handle);
        nvs_close(handle);
    }
}

static bool try_entry(const wifi_saved_entry_t *entry,
                      uint32_t timeout_ms, int attempt, int total)
{
    uint32_t started = millis();
    uint32_t last_dot = started;

    Serial.print("[WiFi] Connecting ");
    Serial.print(entry->ssid);
    Serial.print(" (");
    Serial.print(attempt);
    Serial.print("/");
    Serial.print(total);
    Serial.print(")");

    WiFi.disconnect(false, false);
    WiFi.begin(entry->ssid, entry->password);

    while (WiFi.status() != WL_CONNECTED &&
           (millis() - started) < timeout_ms) {
        delay(100);
        if ((millis() - last_dot) >= 500) {
            Serial.print(".");
            last_dot = millis();
        }
    }
    Serial.println();

    if (WiFi.status() != WL_CONNECTED)
        return false;

    Serial.print("[WiFi] Connected in ");
    Serial.print((unsigned long)(millis() - started));
    Serial.print(" ms; RSSI ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm.");
    return true;
}

bool wifi_manager_connect(void)
{
    wifi_saved_entry_t ordered[WIFI_MANAGER_MAX_NETWORKS];
    wifi_saved_entry_t entry;
    char preferred[WIFI_MANAGER_SSID_MAX];
    int count = 0;
    int preferred_slot = -1;

    wifi_manager_init();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.print("[WiFi] Already connected to ");
        Serial.print(WiFi.SSID());
        Serial.print("; RSSI ");
        Serial.print(WiFi.RSSI());
        Serial.println(" dBm.");
        return true;
    }

    if (knobby_net_active()) {
        Serial.println("[WiFi] Table Sync is active; internet connection skipped.");
        return false;
    }

    preferred[0] = '\0';
    read_string_ns(WIFI_NVS_NAMESPACE, "preferred",
                   preferred, sizeof(preferred));

    for (int i = 0; i < WIFI_MANAGER_MAX_NETWORKS; i++) {
        if (!load_entry(i, &entry))
            continue;
        if (preferred[0] != '\0' && strcmp(entry.ssid, preferred) == 0) {
            preferred_slot = count;
        }
        ordered[count++] = entry;
    }

    if (count <= 0) {
        Serial.println("[WiFi] No saved networks.");
        return false;
    }

    if (preferred_slot > 0) {
        entry = ordered[0];
        ordered[0] = ordered[preferred_slot];
        ordered[preferred_slot] = entry;
    }

    WiFi.persistent(false);
    WiFi.setAutoReconnect(true);
    WiFi.mode(WIFI_STA);

    for (int i = 0; i < count; i++) {
        uint32_t timeout_ms = (i == 0)
                            ? WIFI_FIRST_TIMEOUT_MS
                            : WIFI_FALLBACK_TIMEOUT_MS;
        if (try_entry(&ordered[i], timeout_ms, i + 1, count))
            return true;
        Serial.print("[WiFi] Network failed: ");
        Serial.println(ordered[i].ssid);
    }

    Serial.println("[WiFi] No saved network connected.");
    wifi_manager_disconnect();
    return false;
}

void wifi_manager_disconnect(void)
{
    WiFi.disconnect(true, false);
    WiFi.mode(WIFI_OFF);
}

bool wifi_manager_is_connected(void)
{
    return WiFi.status() == WL_CONNECTED;
}

bool wifi_manager_is_active(void)
{
    return WiFi.status() == WL_CONNECTED || WiFi.getMode() != WIFI_OFF;
}

int wifi_manager_rssi(void)
{
    return WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0;
}

bool wifi_manager_current_ssid(char *out, size_t out_size)
{
    if (out == NULL || out_size == 0)
        return false;
    out[0] = '\0';

    if (WiFi.status() != WL_CONNECTED)
        return false;

    strlcpy(out, WiFi.SSID().c_str(), out_size);
    return out[0] != '\0';
}


int wifi_manager_scan(void)
{
    int found;

    wifi_manager_init();
    if (knobby_net_active()) {
        Serial.println("[WiFi] Scan skipped while Table Sync is active.");
        return -1;
    }

    if (WiFi.getMode() == WIFI_OFF)
        WiFi.mode(WIFI_STA);

    Serial.println("[WiFi] Manual scan started.");
    found = WiFi.scanNetworks(false, true);
    scan_result_count = 0;

    if (found <= 0) {
        WiFi.scanDelete();
        Serial.println("[WiFi] Manual scan found no networks.");
        return 0;
    }

    for (int i = 0; i < found && scan_result_count < WIFI_MANAGER_MAX_SCAN_RESULTS; i++) {
        String ssid = WiFi.SSID(i);
        bool duplicate = false;

        if (ssid.length() == 0 || ssid.length() >= WIFI_MANAGER_SSID_MAX)
            continue;

        for (int j = 0; j < scan_result_count; j++) {
            if (strcmp(scan_results[j].ssid, ssid.c_str()) == 0) {
                duplicate = true;
                if (WiFi.RSSI(i) > scan_results[j].rssi)
                    scan_results[j].rssi = WiFi.RSSI(i);
                break;
            }
        }
        if (duplicate)
            continue;

        strlcpy(scan_results[scan_result_count].ssid, ssid.c_str(),
                sizeof(scan_results[scan_result_count].ssid));
        scan_results[scan_result_count].rssi = WiFi.RSSI(i);
        scan_result_count++;
    }

    WiFi.scanDelete();

    /* Strongest networks first for the small on-device picker. */
    for (int i = 0; i < scan_result_count - 1; i++) {
        for (int j = i + 1; j < scan_result_count; j++) {
            if (scan_results[j].rssi > scan_results[i].rssi) {
                wifi_scan_entry_t tmp = scan_results[i];
                scan_results[i] = scan_results[j];
                scan_results[j] = tmp;
            }
        }
    }

    Serial.print("[WiFi] Manual scan found ");
    Serial.print(scan_result_count);
    Serial.println(" network(s).");
    return scan_result_count;
}

int wifi_manager_scan_count(void)
{
    return scan_result_count;
}

bool wifi_manager_scan_ssid(int index, char *out, size_t out_size, int *rssi)
{
    if (out == NULL || out_size == 0 ||
        index < 0 || index >= scan_result_count)
        return false;

    strlcpy(out, scan_results[index].ssid, out_size);
    if (rssi != NULL)
        *rssi = scan_results[index].rssi;
    return true;
}
