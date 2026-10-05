#include <Arduino.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <WiFi.h>
#include <SPIFFS.h>
#include <nvs.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include "playgroup_api.h"
#include "../knobby_net.h"

extern "C" void knob_print_reset_diagnostics(void);

#define PG_NVS_NAMESPACE "playgroup"
#define PG_API_HOST "playgroup.gg"
#define PG_API_BASE "https://" PG_API_HOST "/api/public/v1"
#define PG_WIFI_FIRST_ATTEMPT_MS 4000UL
#define PG_WIFI_RETRY_ATTEMPT_MS 4500UL
#define PG_WIFI_RETRIES 3
#define PG_HTTP_TIMEOUT_MS 10000U
#define PG_TIME_TIMEOUT_MS 10000UL
#define PG_VALID_EPOCH 1700000000L

#define PG_SSID_MAX 33
#define PG_PASSWORD_MAX 65
#define PG_API_KEY_MAX 192
#define PG_SERIAL_LINE_MAX 320
#define PG_ART_CACHE_SLOTS 8
#define PG_ART_CACHE_MAX_BYTES (256U * 1024U)

static char serial_line[PG_SERIAL_LINE_MAX];
static size_t serial_line_len = 0;

static playgroup_summary_t cached_playgroups[PG_MAX_PLAYGROUPS];
static int cached_playgroup_count = 0;
static playgroup_member_t cached_members[PG_MAX_MEMBERS];
static int cached_member_count = 0;
static playgroup_deck_t cached_decks[PG_MAX_DECKS];
static int cached_deck_count = 0;

typedef struct {
    char scryfall_id[PG_SCRYFALL_ID_LEN];
    uint8_t *data;
    size_t size;
    uint32_t stamp;
} pg_art_cache_entry_t;

typedef struct {
    uint32_t generation;
    int count;
    char ids[PG_MAX_DECKS][PG_SCRYFALL_ID_LEN];
    char art_crop_urls[PG_MAX_DECKS][PG_IMAGE_URL_LEN];
} pg_art_prefetch_job_t;

typedef struct {
    char scryfall_id[PG_SCRYFALL_ID_LEN];
    char art_crop_url[PG_IMAGE_URL_LEN];
} pg_art_single_job_t;

static pg_art_cache_entry_t art_cache[PG_ART_CACHE_SLOTS];
static SemaphoreHandle_t art_cache_mutex = NULL;
static SemaphoreHandle_t art_http_mutex = NULL;
static volatile uint32_t art_prefetch_generation = 0;
static volatile bool art_single_prefetch_active = false;
static uint32_t art_cache_stamp = 1;

static NetworkClientSecure art_tls;
static HTTPClient art_http;
static bool art_http_configured = false;

static NetworkClientSecure api_tls;
static HTTPClient api_http;
static bool api_http_configured = false;

static void api_http_reset(void)
{
    api_http.end();
    api_tls.stop();
    api_http_configured = false;
}

static void api_http_configure_once(void)
{
    if (api_http_configured)
        return;

    api_tls.useBuiltinCACertBundle();
    api_tls.setHandshakeTimeout(12);
    api_http.setConnectTimeout(PG_HTTP_TIMEOUT_MS);
    api_http.setTimeout(PG_HTTP_TIMEOUT_MS);
    api_http.setReuse(true);
    api_http.setUserAgent("DialDosPrimos/0.1 (ESP32-S3)");
    api_http_configured = true;
}
static volatile bool art_background_prefetch_active = false;
#define PG_ART_FILE_MAGIC 0x50474131UL
#define PG_ART_FILE_VERSION 1U

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t reserved;
    uint32_t jpeg_size;
    char scryfall_id[PG_SCRYFALL_ID_LEN];
} pg_art_file_header_t;

static bool art_fs_checked = false;
static bool art_fs_ready = false;

static bool art_fs_begin_read(void)
{
    if (art_fs_checked)
        return art_fs_ready;

    art_fs_checked = true;
    art_fs_ready = SPIFFS.begin(false);

    if (art_fs_ready) {
        Serial.print("[Playgroup] Persistent art cache ready; used ");
        Serial.print((unsigned)SPIFFS.usedBytes());
        Serial.print("/");
        Serial.print((unsigned)SPIFFS.totalBytes());
        Serial.println(" bytes.");
    }

    return art_fs_ready;
}

static bool art_fs_begin_write(void)
{
    if (art_fs_ready)
        return true;

    if (!art_fs_checked) {
        art_fs_checked = true;
        art_fs_ready = SPIFFS.begin(false);
    }

    if (!art_fs_ready) {
        Serial.println("[Playgroup] Initializing persistent art cache...");
        art_fs_ready = SPIFFS.begin(true);
    }

    if (art_fs_ready) {
        Serial.print("[Playgroup] Persistent art cache writable; used ");
        Serial.print((unsigned)SPIFFS.usedBytes());
        Serial.print("/");
        Serial.print((unsigned)SPIFFS.totalBytes());
        Serial.println(" bytes.");
    }

    return art_fs_ready;
}

static uint64_t art_id_hash(const char *id)
{
    uint64_t hash = 1469598103934665603ULL;

    if (id == NULL)
        return 0;

    while (*id) {
        hash ^= (uint8_t)*id++;
        hash *= 1099511628211ULL;
    }
    return hash;
}

static void art_file_path(const char *id, char *out, size_t out_size)
{
    uint64_t hash = art_id_hash(id);
    snprintf(out, out_size, "/pga_%08lx%08lx.bin",
             (unsigned long)(hash >> 32),
             (unsigned long)(hash & 0xffffffffUL));
}

static void deck_art_cache_key(const char *art_crop_url,
                               const char *scryfall_id,
                               char *out,
                               size_t out_size)
{
    if (out == NULL || out_size == 0)
        return;

    out[0] = '\0';

    if (art_crop_url != NULL && art_crop_url[0] != '\0') {
        uint64_t hash = art_id_hash(art_crop_url);
        snprintf(out, out_size, "url_%08lx%08lx",
                 (unsigned long)(hash >> 32),
                 (unsigned long)(hash & 0xffffffffUL));
        return;
    }

    if (scryfall_id != NULL)
        strlcpy(out, scryfall_id, out_size);
}


