#include <Arduino.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <WiFi.h>
#include <nvs.h>
#include <esp_heap_caps.h>

#include "playgroup_api.h"
#include "../knobby_net.h"

#define PG_NVS_NAMESPACE "playgroup"
#define PG_API_HOST "playgroup.gg"
#define PG_API_BASE "https://" PG_API_HOST "/api/public/v1"
#define PG_WIFI_TIMEOUT_MS 15000UL
#define PG_HTTP_TIMEOUT_MS 10000U
#define PG_TIME_TIMEOUT_MS 10000UL
#define PG_VALID_EPOCH 1700000000L

#define PG_SSID_MAX 33
#define PG_PASSWORD_MAX 65
#define PG_API_KEY_MAX 192
#define PG_SERIAL_LINE_MAX 320

static char serial_line[PG_SERIAL_LINE_MAX];
static size_t serial_line_len = 0;

static bool nvs_read_string(const char *key, char *out, size_t out_size)
{
    nvs_handle_t handle;
    size_t required = out_size;
    esp_err_t err;

    if (out == NULL || out_size == 0) return false;
    out[0] = '\0';

    if (nvs_open(PG_NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK)
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

static bool nvs_write_string(const char *key, const char *value)
{
    nvs_handle_t handle;
    esp_err_t err;

    if (value == NULL) return false;
    if (nvs_open(PG_NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK)
        return false;

    err = nvs_set_str(handle, key, value);
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);
    return err == ESP_OK;
}

static bool nvs_clear_credentials(void)
{
    nvs_handle_t handle;
    esp_err_t err;

    if (nvs_open(PG_NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK)
        return false;

    err = nvs_erase_all(handle);
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);
    return err == ESP_OK;
}

bool playgroup_credentials_ready(void)
{
    char ssid[PG_SSID_MAX];
    char api_key[PG_API_KEY_MAX];

    return nvs_read_string("ssid", ssid, sizeof(ssid)) &&
           nvs_read_string("api_key", api_key, sizeof(api_key));
}

static void wifi_power_down(void)
{
    WiFi.disconnect(true, false);
    WiFi.mode(WIFI_OFF);
}

static bool sync_clock_for_tls(void)
{
    uint32_t started;
    time_t now;

    /* Certificate validation needs a sane wall clock. A freshly booted
       ESP32 starts near the Unix epoch, which makes valid HTTPS
       certificates look "not yet valid". Keep UTC only; timezone is
       irrelevant for TLS validity checks. */
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    started = millis();

    Serial.print("[Playgroup] Syncing clock");
    do {
        time(&now);
        if (now >= PG_VALID_EPOCH) {
            Serial.println();
            Serial.println("[Playgroup] Clock synced.");
            return true;
        }
        delay(250);
        Serial.print(".");
    } while ((millis() - started) < PG_TIME_TIMEOUT_MS);

    Serial.println();
    Serial.println("[Playgroup] Clock sync failed; cannot validate HTTPS safely.");
    return false;
}


static bool wifi_connect_saved(void)
{
    char ssid[PG_SSID_MAX];
    char password[PG_PASSWORD_MAX];
    uint32_t started;

    if (!nvs_read_string("ssid", ssid, sizeof(ssid))) {
        Serial.println("[Playgroup] Wi-Fi is not configured.");
        return false;
    }

    /* Empty password is valid for an open network. */
    nvs_read_string("wifi_pass", password, sizeof(password));

    if (knobby_net_active()) {
        Serial.println("[Playgroup] Table Sync is active. Leave Table Sync before using the online API.");
        return false;
    }

    WiFi.persistent(false);
    WiFi.setAutoReconnect(false);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    Serial.print("[Playgroup] Connecting to Wi-Fi");
    started = millis();
    while (WiFi.status() != WL_CONNECTED &&
           (millis() - started) < PG_WIFI_TIMEOUT_MS) {
        delay(250);
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[Playgroup] Wi-Fi connection failed.");
        wifi_power_down();
        return false;
    }

    Serial.println("[Playgroup] Wi-Fi connected.");

    if (!sync_clock_for_tls()) {
        wifi_power_down();
        return false;
    }

    return true;
}

static bool json_extract_string(const String &json, const char *key,
                                char *out, size_t out_size)
{
    String needle;
    int key_pos;
    int colon;
    int first_quote;
    int second_quote;
    size_t copy_len;

    if (out == NULL || out_size == 0 || key == NULL) return false;
    out[0] = '\0';

    needle = String("\"") + key + "\"";
    key_pos = json.indexOf(needle);
    if (key_pos < 0) return false;

    colon = json.indexOf(':', key_pos + needle.length());
    if (colon < 0) return false;

    first_quote = json.indexOf('"', colon + 1);
    if (first_quote < 0) return false;
    second_quote = json.indexOf('"', first_quote + 1);
    if (second_quote < 0) return false;

    copy_len = (size_t)(second_quote - first_quote - 1);
    if (copy_len >= out_size) copy_len = out_size - 1;
    memcpy(out, json.c_str() + first_quote + 1, copy_len);
    out[copy_len] = '\0';
    return true;
}

static void print_heap_diagnostics(void)
{
    size_t internal_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    size_t internal_largest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    size_t psram_largest = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    Serial.print("[Playgroup] Internal heap free: ");
    Serial.print((unsigned)internal_free);
    Serial.print(" bytes; largest block: ");
    Serial.print((unsigned)internal_largest);
    Serial.println(" bytes");

    Serial.print("[Playgroup] PSRAM free: ");
    Serial.print((unsigned)psram_free);
    Serial.print(" bytes; largest block: ");
    Serial.print((unsigned)psram_largest);
    Serial.println(" bytes");
}

static bool diagnose_https_path(void)
{
    IPAddress resolved;
    WiFiClient tcp;
    NetworkClientSecure tls;
    char tls_error[160] = {0};

    Serial.print("[Playgroup] DNS ");
    Serial.print(PG_API_HOST);
    Serial.print(" ... ");
    if (!WiFi.hostByName(PG_API_HOST, resolved)) {
        Serial.println("FAILED");
        return false;
    }
    Serial.println(resolved);

    Serial.print("[Playgroup] TCP 443 ... ");
    if (!tcp.connect(resolved, 443, 5000)) {
        Serial.println("FAILED");
        return false;
    }
    Serial.println("OK");
    tcp.stop();

    print_heap_diagnostics();

    /* Diagnostic only: no HTTP request and no API key are sent here.
       This isolates raw TLS memory pressure from certificate-validation
       overhead. The real API request below still requires verified TLS. */
    Serial.print("[Playgroup] TLS diagnostic (no cert validation, no request) ... ");
    tls.setInsecure();
    tls.setHandshakeTimeout(12);
    if (!tls.connect(PG_API_HOST, 443, 12000)) {
        Serial.println("FAILED");
        int err = tls.lastError(tls_error, sizeof(tls_error));
        Serial.print("[Playgroup] TLS diagnostic error ");
        Serial.print(err);
        Serial.print(": ");
        Serial.println(tls_error[0] ? tls_error : "(no detail)");
        tls.stop();
        return false;
    }

    Serial.println("OK");
    tls.stop();
    return true;
}

static bool playgroup_test_me(void)
{
    char api_key[PG_API_KEY_MAX];
    char username[64];
    NetworkClientSecure tls;
    HTTPClient http;
    String auth;
    String response;
    int status;
    bool ok = false;

    if (!nvs_read_string("api_key", api_key, sizeof(api_key))) {
        Serial.println("[Playgroup] API key is not configured.");
        return false;
    }

    if (!wifi_connect_saved())
        return false;

    if (!diagnose_https_path()) {
        wifi_power_down();
        Serial.println("[Playgroup] Wi-Fi off.");
        return false;
    }

    /* Arduino-ESP32 3.3.12 exposes the IDF/Mozilla built-in CA bundle.
       This keeps TLS verification enabled without pinning an expiring
       leaf/intermediate certificate and without setInsecure(). */
    tls.useBuiltinCACertBundle();
    tls.setHandshakeTimeout(12);

    http.setConnectTimeout(PG_HTTP_TIMEOUT_MS);
    http.setTimeout(PG_HTTP_TIMEOUT_MS);

    if (!http.begin(tls, PG_API_BASE "/me")) {
        Serial.println("[Playgroup] Could not initialize HTTPS.");
        wifi_power_down();
        return false;
    }

    auth.reserve(strlen(api_key) + 8);
    auth = "Bearer ";
    auth += api_key;

    http.addHeader("Authorization", auth);
    http.addHeader("User-Agent", "DialDosPrimos/0.1 (ESP32-S3)");

    Serial.println("[Playgroup] GET /me ...");
    status = http.GET();

    /* Drop the in-RAM Authorization value as soon as the request is sent. */
    auth = "";

    if (status == HTTP_CODE_OK) {
        response = http.getString();
        if (json_extract_string(response, "username", username, sizeof(username))) {
            Serial.print("[Playgroup] Authenticated as ");
            Serial.println(username);
        } else {
            Serial.println("[Playgroup] Authenticated successfully (HTTP 200).");
        }
        ok = true;
    } else if (status > 0) {
        Serial.print("[Playgroup] /me returned HTTP ");
        Serial.println(status);
        if (status == HTTP_CODE_UNAUTHORIZED)
            Serial.println("[Playgroup] Check the API key.");
    } else {
        Serial.print("[Playgroup] HTTPS request failed: ");
        Serial.println(http.errorToString(status).c_str());
    }

    response = "";
    http.end();
    wifi_power_down();
    Serial.println("[Playgroup] Wi-Fi off.");
    return ok;
}

static void print_status(void)
{
    char tmp[PG_API_KEY_MAX];
    bool has_ssid = nvs_read_string("ssid", tmp, sizeof(tmp));
    bool has_key = nvs_read_string("api_key", tmp, sizeof(tmp));

    Serial.println("[Playgroup] Configuration:");
    Serial.print("  Wi-Fi: ");
    Serial.println(has_ssid ? "configured" : "missing");
    Serial.print("  API key: ");
    Serial.println(has_key ? "configured" : "missing");
    Serial.print("  Table Sync: ");
    Serial.println(knobby_net_active() ? "active" : "off");
}

static void print_help(void)
{
    Serial.println("[Playgroup] USB commands:");
    Serial.println("  PG WIFI <ssid>|<password>");
    Serial.println("  PG KEY <api-key>");
    Serial.println("  PG STATUS");
    Serial.println("  PG TEST");
    Serial.println("  PG CLEAR");
    Serial.println("  PG HELP");
    Serial.println("Secrets are stored in NVS and are never echoed back.");
}

static void handle_command(char *line)
{
    const char *wifi_prefix = "PG WIFI ";
    const char *key_prefix = "PG KEY ";

    while (*line == ' ' || *line == '\t') line++;

    if (strcmp(line, "PG HELP") == 0) {
        print_help();
        return;
    }

    if (strcmp(line, "PG STATUS") == 0) {
        print_status();
        return;
    }

    if (strcmp(line, "PG TEST") == 0) {
        playgroup_test_me();
        return;
    }

    if (strcmp(line, "PG CLEAR") == 0) {
        if (nvs_clear_credentials())
            Serial.println("[Playgroup] Stored credentials cleared.");
        else
            Serial.println("[Playgroup] Could not clear stored credentials.");
        return;
    }

    if (strncmp(line, wifi_prefix, strlen(wifi_prefix)) == 0) {
        char *value = line + strlen(wifi_prefix);
        char *separator = strchr(value, '|');
        if (separator == NULL) {
            Serial.println("[Playgroup] Usage: PG WIFI <ssid>|<password>");
            return;
        }

        *separator = '\0';
        const char *ssid = value;
        const char *password = separator + 1;

        if (ssid[0] == '\0' || strlen(ssid) >= PG_SSID_MAX ||
            strlen(password) >= PG_PASSWORD_MAX) {
            Serial.println("[Playgroup] Invalid Wi-Fi credential length.");
            return;
        }

        if (nvs_write_string("ssid", ssid) &&
            nvs_write_string("wifi_pass", password)) {
            Serial.println("[Playgroup] Wi-Fi credentials saved.");
        } else {
            Serial.println("[Playgroup] Could not save Wi-Fi credentials.");
        }
        return;
    }

    if (strncmp(line, key_prefix, strlen(key_prefix)) == 0) {
        const char *key = line + strlen(key_prefix);
        if (key[0] == '\0' || strlen(key) >= PG_API_KEY_MAX) {
            Serial.println("[Playgroup] Invalid API key length.");
            return;
        }

        if (nvs_write_string("api_key", key))
            Serial.println("[Playgroup] API key saved.");
        else
            Serial.println("[Playgroup] Could not save API key.");
        return;
    }

    if (strncmp(line, "PG", 2) == 0)
        Serial.println("[Playgroup] Unknown command. Type PG HELP.");
}

void playgroup_process_serial(void)
{
    while (Serial.available() > 0) {
        char c = (char)Serial.read();

        if (c == '\r') continue;

        if (c == '\n') {
            if (serial_line_len > 0) {
                serial_line[serial_line_len] = '\0';
                handle_command(serial_line);
                serial_line_len = 0;
            }
            continue;
        }

        if (serial_line_len + 1 >= sizeof(serial_line)) {
            serial_line_len = 0;
            Serial.println("[Playgroup] Serial command too long.");
            continue;
        }

        serial_line[serial_line_len++] = c;
    }
}