static bool art_persistent_load(const char *id,
                                uint8_t **out_data,
                                size_t *out_size)
{
    char path[32];
    pg_art_file_header_t header;
    File file;
    uint8_t *data;
    size_t read_count;
    uint32_t started;

    if (id == NULL || id[0] == '\0' || out_data == NULL || out_size == NULL)
        return false;

    *out_data = NULL;
    *out_size = 0;

    if (!art_fs_begin_read())
        return false;

    art_file_path(id, path, sizeof(path));
    if (!SPIFFS.exists(path))
        return false;

    started = millis();
    file = SPIFFS.open(path, FILE_READ);
    if (!file)
        return false;

    if (file.read((uint8_t *)&header, sizeof(header)) != sizeof(header) ||
        header.magic != PG_ART_FILE_MAGIC ||
        header.version != PG_ART_FILE_VERSION ||
        header.jpeg_size == 0 ||
        header.jpeg_size > PG_ART_CACHE_MAX_BYTES ||
        strncmp(header.scryfall_id, id, sizeof(header.scryfall_id)) != 0 ||
        file.size() != (size_t)sizeof(header) + (size_t)header.jpeg_size) {
        file.close();
        SPIFFS.remove(path);
        return false;
    }

    data = (uint8_t *)heap_caps_malloc((size_t)header.jpeg_size,
                                      MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (data == NULL) {
        file.close();
        return false;
    }

    read_count = file.read(data, (size_t)header.jpeg_size);
    file.close();

    if (read_count != (size_t)header.jpeg_size) {
        heap_caps_free(data);
        return false;
    }

    *out_data = data;
    *out_size = read_count;

    Serial.print("[Playgroup] Commander art flash cache hit in ");
    Serial.print((unsigned long)(millis() - started));
    Serial.print(" ms: ");
    Serial.println(id);
    return true;
}

static void art_persistent_store(const char *id,
                                 const uint8_t *data,
                                 size_t size)
{
    char path[32];
    char temp_path[32];
    pg_art_file_header_t header;
    File file;
    size_t written;
    uint32_t started;

    if (id == NULL || id[0] == '\0' || data == NULL || size == 0 ||
        size > PG_ART_CACHE_MAX_BYTES || !art_fs_begin_write())
        return;

    art_file_path(id, path, sizeof(path));
    if (SPIFFS.exists(path))
        return;

    snprintf(temp_path, sizeof(temp_path), "/pgt_%08lx.tmp",
             (unsigned long)(art_id_hash(id) & 0xffffffffUL));

    memset(&header, 0, sizeof(header));
    header.magic = PG_ART_FILE_MAGIC;
    header.version = PG_ART_FILE_VERSION;
    header.jpeg_size = (uint32_t)size;
    strlcpy(header.scryfall_id, id, sizeof(header.scryfall_id));

    started = millis();
    SPIFFS.remove(temp_path);
    file = SPIFFS.open(temp_path, FILE_WRITE);
    if (!file)
        return;

    written = file.write((const uint8_t *)&header, sizeof(header));
    if (written == sizeof(header))
        written = file.write(data, size);
    else
        written = 0;

    file.close();

    if (written != size) {
        SPIFFS.remove(temp_path);
        Serial.println("[Playgroup] Commander art flash cache write failed.");
        return;
    }

    SPIFFS.remove(path);
    if (!SPIFFS.rename(temp_path, path)) {
        SPIFFS.remove(temp_path);
        return;
    }

    Serial.print("[Playgroup] Commander art saved to flash in ");
    Serial.print((unsigned long)(millis() - started));
    Serial.print(" ms: ");
    Serial.println(id);
}


static void art_http_reset(void)
{
    art_http.end();
    art_tls.stop();
    art_http_configured = false;
}

static void art_http_configure_once(void)
{
    if (art_http_configured)
        return;

    art_tls.useBuiltinCACertBundle();
    art_tls.setHandshakeTimeout(12);
    art_http.setConnectTimeout(PG_HTTP_TIMEOUT_MS);
    art_http.setTimeout(PG_HTTP_TIMEOUT_MS);
    art_http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    art_http.setReuse(true);
    art_http.setUserAgent("DialDosPrimos/0.1 (ESP32-S3)");
    art_http_configured = true;
}

static void art_cache_init(void)
{
    if (art_cache_mutex == NULL)
        art_cache_mutex = xSemaphoreCreateMutex();
    if (art_http_mutex == NULL)
        art_http_mutex = xSemaphoreCreateMutex();
}

static bool art_cache_copy(const char *id, uint8_t **out_data, size_t *out_size)
{
    bool found = false;

    art_cache_init();
    if (art_cache_mutex == NULL || id == NULL || out_data == NULL || out_size == NULL)
        return false;

    xSemaphoreTake(art_cache_mutex, portMAX_DELAY);
    for (int i = 0; i < PG_ART_CACHE_SLOTS; i++) {
        if (art_cache[i].data != NULL && strcmp(art_cache[i].scryfall_id, id) == 0) {
            uint8_t *copy = (uint8_t *)heap_caps_malloc(art_cache[i].size,
                                                       MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            if (copy != NULL) {
                memcpy(copy, art_cache[i].data, art_cache[i].size);
                *out_data = copy;
                *out_size = art_cache[i].size;
                art_cache[i].stamp = art_cache_stamp++;
                found = true;
            }
            break;
        }
    }
    xSemaphoreGive(art_cache_mutex);
    return found;
}

static void art_cache_store(const char *id, const uint8_t *data, size_t size)
{
    int slot = -1;
    uint32_t oldest = UINT32_MAX;

    if (id == NULL || data == NULL || size == 0 || size > PG_ART_CACHE_MAX_BYTES)
        return;

    art_cache_init();
    if (art_cache_mutex == NULL)
        return;

    xSemaphoreTake(art_cache_mutex, portMAX_DELAY);

    for (int i = 0; i < PG_ART_CACHE_SLOTS; i++) {
        if (art_cache[i].data != NULL && strcmp(art_cache[i].scryfall_id, id) == 0) {
            art_cache[i].stamp = art_cache_stamp++;
            xSemaphoreGive(art_cache_mutex);
            return;
        }
        if (art_cache[i].data == NULL) {
            slot = i;
            break;
        }
        if (art_cache[i].stamp < oldest) {
            oldest = art_cache[i].stamp;
            slot = i;
        }
    }

    if (slot >= 0) {
        uint8_t *copy = (uint8_t *)heap_caps_malloc(size,
                                                   MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (copy != NULL) {
            memcpy(copy, data, size);
            if (art_cache[slot].data != NULL)
                heap_caps_free(art_cache[slot].data);
            art_cache[slot].data = copy;
            art_cache[slot].size = size;
            art_cache[slot].stamp = art_cache_stamp++;
            strlcpy(art_cache[slot].scryfall_id, id, sizeof(art_cache[slot].scryfall_id));
        }
    }

    xSemaphoreGive(art_cache_mutex);
}

static void art_cache_clear(void)
{
    art_cache_init();
    if (art_cache_mutex == NULL)
        return;

    xSemaphoreTake(art_cache_mutex, portMAX_DELAY);
    for (int i = 0; i < PG_ART_CACHE_SLOTS; i++) {
        if (art_cache[i].data != NULL) {
            heap_caps_free(art_cache[i].data);
            art_cache[i].data = NULL;
        }
        art_cache[i].size = 0;
        art_cache[i].stamp = 0;
        art_cache[i].scryfall_id[0] = '\0';
    }
    xSemaphoreGive(art_cache_mutex);
}

class PsramBufferStream : public Stream {
public:
    PsramBufferStream(uint8_t *buffer, size_t capacity)
        : buffer_(buffer), capacity_(capacity), pos_(0) {}

    int available() override { return 0; }
    int read() override { return -1; }
    int peek() override { return -1; }
    void flush() override {}

    size_t write(uint8_t value) override {
        if (pos_ >= capacity_) return 0;
        buffer_[pos_++] = value;
        if (art_background_prefetch_active)
            vTaskDelay(1);
        return 1;
    }

    size_t write(const uint8_t *data, size_t size) override {
        if (data == NULL || size == 0 || pos_ >= capacity_) return 0;
        size_t room = capacity_ - pos_;
        size_t count = size < room ? size : room;
        memcpy(buffer_ + pos_, data, count);
        pos_ += count;
        if (art_background_prefetch_active)
            vTaskDelay(1);
        return count;
    }

    size_t size() const { return pos_; }

private:
    uint8_t *buffer_;
    size_t capacity_;
    size_t pos_;
};

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

    time(&now);
    if (now >= PG_VALID_EPOCH) {
        return true;
    }

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
            Serial.print("[Playgroup] Clock synced in ");
            Serial.print((unsigned long)(millis() - started));
            Serial.println(" ms.");
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

    if (WiFi.status() == WL_CONNECTED) {
        Serial.print("[Playgroup] Wi-Fi already connected; RSSI ");
        Serial.print(WiFi.RSSI());
        Serial.println(" dBm.");
        return sync_clock_for_tls();
    }

    if (!nvs_read_string("ssid", ssid, sizeof(ssid))) {
        Serial.println("[Playgroup] Wi-Fi is not configured.");
        return false;
    }

    nvs_read_string("wifi_pass", password, sizeof(password));

    if (knobby_net_active()) {
        Serial.println("[Playgroup] Table Sync is active. Leave Table Sync before using the online API.");
        return false;
    }

    WiFi.persistent(false);
    WiFi.setAutoReconnect(true);
    WiFi.mode(WIFI_STA);

    for (int attempt = 1; attempt <= PG_WIFI_RETRIES; attempt++) {
        uint32_t started = millis();
        uint32_t attempt_timeout =
            (attempt == 1) ? PG_WIFI_FIRST_ATTEMPT_MS
                           : PG_WIFI_RETRY_ATTEMPT_MS;

        Serial.print("[Playgroup] Connecting to Wi-Fi (");
        Serial.print(attempt);
        Serial.print("/");
        Serial.print(PG_WIFI_RETRIES);
        Serial.print(")");

        WiFi.begin(ssid, password);

        uint32_t last_dot = started;
        while (WiFi.status() != WL_CONNECTED &&
               (millis() - started) < attempt_timeout) {
            delay(100);
            if ((millis() - last_dot) >= 500) {
                Serial.print(".");
                last_dot = millis();
            }
        }
        Serial.println();

        if (WiFi.status() == WL_CONNECTED) {
            Serial.print("[Playgroup] Wi-Fi connected in ");
            Serial.print((unsigned long)(millis() - started));
            Serial.print(" ms; RSSI ");
            Serial.print(WiFi.RSSI());
            Serial.println(" dBm.");

            if (!sync_clock_for_tls()) {
                wifi_power_down();
                return false;
            }
            return true;
        }

        Serial.print("[Playgroup] Wi-Fi attempt ");
        Serial.print(attempt);
        Serial.println(" failed.");
        WiFi.disconnect(false, false);
        delay(250);
    }

    Serial.println("[Playgroup] Wi-Fi connection failed after retries.");
    wifi_power_down();
    return false;
}

bool playgroup_prepare_connection(void)
{
    return wifi_connect_saved();
}

bool playgroup_network_active(void)
{
    return WiFi.status() == WL_CONNECTED || WiFi.getMode() != WIFI_OFF;
}

void playgroup_end_session(void)
{
    ++art_prefetch_generation;
    art_cache_clear();
    art_http_reset();
    api_http_reset();

    if (WiFi.status() == WL_CONNECTED || WiFi.getMode() != WIFI_OFF) {
        Serial.println("[Playgroup] Ending Wi-Fi session.");
        wifi_power_down();
    }
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

static bool playgroup_https_get(const String &path, String &response, int &status)
{
    char api_key[PG_API_KEY_MAX];
    String auth;
    String url;
    uint32_t request_started;

    response = "";
    status = -1;

    if (!nvs_read_string("api_key", api_key, sizeof(api_key))) {
        Serial.println("[Playgroup] API key is not configured.");
        return false;
    }

    if (!wifi_connect_saved())
        return false;

    api_http_configure_once();

    url.reserve(strlen(PG_API_BASE) + path.length() + 1);
    url = PG_API_BASE;
    url += path;

    if (!api_http.begin(api_tls, url)) {
        Serial.println("[Playgroup] Could not initialize HTTPS.");
        api_http_reset();
        return false;
    }

    auth.reserve(strlen(api_key) + 8);
    auth = "Bearer ";
    auth += api_key;
    api_http.addHeader("Authorization", auth);
    api_http.addHeader("Connection", "keep-alive");

    Serial.print("[Playgroup] API TLS socket before GET ");
    Serial.print(path);
    Serial.print(": ");
    Serial.println(api_tls.connected() ? "reusable" : "new handshake");

    request_started = millis();
    status = api_http.GET();
    auth = "";

    if (status > 0)
        response = api_http.getString();

    Serial.print("[Playgroup] GET ");
    Serial.print(path);
    Serial.print(" -> HTTP ");
    Serial.print(status);
    Serial.print(" in ");
    Serial.print((unsigned long)(millis() - request_started));
    Serial.print(" ms; body ");
    Serial.print((unsigned)response.length());
    Serial.println(" bytes");

    api_http.end();

    if (status <= 0) {
        api_http_reset();
        return false;
    }

    return true;
}

static bool json_extract_number_token(const String &json, const char *key,
                                      char *out, size_t out_size)
{
    String needle;
    int key_pos;
    int colon;
    int pos;
    int end_pos;
    size_t copy_len;

    if (out == NULL || out_size == 0 || key == NULL) return false;
    out[0] = '\0';

    needle = String("\"") + key + "\"";
    key_pos = json.indexOf(needle);
    if (key_pos < 0) return false;

    colon = json.indexOf(':', key_pos + needle.length());
    if (colon < 0) return false;

    pos = colon + 1;
    while (pos < (int)json.length() &&
           (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\r' || json[pos] == '\n'))
        pos++;

    end_pos = pos;
    while (end_pos < (int)json.length() &&
           ((json[end_pos] >= '0' && json[end_pos] <= '9') || json[end_pos] == '-'))
        end_pos++;

    if (end_pos <= pos) return false;

    copy_len = (size_t)(end_pos - pos);
    if (copy_len >= out_size) copy_len = out_size - 1;
    memcpy(out, json.c_str() + pos, copy_len);
    out[copy_len] = '\0';
    return true;
}

static int json_collect_top_level_ids(const String &json, long *ids, int max_ids)
{
    int brace_depth = 0;
    bool in_string = false;
    bool escape = false;
    int count = 0;

    for (int i = 0; i < (int)json.length(); i++) {
        char ch = json[i];

        if (in_string) {
            if (escape) {
                escape = false;
            } else if (ch == '\\') {
                escape = true;
            } else if (ch == '"') {
                in_string = false;
            }
            continue;
        }

        if (ch == '"') {
            /* At depth 1 we are inside one playgroup object from the root
               array. Nested league objects are depth 2+, so their ids are
               deliberately ignored. */
            if (brace_depth == 1 && json.startsWith("\"id\"", i)) {
                int colon = json.indexOf(':', i + 4);
                if (colon >= 0) {
                    int pos = colon + 1;
                    while (pos < (int)json.length() &&
                           (json[pos] == ' ' || json[pos] == '\t' ||
                            json[pos] == '\r' || json[pos] == '\n'))
                        pos++;

                    long id = 0;
                    bool found_digit = false;
                    while (pos < (int)json.length() &&
                           json[pos] >= '0' && json[pos] <= '9') {
                        found_digit = true;
                        id = (id * 10L) + (json[pos] - '0');
                        pos++;
                    }

                    if (found_digit && count < max_ids)
                        ids[count++] = id;
                }
            }
            in_string = true;
        } else if (ch == '{') {
            brace_depth++;
        } else if (ch == '}') {
            if (brace_depth > 0) brace_depth--;
        }
    }

    return count;
}


static bool json_extract_string_from_object(const String &obj, const char *key,
                                            char *out, size_t out_size)
{
    return json_extract_string(obj, key, out, out_size);
}

static bool json_extract_long_from_object(const String &obj, const char *key, long *out)
{
    char token[24];
    if (out == NULL) return false;
    if (!json_extract_number_token(obj, key, token, sizeof(token))) return false;
    *out = strtol(token, NULL, 10);
    return true;
}

static bool json_extract_bool_from_object(const String &obj, const char *key, bool *out)
{
    String needle = String("\"") + key + "\"";
    int key_pos = obj.indexOf(needle);
    int colon;
    int pos;

    if (out == NULL || key_pos < 0) return false;
    colon = obj.indexOf(':', key_pos + needle.length());
    if (colon < 0) return false;
    pos = colon + 1;
    while (pos < (int)obj.length() &&
           (obj[pos] == ' ' || obj[pos] == '\t' || obj[pos] == '\r' || obj[pos] == '\n'))
        pos++;

    if (obj.startsWith("true", pos)) {
        *out = true;
        return true;
    }
    if (obj.startsWith("false", pos)) {
        *out = false;
        return true;
    }
    return false;
}

static bool json_extract_float_from_object(const String &obj, const char *key, float *out)
{
    String needle = String("\"") + key + "\"";
    int key_pos = obj.indexOf(needle);
    int colon;
    int pos;
    int end_pos;
    char token[24];

    if (out == NULL || key_pos < 0) return false;
    colon = obj.indexOf(':', key_pos + needle.length());
    if (colon < 0) return false;
    pos = colon + 1;
    while (pos < (int)obj.length() &&
           (obj[pos] == ' ' || obj[pos] == '\t' || obj[pos] == '\r' || obj[pos] == '\n'))
        pos++;

    if (obj.startsWith("null", pos)) {
        *out = 0.0f;
        return true;
    }

    end_pos = pos;
    while (end_pos < (int)obj.length() &&
           ((obj[end_pos] >= '0' && obj[end_pos] <= '9') ||
            obj[end_pos] == '-' || obj[end_pos] == '+' ||
            obj[end_pos] == '.' || obj[end_pos] == 'e' || obj[end_pos] == 'E'))
        end_pos++;

    if (end_pos <= pos || (end_pos - pos) >= (int)sizeof(token)) return false;
    memcpy(token, obj.c_str() + pos, end_pos - pos);
    token[end_pos - pos] = '\0';
    *out = strtof(token, NULL);
    return true;
}

static bool json_extract_nested_string_field(const String &obj, const char *key,
                                             const char *nested_key,
                                             char *out, size_t out_size)
{
    String needle = String("\"") + key + "\"";
    int key_pos = obj.indexOf(needle);
    int colon;
    int pos;
    int object_start;
    int depth = 0;
    bool in_string = false;
    bool escape = false;

    if (out == NULL || out_size == 0) return false;
    out[0] = '\0';
    if (key_pos < 0) return false;

    colon = obj.indexOf(':', key_pos + needle.length());
    if (colon < 0) return false;
    pos = colon + 1;
    while (pos < (int)obj.length() &&
           (obj[pos] == ' ' || obj[pos] == '\t' || obj[pos] == '\r' || obj[pos] == '\n'))
        pos++;

    if (obj.startsWith("null", pos)) return true;
    object_start = obj.indexOf('{', pos);
    if (object_start < 0) return false;

    for (int i = object_start; i < (int)obj.length(); i++) {
        char ch = obj[i];
        if (in_string) {
            if (escape) escape = false;
            else if (ch == '\\') escape = true;
            else if (ch == '"') in_string = false;
            continue;
        }
        if (ch == '"') in_string = true;
        else if (ch == '{') depth++;
        else if (ch == '}') {
            depth--;
            if (depth == 0) {
                String nested = obj.substring(object_start, i + 1);
                return json_extract_string(nested, nested_key, out, out_size);
            }
        }
    }
    return false;
}

static bool json_extract_nested_name(const String &obj, const char *key,
                                     char *out, size_t out_size)
{
    return json_extract_nested_string_field(obj, key, "name", out, out_size);
}

typedef bool (*json_object_cb_t)(const String &obj, void *ctx);

static int json_for_each_top_level_object(const String &json, json_object_cb_t cb,
                                          void *ctx, int max_objects)
{
    int count = 0;
    int depth = 0;
    int object_start = -1;
    bool in_string = false;
    bool escape = false;

    for (int i = 0; i < (int)json.length(); i++) {
        char ch = json[i];

        if (in_string) {
            if (escape) escape = false;
            else if (ch == '\\') escape = true;
            else if (ch == '"') in_string = false;
            continue;
        }

        if (ch == '"') {
            in_string = true;
        } else if (ch == '{') {
            if (depth == 0) object_start = i;
            depth++;
        } else if (ch == '}') {
            if (depth > 0) depth--;
            if (depth == 0 && object_start >= 0) {
                String obj = json.substring(object_start, i + 1);
                if (cb != NULL && !cb(obj, ctx)) break;
                count++;
                object_start = -1;
                if (count >= max_objects) break;
            }
        }
    }

    return count;
}

static bool parse_playgroup_object(const String &obj, void *ctx)
{
    (void)ctx;
    if (cached_playgroup_count >= PG_MAX_PLAYGROUPS) return false;

    playgroup_summary_t *pg = &cached_playgroups[cached_playgroup_count];
    memset(pg, 0, sizeof(*pg));

    long member_count = 0;
    if (!json_extract_long_from_object(obj, "id", &pg->id)) return true;
    json_extract_long_from_object(obj, "member_count", &member_count);
    pg->member_count = (int)member_count;
    json_extract_string_from_object(obj, "name", pg->name, sizeof(pg->name));
    cached_playgroup_count++;
    return true;
}

static bool parse_member_object(const String &obj, void *ctx)
{
    (void)ctx;
    if (cached_member_count >= PG_MAX_MEMBERS) return false;

    playgroup_member_t *member = &cached_members[cached_member_count];
    memset(member, 0, sizeof(*member));

    if (!json_extract_long_from_object(obj, "user_id", &member->user_id)) return true;
    json_extract_bool_from_object(obj, "admin", &member->admin);
    json_extract_string_from_object(obj, "username", member->username, sizeof(member->username));
    cached_member_count++;
    return true;
}

static bool parse_deck_object(const String &obj, void *ctx)
{
    (void)ctx;
    if (cached_deck_count >= PG_MAX_DECKS) return false;

    playgroup_deck_t *deck = &cached_decks[cached_deck_count];
    memset(deck, 0, sizeof(*deck));

    if (!json_extract_long_from_object(obj, "id", &deck->id)) return true;
    json_extract_long_from_object(obj, "user_id", &deck->user_id);
    json_extract_bool_from_object(obj, "archived", &deck->archived);
    json_extract_float_from_object(obj, "power_level", &deck->power_level);
    {
        long wins = 0;
        long losses = 0;
        json_extract_long_from_object(obj, "games_won", &wins);
        json_extract_long_from_object(obj, "games_lost", &losses);
        deck->games_won = (int)wins;
        deck->games_lost = (int)losses;
    }
    json_extract_string_from_object(obj, "last_game_played_at",
                                    deck->last_game_played_at,
                                    sizeof(deck->last_game_played_at));
    json_extract_string_from_object(obj, "name", deck->name, sizeof(deck->name));
    json_extract_nested_name(obj, "commander", deck->commander, sizeof(deck->commander));
    json_extract_nested_name(obj, "partner", deck->partner, sizeof(deck->partner));
    json_extract_nested_string_field(obj, "commander", "art_crop_url",
                                     deck->art_crop_url, sizeof(deck->art_crop_url));
    json_extract_nested_string_field(obj, "commander", "scryfall_id",
                                     deck->scryfall_id, sizeof(deck->scryfall_id));

    cached_deck_count++;
    return true;
}

bool playgroup_refresh_playgroups(void)
{
    String me;
    String response;
    char user_id[24];
    int status;

    cached_playgroup_count = 0;
    if (!wifi_connect_saved()) return false;

    if (!playgroup_https_get("/me", me, status) || status != HTTP_CODE_OK ||
        !json_extract_number_token(me, "id", user_id, sizeof(user_id))) {
        return false;
    }

    String path = String("/users/") + user_id + "/playgroups";
    if (!playgroup_https_get(path, response, status) || status != HTTP_CODE_OK) {
        return false;
    }

    json_for_each_top_level_object(response, parse_playgroup_object, NULL, PG_MAX_PLAYGROUPS);
    return cached_playgroup_count > 0;
}

int playgroup_cached_playgroup_count(void)
{
    return cached_playgroup_count;
}

const playgroup_summary_t *playgroup_cached_playgroup(int index)
{
    if (index < 0 || index >= cached_playgroup_count) return NULL;
    return &cached_playgroups[index];
}

bool playgroup_refresh_members(long playgroup_id)
{
    String response;
    int status;

    cached_member_count = 0;
    if (playgroup_id <= 0 || !wifi_connect_saved()) return false;

    String path = String("/playgroups/") + String(playgroup_id) + "/members";
    if (!playgroup_https_get(path, response, status) || status != HTTP_CODE_OK) {
        return false;
    }

    json_for_each_top_level_object(response, parse_member_object, NULL, PG_MAX_MEMBERS);
    return cached_member_count > 0;
}

int playgroup_cached_member_count(void)
{
    return cached_member_count;
}

const playgroup_member_t *playgroup_cached_member(int index)
{
    if (index < 0 || index >= cached_member_count) return NULL;
    return &cached_members[index];
}

static int deck_usage_compare(const playgroup_deck_t *a,
                              const playgroup_deck_t *b)
{
    int games_a = a->games_won + a->games_lost;
    int games_b = b->games_won + b->games_lost;

    if (games_a != games_b)
        return (games_a > games_b) ? -1 : 1;

    /* Playgroup timestamps are YYYY-MM-DD..., so lexical order matches
       chronological order. Null/missing timestamps stay behind played decks. */
    if (a->last_game_played_at[0] != '\0' ||
        b->last_game_played_at[0] != '\0') {
        int cmp = strcmp(a->last_game_played_at, b->last_game_played_at);
        if (cmp != 0)
            return (cmp > 0) ? -1 : 1;
    }

    /* Stable, deterministic fallback. */
    return (a->id < b->id) ? -1 : (a->id > b->id ? 1 : 0);
}

static void sort_cached_decks_by_usage(void)
{
    for (int i = 1; i < cached_deck_count; i++) {
        playgroup_deck_t key = cached_decks[i];
        int j = i - 1;

        while (j >= 0 && deck_usage_compare(&key, &cached_decks[j]) < 0) {
            cached_decks[j + 1] = cached_decks[j];
            j--;
        }
        cached_decks[j + 1] = key;
    }
}

bool playgroup_refresh_decks(long user_id)
{
    String response;
    int status;

    cached_deck_count = 0;
    if (user_id <= 0 || !wifi_connect_saved()) return false;

    String path = String("/users/") + String(user_id) + "/decks";
    if (!playgroup_https_get(path, response, status) || status != HTTP_CODE_OK) {
        return false;
    }

    json_for_each_top_level_object(response, parse_deck_object, NULL, PG_MAX_DECKS);
    sort_cached_decks_by_usage();
    return true;
}

int playgroup_cached_deck_count(void)
{
    return cached_deck_count;
}

const playgroup_deck_t *playgroup_cached_deck(int index)
{
    if (index < 0 || index >= cached_deck_count) return NULL;
    return &cached_decks[index];
}

static bool playgroup_download_image_url(const char *url_cstr,
                                         const char *log_label,
                                         uint8_t **out_data,
                                         size_t *out_size)
{
    uint8_t *data;
    int status;
    int content_length;
    size_t received;
    int stream_result;
    uint32_t request_started;
    uint32_t download_started;

    if (url_cstr == NULL || url_cstr[0] == '\0' ||
        out_data == NULL || out_size == NULL)
        return false;

    *out_data = NULL;
    *out_size = 0;

    if (!wifi_connect_saved())
        return false;

    art_http_configure_once();

    if (!art_http.begin(art_tls, url_cstr)) {
        Serial.println("[Playgroup] Commander art HTTPS init failed.");
        art_http_reset();
        return false;
    }

    art_http.addHeader("Accept", "image/jpeg,image/*;q=0.9,*/*;q=0.8");
    art_http.addHeader("Connection", "keep-alive");

    Serial.print("[Playgroup] Commander art direct source: ");
    Serial.println(log_label != NULL ? log_label : url_cstr);
    Serial.print("[Playgroup] Art TLS socket before GET: ");
    Serial.println(art_tls.connected() ? "reusable" : "new handshake");

    request_started = millis();
    status = art_http.GET();

    Serial.print("[Playgroup] Commander art -> HTTP ");
    Serial.print(status);
    Serial.print(" in ");
    Serial.print((unsigned long)(millis() - request_started));
    Serial.println(" ms");

    if (status != HTTP_CODE_OK) {
        String error_body = art_http.getString();
        if (error_body.length() > 0) {
            Serial.print("[Playgroup] Commander art error body: ");
            Serial.println(error_body);
        }
        art_http.end();
        return false;
    }

    content_length = art_http.getSize();
    if (content_length <= 0 || content_length > (512 * 1024)) {
        Serial.print("[Playgroup] Commander art invalid size: ");
        Serial.println(content_length);
        art_http.end();
        return false;
    }

    data = (uint8_t *)heap_caps_malloc((size_t)content_length,
                                      MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (data == NULL) {
        Serial.println("[Playgroup] Commander art PSRAM allocation failed.");
        art_http.end();
        return false;
    }

    Serial.print("[Playgroup] Commander art content length: ");
    Serial.print(content_length);
    Serial.println(" bytes");

    {
        NetworkClient *stream = art_http.getStreamPtr();
        uint32_t last_progress;

        received = 0;
        stream_result = 0;
        download_started = millis();
        last_progress = download_started;

        /* Read directly into the final PSRAM buffer. Avoid HTTPClient's
           writeToStream()/Print layer; the content length is known, so no
           intermediate copy or byte-at-a-time sink is necessary. */
        while (received < (size_t)content_length &&
               (millis() - last_progress) < PG_HTTP_TIMEOUT_MS) {
            int available = stream != NULL ? stream->available() : 0;

            if (available > 0) {
                size_t remaining = (size_t)content_length - received;
                size_t chunk = (size_t)available;
                if (chunk > remaining) chunk = remaining;
                if (chunk > 16384U) chunk = 16384U;

                int got = stream->read(data + received, chunk);
                if (got > 0) {
                    received += (size_t)got;
                    last_progress = millis();
                    continue;
                }
                if (got < 0)
                    break;
            }

            delay(1);
        }

        stream_result = (received == (size_t)content_length)
                            ? (int)received
                            : -1;

        Serial.print("[Playgroup] Commander art direct read result ");
        Serial.print(stream_result);
        Serial.print("; received ");
        Serial.print((unsigned)received);
        Serial.print(" bytes in ");
        Serial.print((unsigned long)(millis() - download_started));
        Serial.println(" ms");
    }

    if (received >= 4) {
        Serial.print("[Playgroup] Commander art signature: ");
        for (int i = 0; i < 4; i++) {
            if (data[i] < 16) Serial.print("0");
            Serial.print(data[i], HEX);
            if (i < 3) Serial.print(" ");
        }
        Serial.println();
    }

    art_http.end();

    if (stream_result < 0 || received != (size_t)content_length) {
        Serial.print("[Playgroup] Commander art direct body read failed: ");
        Serial.print((unsigned)received);
        Serial.print("/");
        Serial.println(content_length);
        heap_caps_free(data);
        return false;
    }

    *out_data = data;
    *out_size = received;
    return true;
}

static bool playgroup_download_image_network(const char *scryfall_id,
                                             uint8_t **out_data,
                                             size_t *out_size)
{
    String url;

    if (scryfall_id == NULL || scryfall_id[0] == '\0')
        return false;

    url = "https://api.scryfall.com/cards/";
    url += scryfall_id;
    url += "?format=image&version=art_crop";

    Serial.print("[Playgroup] Commander art via Scryfall API fallback: ");
    Serial.println(scryfall_id);

    return playgroup_download_image_url(url.c_str(), scryfall_id,
                                        out_data, out_size);
}

bool playgroup_download_deck_image(const char *art_crop_url,
                                   const char *scryfall_id,
                                   uint8_t **out_data,
                                   size_t *out_size)
{
    bool ok = false;
    char cache_key[PG_SCRYFALL_ID_LEN];

    if (out_data == NULL || out_size == NULL)
        return false;

    *out_data = NULL;
    *out_size = 0;
    deck_art_cache_key(art_crop_url, scryfall_id,
                       cache_key, sizeof(cache_key));

    if (cache_key[0] != '\0' &&
        art_cache_copy(cache_key, out_data, out_size)) {
        Serial.print("[Playgroup] Commander art cache hit: ");
        Serial.println(cache_key);
        return true;
    }

    if (cache_key[0] != '\0' &&
        art_persistent_load(cache_key, out_data, out_size)) {
        art_cache_store(cache_key, *out_data, *out_size);
        return true;
    }

    art_cache_init();
    if (art_http_mutex == NULL)
        return false;

    xSemaphoreTake(art_http_mutex, portMAX_DELAY);

    if (cache_key[0] != '\0' &&
        art_cache_copy(cache_key, out_data, out_size)) {
        xSemaphoreGive(art_http_mutex);
        return true;
    }

    if (art_crop_url != NULL && art_crop_url[0] != '\0') {
        Serial.println("[Playgroup] Trying direct Playgroup art_crop_url...");
        ok = playgroup_download_image_url(art_crop_url, "Playgroup art_crop_url",
                                          out_data, out_size);
    }

    if (!ok && scryfall_id != NULL && scryfall_id[0] != '\0') {
        Serial.println("[Playgroup] Direct art URL failed; using Scryfall API fallback.");
        ok = playgroup_download_image_network(scryfall_id, out_data, out_size);
    }

    if (ok && cache_key[0] != '\0' &&
        *out_data != NULL && *out_size > 0) {
        art_cache_store(cache_key, *out_data, *out_size);
    }

    xSemaphoreGive(art_http_mutex);
    return ok;
}

bool playgroup_cached_image_copy(const char *scryfall_id,
                                 uint8_t **out_data, size_t *out_size)
{
    if (out_data == NULL || out_size == NULL)
        return false;
    *out_data = NULL;
    *out_size = 0;
    return art_cache_copy(scryfall_id, out_data, out_size);
}

bool playgroup_cached_deck_image_copy(const char *art_crop_url,
                                      const char *scryfall_id,
                                      uint8_t **out_data,
                                      size_t *out_size)
{
    char cache_key[PG_SCRYFALL_ID_LEN];

    if (out_data == NULL || out_size == NULL)
        return false;

    *out_data = NULL;
    *out_size = 0;
    deck_art_cache_key(art_crop_url, scryfall_id,
                       cache_key, sizeof(cache_key));
    if (cache_key[0] == '\0')
        return false;

    return art_cache_copy(cache_key, out_data, out_size);
}

bool playgroup_persist_cached_image(const char *scryfall_id)
{
    uint8_t *data = NULL;
    size_t size = 0;

    if (scryfall_id == NULL || scryfall_id[0] == '\0')
        return false;

    if (!art_cache_copy(scryfall_id, &data, &size))
        return false;

    art_persistent_store(scryfall_id, data, size);
    heap_caps_free(data);
    return true;
}

bool playgroup_persist_cached_deck_image(const char *art_crop_url,
                                         const char *scryfall_id)
{
    char cache_key[PG_SCRYFALL_ID_LEN];
    uint8_t *data = NULL;
    size_t size = 0;

    deck_art_cache_key(art_crop_url, scryfall_id,
                       cache_key, sizeof(cache_key));
    if (cache_key[0] == '\0')
        return false;

    if (!art_cache_copy(cache_key, &data, &size))
        return false;

    art_persistent_store(cache_key, data, size);
    heap_caps_free(data);
    return true;
}

bool playgroup_download_image(const char *scryfall_id, uint8_t **out_data, size_t *out_size)
{
    bool ok;

    if (scryfall_id == NULL || scryfall_id[0] == '\0' ||
        out_data == NULL || out_size == NULL)
        return false;

    *out_data = NULL;
    *out_size = 0;

    if (art_cache_copy(scryfall_id, out_data, out_size)) {
        Serial.print("[Playgroup] Commander art cache hit: ");
        Serial.println(scryfall_id);
        return true;
    }

    art_cache_init();
    if (art_http_mutex == NULL)
        return false;

    xSemaphoreTake(art_http_mutex, portMAX_DELAY);

    if (art_cache_copy(scryfall_id, out_data, out_size)) {
        xSemaphoreGive(art_http_mutex);
        Serial.print("[Playgroup] Commander art cache hit after wait: ");
        Serial.println(scryfall_id);
        return true;
    }

    ok = playgroup_download_image_network(scryfall_id, out_data, out_size);
    if (ok && *out_data != NULL && *out_size > 0)
        art_cache_store(scryfall_id, *out_data, *out_size);

    xSemaphoreGive(art_http_mutex);
    return ok;
}

static void playgroup_art_prefetch_task(void *param)
{
    pg_art_prefetch_job_t *job = (pg_art_prefetch_job_t *)param;

    if (job == NULL) {
        vTaskDelete(NULL);
        return;
    }

    Serial.print("[Playgroup] Art prefetch started for ");
    Serial.print(job->count);
    Serial.println(" decks.");

    for (int i = 0; i < job->count; i++) {
        uint8_t *data = NULL;
        size_t size = 0;

        if (job->generation != art_prefetch_generation)
            break;
        if (job->ids[i][0] == '\0' && job->art_crop_urls[i][0] == '\0')
            continue;

        if (playgroup_cached_deck_image_copy(job->art_crop_urls[i],
                                             job->ids[i],
                                             &data, &size)) {
            playgroup_free_image(data);
            continue;
        }

        Serial.print("[Playgroup] Prefetch art ");
        Serial.print(i + 1);
        Serial.print("/");
        Serial.print(job->count);
        Serial.print(": ");
        Serial.println(job->ids[i]);

        art_background_prefetch_active = true;
        if (playgroup_download_deck_image(job->art_crop_urls[i],
                                          job->ids[i],
                                          &data, &size))
            playgroup_free_image(data);
        art_background_prefetch_active = false;

        /* Prefetch is opportunistic; never compete with UI/system tasks. */
        vTaskDelay(pdMS_TO_TICKS(25));
    }

    if (job->generation == art_prefetch_generation)
        Serial.println("[Playgroup] Art prefetch finished.");

    heap_caps_free(job);
    vTaskDelete(NULL);
}

static void playgroup_single_art_prefetch_task(void *param)
{
    pg_art_single_job_t *job = (pg_art_single_job_t *)param;
    uint8_t *data = NULL;
    size_t size = 0;

    if (job != NULL) {
        Serial.print("[Playgroup] Async commander art start: ");
        Serial.println(job->scryfall_id);

        if (playgroup_download_deck_image(job->art_crop_url,
                                          job->scryfall_id,
                                          &data, &size)) {
            playgroup_free_image(data);
            Serial.print("[Playgroup] Async commander art ready: ");
            Serial.println(job->scryfall_id);
        } else {
            Serial.print("[Playgroup] Async commander art failed: ");
            Serial.println(job->scryfall_id);
        }

        heap_caps_free(job);
    }

    art_single_prefetch_active = false;
    vTaskDelete(NULL);
}

bool playgroup_prefetch_deck_image_async(const char *art_crop_url,
                                         const char *scryfall_id)
{
    uint8_t *cached = NULL;
    size_t cached_size = 0;
    pg_art_single_job_t *job;

    if ((scryfall_id == NULL || scryfall_id[0] == '\0') &&
        (art_crop_url == NULL || art_crop_url[0] == '\0'))
        return false;

    if (playgroup_cached_deck_image_copy(art_crop_url, scryfall_id,
                                         &cached, &cached_size)) {
        playgroup_free_image(cached);
        return true;
    }

    if (art_single_prefetch_active)
        return false;

    job = (pg_art_single_job_t *)heap_caps_calloc(
        1, sizeof(*job), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (job == NULL)
        return false;

    strlcpy(job->scryfall_id, scryfall_id, sizeof(job->scryfall_id));
    if (art_crop_url != NULL)
        strlcpy(job->art_crop_url, art_crop_url, sizeof(job->art_crop_url));

    art_single_prefetch_active = true;
    if (xTaskCreatePinnedToCore(playgroup_single_art_prefetch_task,
                                "pg_art_one",
                                7168, job, 0, NULL, 1) != pdPASS) {
        art_single_prefetch_active = false;
        heap_caps_free(job);
        return false;
    }

    return true;
}

void playgroup_prefetch_deck_images(void)
{
    pg_art_prefetch_job_t *job;
    uint32_t generation = ++art_prefetch_generation;

    if (cached_deck_count <= 0)
        return;

    job = (pg_art_prefetch_job_t *)heap_caps_calloc(1, sizeof(*job),
                                                    MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (job == NULL) {
        Serial.println("[Playgroup] Could not allocate art prefetch job.");
        return;
    }

    job->generation = generation;
    job->count = cached_deck_count;
    if (job->count > PG_ART_CACHE_SLOTS)
        job->count = PG_ART_CACHE_SLOTS;

    for (int i = 0; i < job->count; i++) {
        strlcpy(job->ids[i], cached_decks[i].scryfall_id,
                sizeof(job->ids[i]));
        strlcpy(job->art_crop_urls[i], cached_decks[i].art_crop_url,
                sizeof(job->art_crop_urls[i]));
    }

    /* Keep HTTPS prefetch off CPU0. Wi-Fi/system work and IDLE0 are
       watchdog-sensitive there on this board. CPU1 is also where the
       previously stable foreground HTTPS path ran; priority 0 keeps UI/game
       work ahead of opportunistic art downloads. */
    if (xTaskCreatePinnedToCore(playgroup_art_prefetch_task, "pg_art_prefetch",
                                6144, job, 0, NULL, 1) != pdPASS) {
        Serial.println("[Playgroup] Could not start art prefetch task.");
        heap_caps_free(job);
    }
}

void playgroup_free_image(uint8_t *data)
{
    if (data != NULL)
        heap_caps_free(data);
}

static bool playgroup_discover_my_decks(void)
{
    String me;
    String decks;
    char user_id[24];
    int status;

    if (!wifi_connect_saved())
        return false;

    Serial.println("[Playgroup] GET /me ...");
    if (!playgroup_https_get("/me", me, status) || status != HTTP_CODE_OK) {
        Serial.print("[Playgroup] /me failed, HTTP ");
        Serial.println(status);
        wifi_power_down();
        Serial.println("[Playgroup] Wi-Fi off.");
        return false;
    }

    if (!json_extract_number_token(me, "id", user_id, sizeof(user_id))) {
        Serial.println("[Playgroup] Could not read user id from /me response.");
        wifi_power_down();
        Serial.println("[Playgroup] Wi-Fi off.");
        return false;
    }

    String path = String("/users/") + user_id + "/decks";
    Serial.print("[Playgroup] GET decks for user ");
    Serial.print(user_id);
    Serial.println(" ...");

    if (!playgroup_https_get(path, decks, status)) {
        Serial.println("[Playgroup] Decks request failed.");
        wifi_power_down();
        Serial.println("[Playgroup] Wi-Fi off.");
        return false;
    }

    Serial.print("[Playgroup] Decks HTTP ");
    Serial.println(status);
    if (decks.length() > 0) {
        Serial.println("[Playgroup] Decks response:");
        Serial.println(decks);
    }

    wifi_power_down();
    Serial.println("[Playgroup] Wi-Fi off.");
    return status == HTTP_CODE_OK;
}

static bool playgroup_discover(void)
{
    String me;
    String playgroups;
    char user_id[24];
    int status;

    if (!wifi_connect_saved())
        return false;

    Serial.println("[Playgroup] GET /me ...");
    if (!playgroup_https_get("/me", me, status)) {
        Serial.println("[Playgroup] /me request failed.");
        wifi_power_down();
        Serial.println("[Playgroup] Wi-Fi off.");
        return false;
    }

    if (status != HTTP_CODE_OK) {
        Serial.print("[Playgroup] /me returned HTTP ");
        Serial.println(status);
        wifi_power_down();
        Serial.println("[Playgroup] Wi-Fi off.");
        return false;
    }

    if (!json_extract_number_token(me, "id", user_id, sizeof(user_id))) {
        Serial.println("[Playgroup] Could not read user id from /me response.");
        Serial.println("[Playgroup] /me body:");
        Serial.println(me);
        wifi_power_down();
        Serial.println("[Playgroup] Wi-Fi off.");
        return false;
    }

    Serial.print("[Playgroup] User id: ");
    Serial.println(user_id);

    String path = String("/users/") + user_id + "/playgroups";
    Serial.println("[Playgroup] GET user playgroups ...");
    if (!playgroup_https_get(path, playgroups, status)) {
        Serial.println("[Playgroup] Playgroups request failed.");
        wifi_power_down();
        Serial.println("[Playgroup] Wi-Fi off.");
        return false;
    }

    Serial.print("[Playgroup] Playgroups HTTP ");
    Serial.println(status);
    if (playgroups.length() > 0) {
        Serial.println("[Playgroup] Playgroups response:");
        Serial.println(playgroups);
    }

    if (status == HTTP_CODE_OK) {
        long playgroup_ids[12];
        int playgroup_count =
            json_collect_top_level_ids(playgroups, playgroup_ids,
                                       (int)(sizeof(playgroup_ids) / sizeof(playgroup_ids[0])));

        Serial.print("[Playgroup] Found ");
        Serial.print(playgroup_count);
        Serial.println(" playgroup id(s).");

        for (int i = 0; i < playgroup_count; i++) {
            String members;
            String members_path = String("/playgroups/") +
                                  String(playgroup_ids[i]) + "/members";

            Serial.print("[Playgroup] GET members for playgroup ");
            Serial.print(playgroup_ids[i]);
            Serial.println(" ...");

            if (!playgroup_https_get(members_path, members, status)) {
                Serial.println("[Playgroup] Members request failed.");
                continue;
            }

            Serial.print("[Playgroup] Members HTTP ");
            Serial.println(status);
            if (members.length() > 0) {
                Serial.print("[Playgroup] Members response for ");
                Serial.print(playgroup_ids[i]);
                Serial.println(":");
                Serial.println(members);
            }
        }
    }

    wifi_power_down();
    Serial.println("[Playgroup] Wi-Fi off.");
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

    /* Arduino-ESP32 3.3.12 exposes the IDF/Mozilla built-in CA bundle.
       This keeps TLS verification enabled without pinning an expiring
       leaf/intermediate certificate and without setInsecure(). */
    tls.useBuiltinCACertBundle();
    tls.setHandshakeTimeout(12);

    http.setConnectTimeout(PG_HTTP_TIMEOUT_MS);
    http.setTimeout(PG_HTTP_TIMEOUT_MS);

    if (!http.begin(tls, PG_API_BASE "/me")) {
        Serial.println("[Playgroup] Could not initialize HTTPS.");
        return false;
    }

    auth.reserve(strlen(api_key) + 8);
    auth = "Bearer ";
    auth += api_key;

    http.addHeader("Authorization", auth);
    http.setUserAgent("DialDosPrimos/0.1 (ESP32-S3)");

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
    Serial.println("  PG RESET");
    Serial.println("  PG TEST");
    Serial.println("  PG DISCOVER");
    Serial.println("  PG MYDECKS");
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

    if (strcmp(line, "PG RESET") == 0) {
        knob_print_reset_diagnostics();
        return;
    }

    if (strcmp(line, "PG TEST") == 0) {
        playgroup_test_me();
        return;
    }

    if (strcmp(line, "PG DISCOVER") == 0) {
        playgroup_discover();
        return;
    }

    if (strcmp(line, "PG MYDECKS") == 0) {
        playgroup_discover_my_decks();
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
