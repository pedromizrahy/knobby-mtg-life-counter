#include <Arduino.h>
#include <SPIFFS.h>
#include <esp_random.h>
#include <esp_heap_caps.h>
#include <time.h>
#include <stdarg.h>

#include "playgroup_api.h"

#include "playgroup_pending.h"
#include "damage_log.h"
#include "game.h"
#include "storage.h"

#define PG_PENDING_MAGIC 0x50475131UL
#define PG_PENDING_VERSION 2U
#define PG_PENDING_PATH_PREFIX "/pgq_"

typedef struct {
    long user_id;
    long deck_id;
    long commander_id;
    uint8_t mulligans;
    uint8_t eliminated;
    int16_t final_life;
    int16_t poison;
    char name[16];
    char deck_name[64];
    char commander_name[64];
} pg_pending_player_snapshot_t;

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t reserved;
    uint32_t session_id;
    long playgroup_id;
    long remote_game_id;
    uint32_t game_started_epoch;
    uint32_t game_started_tick_ms;
    int16_t starting_life;
    uint8_t player_count;
    int8_t winner;
    int8_t starting_player;
    uint8_t result_confirmed;
    uint8_t went_infinite;
    uint8_t sync_phase;
    char win_condition[32];
    uint16_t event_count;
    uint16_t raw_event_count;
    pg_pending_player_snapshot_t players[MAX_DISPLAY_PLAYERS];
    damage_log_record_t events[PG_PENDING_MAX_EVENTS];
} pg_pending_snapshot_t;

static pg_pending_seed_t current_seed;
static bool current_active = false;
static bool current_snapshot_saved = false;
static uint32_t current_session_id = 0;
static uint32_t current_game_started_epoch = 0;
static uint32_t current_game_started_tick_ms = 0;
static int8_t current_starting_player = -1;
static bool current_result_confirmed = false;
static bool current_went_infinite = false;
static int8_t current_result_winner = -1;
static char current_win_condition[32] = {0};

static bool pending_fs_ready(void)
{
    static bool checked = false;
    static bool ready = false;

    if (checked)
        return ready;

    checked = true;
    ready = SPIFFS.begin(false);
    if (!ready) {
        Serial.println("[Playgroup] Pending-game storage unavailable.");
    }
    return ready;
}

static void snapshot_path(uint32_t session_id, char *out, size_t out_size)
{
    snprintf(out, out_size, PG_PENDING_PATH_PREFIX "%08lx.bin",
             (unsigned long)session_id);
}

static bool is_pending_filename(const char *name)
{
    if (name == NULL)
        return false;

    /* Arduino-ESP32 SPIFFS may expose File.name() either with or without
       the leading slash depending on the FS wrapper/version. Accept both. */
    if (strncmp(name, PG_PENDING_PATH_PREFIX,
                strlen(PG_PENDING_PATH_PREFIX)) == 0)
        return true;

    return PG_PENDING_PATH_PREFIX[0] == '/' &&
           strncmp(name, PG_PENDING_PATH_PREFIX + 1,
                   strlen(PG_PENDING_PATH_PREFIX + 1)) == 0;
}

bool playgroup_pending_current_active(void)
{
    return current_active;
}

bool playgroup_pending_result_confirmed(void)
{
    return current_result_confirmed;
}

int playgroup_pending_count(void)
{
    int count = 0;
    File root;
    File file;

    if (!pending_fs_ready())
        return 0;

    root = SPIFFS.open("/");
    if (!root || !root.isDirectory())
        return 0;

    file = root.openNextFile();
    while (file) {
        if (!file.isDirectory() && is_pending_filename(file.name()))
            count++;
        file = root.openNextFile();
    }
    return count;
}


#define PG_SYNC_PHASE_QUEUED 0U
#define PG_SYNC_PHASE_REMOTE_CREATED 1U
#define PG_SYNC_PHASE_POST_STARTED 2U
#define PG_SYNC_BODY_CAPACITY (96U * 1024U)

static bool snapshot_write_file(const char *path,
                                const pg_pending_snapshot_t *snapshot)
{
    char temp_path[40];
    File file;
    size_t written;

    if (path == NULL || snapshot == NULL || !pending_fs_ready())
        return false;

    snprintf(temp_path, sizeof(temp_path), "%s.tmp", path);
    SPIFFS.remove(temp_path);

    file = SPIFFS.open(temp_path, FILE_WRITE);
    if (!file)
        return false;

    written = file.write((const uint8_t *)snapshot, sizeof(*snapshot));
    file.close();

    if (written != sizeof(*snapshot)) {
        SPIFFS.remove(temp_path);
        return false;
    }

    SPIFFS.remove(path);
    if (!SPIFFS.rename(temp_path, path)) {
        SPIFFS.remove(temp_path);
        return false;
    }
    return true;
}

static bool snapshot_read_file(const char *path,
                               pg_pending_snapshot_t *snapshot)
{
    File file;
    size_t read_count;

    if (path == NULL || snapshot == NULL || !pending_fs_ready())
        return false;

    file = SPIFFS.open(path, FILE_READ);
    if (!file)
        return false;

    read_count = file.read((uint8_t *)snapshot, sizeof(*snapshot));
    file.close();

    return read_count == sizeof(*snapshot) &&
           snapshot->magic == PG_PENDING_MAGIC &&
           snapshot->version == PG_PENDING_VERSION &&
           snapshot->player_count >= 2 &&
           snapshot->player_count <= MAX_DISPLAY_PLAYERS;
}

static bool json_appendf(char *buffer, size_t capacity, size_t *length,
                         const char *format, ...)
{
    va_list args;
    int written;

    if (buffer == NULL || length == NULL || format == NULL ||
        *length >= capacity)
        return false;

    va_start(args, format);
    written = vsnprintf(buffer + *length, capacity - *length, format, args);
    va_end(args);

    if (written < 0 || (size_t)written >= capacity - *length)
        return false;

    *length += (size_t)written;
    return true;
}

static bool json_append_escaped(char *buffer, size_t capacity, size_t *length,
                                const char *text)
{
    const unsigned char *p = (const unsigned char *)(text != NULL ? text : "");

    while (*p != '\0') {
        char escaped[7];
        const char *piece = NULL;

        switch (*p) {
            case '"': piece = "\\\""; break;
            case '\\': piece = "\\\\"; break;
            case '\b': piece = "\\b"; break;
            case '\f': piece = "\\f"; break;
            case '\n': piece = "\\n"; break;
            case '\r': piece = "\\r"; break;
            case '\t': piece = "\\t"; break;
            default:
                if (*p < 0x20) {
                    snprintf(escaped, sizeof(escaped), "\\u%04x", *p);
                    piece = escaped;
                }
                break;
        }

        if (piece != NULL) {
            if (!json_appendf(buffer, capacity, length, "%s", piece))
                return false;
        } else {
            if (*length + 1 >= capacity)
                return false;
            buffer[(*length)++] = (char)*p;
            buffer[*length] = '\0';
        }
        p++;
    }
    return true;
}

static uint32_t snapshot_max_elapsed_ms(const pg_pending_snapshot_t *snapshot)
{
    uint32_t max_elapsed = 0;

    for (int i = 0; i < snapshot->event_count; i++) {
        const damage_log_record_t *event = &snapshot->events[i];
        uint32_t elapsed = event->timestamp_ms - snapshot->game_started_tick_ms;

        if (elapsed > max_elapsed)
            max_elapsed = elapsed;
        if (event->event_type == LOG_EVT_TURN_END &&
            elapsed + event->duration_ms > max_elapsed)
            max_elapsed = elapsed + event->duration_ms;
    }
    return max_elapsed;
}

static uint32_t snapshot_base_epoch(const pg_pending_snapshot_t *snapshot)
{
    if (snapshot->game_started_epoch > 1700000000UL)
        return snapshot->game_started_epoch;

    {
        time_t now = time(NULL);
        uint32_t elapsed_s = snapshot_max_elapsed_ms(snapshot) / 1000U;
        if (now > 1700000000L && (uint32_t)now > elapsed_s)
            return (uint32_t)now - elapsed_s;
    }

    return 1700000000UL;
}

static uint32_t event_epoch(const pg_pending_snapshot_t *snapshot,
                            const damage_log_record_t *event,
                            uint32_t base_epoch)
{
    uint32_t elapsed_ms = event->timestamp_ms - snapshot->game_started_tick_ms;
    return base_epoch + (elapsed_ms / 1000U);
}

static int next_turn_player(const pg_pending_snapshot_t *snapshot, int index)
{
    uint16_t turn = snapshot->events[index].turn_number;

    for (int i = index + 1; i < snapshot->event_count; i++) {
        const damage_log_record_t *candidate = &snapshot->events[i];
        if (candidate->turn_number > turn &&
            candidate->turn_player >= 0 &&
            candidate->turn_player < snapshot->player_count)
            return candidate->turn_player;
    }
    return -1;
}

static bool append_event_prefix(char *body, size_t capacity, size_t *length,
                                bool *first,
                                unsigned long long external_id,
                                const char *name,
                                int source, int target, int active,
                                uint32_t event_time, int turn)
{
    if (!*first && !json_appendf(body, capacity, length, ","))
        return false;
    *first = false;

    if (!json_appendf(body, capacity, length,
                      "{\"id\":%llu,\"name\":\"%s\","
                      "\"source_player_id\":\"%d\"",
                      external_id, name, source))
        return false;

    if (target >= 0 &&
        !json_appendf(body, capacity, length,
                      ",\"target_player_id\":\"%d\"", target))
        return false;

    if (active >= 0 &&
        !json_appendf(body, capacity, length,
                      ",\"active_player_id\":\"%d\"", active))
        return false;

    return json_appendf(body, capacity, length,
                        ",\"time\":%lu,\"turn\":%d",
                        (unsigned long)event_time, turn);
}

static bool build_sync_body(const pg_pending_snapshot_t *snapshot,
                            char *body, size_t capacity)
{
    size_t length = 0;
    bool first = true;
    uint32_t base_epoch;
    unsigned long long id_base;
    unsigned ordinal = 1;
    int starting_player;

    if (snapshot == NULL || body == NULL || capacity < 1024)
        return false;

    body[0] = '\0';
    base_epoch = snapshot_base_epoch(snapshot);
    id_base = 4000000000000ULL +
              ((unsigned long long)snapshot->session_id * 1000ULL);
    starting_player = snapshot->starting_player;
    if (starting_player < 0 || starting_player >= snapshot->player_count)
        starting_player = 0;

    if (!json_appendf(body, capacity, &length, "{\"events\":["))
        return false;

    /* Setup events are synthesized from the immutable snapshot identity. */
    for (int i = 0; i < snapshot->player_count; i++) {
        const pg_pending_player_snapshot_t *player = &snapshot->players[i];

        if (!append_event_prefix(body, capacity, &length, &first,
                                 id_base + ordinal++, "Login",
                                 i, i, i, base_epoch, 0) ||
            !json_appendf(body, capacity, &length,
                          ",\"metadata\":{\"user_id\":%ld,"
                          "\"roster_player_id\":null,\"username\":\"",
                          player->user_id) ||
            !json_append_escaped(body, capacity, &length, player->name) ||
            !json_appendf(body, capacity, &length,
                          "\",\"commander_id\":null,"
                          "\"commander_name\":null,"
                          "\"commander_image\":null}}"))
            return false;
    }

    for (int i = 0; i < snapshot->player_count; i++) {
        const pg_pending_player_snapshot_t *player = &snapshot->players[i];

        if (!append_event_prefix(body, capacity, &length, &first,
                                 id_base + ordinal++, "DeckSelect",
                                 i, i, i, base_epoch + 1U, 0) ||
            !json_appendf(body, capacity, &length,
                          ",\"metadata\":{\"deck_id\":%ld,\"deck_name\":\"",
                          player->deck_id) ||
            !json_append_escaped(body, capacity, &length, player->deck_name) ||
            !json_appendf(body, capacity, &length,
                          "\",\"commander_id\":%ld,\"commander_name\":\"",
                          player->commander_id) ||
            !json_append_escaped(body, capacity, &length,
                                 player->commander_name) ||
            !json_appendf(body, capacity, &length,
                          "\",\"partner_id\":null,\"partner_name\":null,"
                          "\"playgroup_id\":%ld,\"counters\":[]}}",
                          snapshot->playgroup_id))
            return false;

        if (!append_event_prefix(body, capacity, &length, &first,
                                 id_base + ordinal++, "SeatReady",
                                 i, i, i, base_epoch + 2U, 0) ||
            !json_appendf(body, capacity, &length,
                          ",\"metadata\":{\"ready\":true}}"))
            return false;
    }

    if (!append_event_prefix(body, capacity, &length, &first,
                             id_base + ordinal++, "StartingPlayer",
                             starting_player, starting_player, starting_player,
                             base_epoch + 3U, 0) ||
        !json_appendf(body, capacity, &length, ",\"metadata\":{}}"))
        return false;

    for (int i = 0; i < snapshot->player_count; i++) {
        const pg_pending_player_snapshot_t *player = &snapshot->players[i];
        if (!append_event_prefix(body, capacity, &length, &first,
                                 id_base + ordinal++, "KeepHand",
                                 i, i, i, base_epoch + 4U, 0) ||
            !json_appendf(body, capacity, &length,
                          ",\"metadata\":{\"mulligans_taken\":%u}}",
                          (unsigned)player->mulligans))
            return false;
    }

    if (!append_event_prefix(body, capacity, &length, &first,
                             id_base + ordinal++, "StartGame",
                             starting_player, -1, starting_player,
                             base_epoch + 5U, 0) ||
        !json_appendf(body, capacity, &length,
                      ",\"metadata\":{\"started_at\":%lu}}",
                      (unsigned long)(base_epoch + 5U)))
        return false;

    for (int i = 0; i < snapshot->event_count; i++) {
        const damage_log_record_t *event = &snapshot->events[i];
        int source = event->source;
        int target = event->player;
        int active = event->turn_player;
        int turn = event->turn_number > 0 ? event->turn_number : 1;
        int amount = event->delta >= 0 ? event->delta : -event->delta;
        uint32_t when = event_epoch(snapshot, event, base_epoch + 5U);

        if (target < 0 || target >= snapshot->player_count)
            continue;
        if (active < 0 || active >= snapshot->player_count)
            active = starting_player;

        switch (event->event_type) {
            case LOG_EVT_DAMAGE:
                if (source < 0 || source >= snapshot->player_count)
                    source = target;
                if (!append_event_prefix(body, capacity, &length, &first,
                                         id_base + ordinal++, "Damage",
                                         source, target, active, when, turn) ||
                    !json_appendf(body, capacity, &length,
                                  ",\"amount\":%d,\"metadata\":{}}",
                                  amount))
                    return false;
                break;

            case LOG_EVT_CMD_DAMAGE:
                if (source < 0 || source >= snapshot->player_count)
                    continue;
                if (!append_event_prefix(body, capacity, &length, &first,
                                         id_base + ordinal++,
                                         "CommanderDamage",
                                         source, target, active, when, turn) ||
                    !json_appendf(body, capacity, &length,
                                  ",\"amount\":%d,\"commander_id\":%ld,"
                                  "\"metadata\":{}}",
                                  amount,
                                  snapshot->players[source].commander_id))
                    return false;
                break;

            case LOG_EVT_CMD_INFECT:
                /* Playgroup has no atomic commander+infect event. Import the
                   poison effect only rather than incorrectly reducing life. */
                if (source < 0 || source >= snapshot->player_count)
                    source = target;
                if (!append_event_prefix(body, capacity, &length, &first,
                                         id_base + ordinal++, "PoisonCounter",
                                         source, target, active, when, turn) ||
                    !json_appendf(body, capacity, &length,
                                  ",\"amount\":%d,\"metadata\":{}}",
                                  amount))
                    return false;
                break;

            case LOG_EVT_POISON:
                if (source < 0 || source >= snapshot->player_count)
                    source = target;
                if (!append_event_prefix(body, capacity, &length, &first,
                                         id_base + ordinal++, "PoisonCounter",
                                         source, target, active, when, turn) ||
                    !json_appendf(body, capacity, &length,
                                  ",\"amount\":%d,\"metadata\":{}}",
                                  event->delta))
                    return false;
                break;

            case LOG_EVT_LIFE:
                if (event->delta > 0) {
                    if (!append_event_prefix(body, capacity, &length, &first,
                                             id_base + ordinal++, "Healing",
                                             target, target, active, when, turn) ||
                        !json_appendf(body, capacity, &length,
                                      ",\"amount\":%d,\"metadata\":{}}",
                                      amount))
                        return false;
                } else {
                    /* Unsourced manual life loss has no dedicated accepted
                       batch name. Self-source preserves the life timeline
                       without attributing damage to another player. */
                    if (!append_event_prefix(body, capacity, &length, &first,
                                             id_base + ordinal++, "Damage",
                                             target, target, active, when, turn) ||
                        !json_appendf(body, capacity, &length,
                                      ",\"amount\":%d,\"metadata\":{}}",
                                      amount))
                        return false;
                }
                break;

            case LOG_EVT_TURN_END: {
                int next = next_turn_player(snapshot, i);
                if (next < 0)
                    break;
                if (!append_event_prefix(body, capacity, &length, &first,
                                         id_base + ordinal++, "PassTurn",
                                         target, -1, target, when, turn) ||
                    !json_appendf(body, capacity, &length,
                                  ",\"metadata\":{\"next_player_id\":\"%d\"}}",
                                  next))
                    return false;
                break;
            }

            default:
                /* Non-Playgroup counters stay local until their public event
                   contract is explicitly verified. */
                break;
        }
    }

    {
        uint32_t end_time = base_epoch + 5U +
                            (snapshot_max_elapsed_ms(snapshot) / 1000U) + 1U;
        int winner = snapshot->winner;
        int final_turn = 1;

        if (snapshot->event_count > 0) {
            int candidate =
                snapshot->events[snapshot->event_count - 1].turn_number;
            if (candidate > 0)
                final_turn = candidate;
        }

        if (!append_event_prefix(body, capacity, &length, &first,
                                 id_base + ordinal++, "EndGame",
                                 winner, -1, winner, end_time, final_turn) ||
            !json_appendf(body, capacity, &length, ",\"metadata\":{}}") ||
            !append_event_prefix(body, capacity, &length, &first,
                                 id_base + ordinal++, "WinnerDeclared",
                                 winner, -1, winner, end_time + 1U, final_turn) ||
            !json_appendf(body, capacity, &length, ",\"metadata\":{}}") ||
            !append_event_prefix(body, capacity, &length, &first,
                                 id_base + ordinal++, "WinConSet",
                                 winner, -1, winner, end_time + 2U, final_turn) ||
            !json_appendf(body, capacity, &length,
                          ",\"metadata\":{\"win_con\":\"") ||
            !json_append_escaped(body, capacity, &length,
                                 snapshot->win_condition) ||
            !json_appendf(body, capacity, &length,
                          "\",\"infinite\":%s}}",
                          snapshot->went_infinite ? "true" : "false"))
            return false;
    }

    return json_appendf(body, capacity, &length, "]}");
}

static bool snapshot_ready_for_sync(const pg_pending_snapshot_t *snapshot)
{
    if (snapshot == NULL || !snapshot->result_confirmed ||
        snapshot->playgroup_id <= 0 ||
        snapshot->winner < 0 || snapshot->winner >= snapshot->player_count ||
        snapshot->win_condition[0] == '\0')
        return false;

    for (int i = 0; i < snapshot->player_count; i++) {
        if (snapshot->players[i].user_id <= 0 ||
            snapshot->players[i].deck_id <= 0)
            return false;
    }
    return true;
}

static bool sync_one_snapshot(const char *path,
                              pg_pending_snapshot_t *snapshot)
{
    char *body = NULL;
    int status;
    long winner_user_id;

    if (!snapshot_ready_for_sync(snapshot))
        return false;

    winner_user_id = snapshot->players[snapshot->winner].user_id;

    /* A previous POST may have committed even if the Dial lost the response.
       Reconcile before any retry because duplicate external_ids return 422. */
    if (snapshot->sync_phase == PG_SYNC_PHASE_POST_STARTED &&
        snapshot->remote_game_id > 0) {
        if (playgroup_remote_game_finalized(snapshot->playgroup_id,
                                            snapshot->remote_game_id,
                                            winner_user_id)) {
            Serial.println("[Playgroup] Pending game reconciled after ambiguous POST.");
            return true;
        }
    }

    if (snapshot->remote_game_id <= 0) {
        snapshot->remote_game_id =
            playgroup_create_game_for_sync(snapshot->playgroup_id,
                                           snapshot->player_count,
                                           snapshot->starting_life);
        if (snapshot->remote_game_id <= 0)
            return false;

        snapshot->sync_phase = PG_SYNC_PHASE_REMOTE_CREATED;
        if (!snapshot_write_file(path, snapshot)) {
            Serial.println("[Playgroup] Could not persist remote game id; refusing import.");
            return false;
        }
    }

    body = (char *)heap_caps_malloc(PG_SYNC_BODY_CAPACITY,
                                    MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (body == NULL) {
        Serial.println("[Playgroup] Sync body allocation failed.");
        return false;
    }

    if (!build_sync_body(snapshot, body, PG_SYNC_BODY_CAPACITY)) {
        Serial.println("[Playgroup] Could not build pending game event batch.");
        heap_caps_free(body);
        return false;
    }

    /* Persist this phase before the network write. A reboot from this point
       onward is treated as ambiguous and reconciled before retry. */
    snapshot->sync_phase = PG_SYNC_PHASE_POST_STARTED;
    if (!snapshot_write_file(path, snapshot)) {
        heap_caps_free(body);
        return false;
    }

    status = playgroup_import_events_for_sync(snapshot->remote_game_id, body);
    heap_caps_free(body);

    if (status == 201)
        return true;

    if (playgroup_remote_game_finalized(snapshot->playgroup_id,
                                        snapshot->remote_game_id,
                                        winner_user_id))
        return true;

    Serial.println("[Playgroup] Pending game remains queued for a later connection.");
    return false;
}

int playgroup_pending_sync_queue(void)
{
    char paths[PG_PENDING_MAX_QUEUE][40];
    int path_count = 0;
    int synced = 0;
    File root;
    File file;
    pg_pending_snapshot_t *snapshot = NULL;

    if (!pending_fs_ready())
        return 0;

    /* Snapshot the tiny directory listing first. This avoids mutating SPIFFS
       while iterating its directory handle and keeps the control flow simple. */
    root = SPIFFS.open("/");
    if (!root || !root.isDirectory())
        return 0;

    file = root.openNextFile();
    while (file && path_count < PG_PENDING_MAX_QUEUE) {
        if (!file.isDirectory() && is_pending_filename(file.name())) {
            if (file.name()[0] == '/')
                strlcpy(paths[path_count], file.name(),
                        sizeof(paths[path_count]));
            else
                snprintf(paths[path_count], sizeof(paths[path_count]),
                         "/%s", file.name());
            path_count++;
        }
        file = root.openNextFile();
    }
    if (file)
        file.close();
    root.close();

    if (path_count == 0)
        return 0;

    snapshot = (pg_pending_snapshot_t *)heap_caps_calloc(
        1, sizeof(pg_pending_snapshot_t),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (snapshot == NULL)
        return 0;

    for (int i = 0; i < path_count; i++) {
        memset(snapshot, 0, sizeof(*snapshot));

        if (!snapshot_read_file(paths[i], snapshot) ||
            !snapshot_ready_for_sync(snapshot))
            continue;

        Serial.print("[Playgroup] Syncing pending session ");
        Serial.print((unsigned long)snapshot->session_id, HEX);
        Serial.print(" -> ");
        Serial.println(paths[i]);

        if (!sync_one_snapshot(paths[i], snapshot)) {
            /* Network/API failure: stop here. Repeated attempts in the same
               connection only add latency; the next connection retries. */
            break;
        }

        if (!SPIFFS.remove(paths[i])) {
            Serial.println("[Playgroup] Remote sync confirmed but local queue removal failed.");
            break;
        }

        synced++;
        Serial.println("[Playgroup] Pending game synced and removed.");
        if (current_snapshot_saved &&
            snapshot->session_id == current_session_id)
            current_snapshot_saved = false;

        /* Drain one game per connection so entering Playgroup setup never
           blocks behind a long offline backlog. */
        break;
    }

    heap_caps_free(snapshot);

    if (synced > 0) {
        Serial.print("[Playgroup] Pending sync complete: ");
        Serial.print(synced);
        Serial.println(" game confirmed.");
    }
    return synced;
}

static bool remove_current_snapshot(void)
{
    char path[32];

    if (!current_snapshot_saved || current_session_id == 0)
        return true;
    if (!pending_fs_ready())
        return false;

    snapshot_path(current_session_id, path, sizeof(path));
    if (SPIFFS.exists(path) && !SPIFFS.remove(path))
        return false;

    current_snapshot_saved = false;
    Serial.println("[Playgroup] Pending game reopened after undo; snapshot removed.");
    return true;
}

void playgroup_pending_begin_game(const pg_pending_seed_t *seed)
{
    if (seed == NULL || seed->playgroup_id <= 0 ||
        seed->player_count < 2 || seed->player_count > MAX_DISPLAY_PLAYERS) {
        playgroup_pending_disable_current();
        return;
    }

    current_seed = *seed;
    current_active = true;
    current_snapshot_saved = false;
    current_game_started_tick_ms = millis();
    {
        time_t now = time(NULL);
        current_game_started_epoch = (now > 1700000000L) ? (uint32_t)now : 0U;
    }
    current_starting_player = -1;
    current_result_confirmed = false;
    current_went_infinite = false;
    current_result_winner = -1;
    current_win_condition[0] = '\0';

    do {
        current_session_id = esp_random();
    } while (current_session_id == 0);

    Serial.print("[Playgroup] Tracking game session ");
    Serial.print((unsigned long)current_session_id, HEX);
    Serial.print(" for playgroup ");
    Serial.println(current_seed.playgroup_id);
}

void playgroup_pending_disable_current(void)
{
    current_active = false;
    current_snapshot_saved = false;
    current_session_id = 0;
    current_game_started_epoch = 0;
    current_game_started_tick_ms = 0;
    current_starting_player = -1;
    current_result_confirmed = false;
    current_went_infinite = false;
    current_result_winner = -1;
    current_win_condition[0] = '\0';
    memset(&current_seed, 0, sizeof(current_seed));
}

static bool write_finished_snapshot(int winner)
{
    pg_pending_snapshot_t *snapshot;
    char path[32];
    File file;
    int log_count;
    int start;
    int copied = 0;
    int queue_count;
    bool was_saved;
    bool ok = false;

    if (!current_active || winner < 0 ||
        winner >= current_seed.player_count)
        return false;

    if (!pending_fs_ready())
        return false;

    queue_count = playgroup_pending_count();
    if (!current_snapshot_saved && queue_count >= PG_PENDING_MAX_QUEUE) {
        Serial.println("[Playgroup] Pending queue full; game snapshot not saved.");
        return false;
    }

    snapshot = (pg_pending_snapshot_t *)heap_caps_calloc(
        1, sizeof(pg_pending_snapshot_t),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (snapshot == NULL) {
        Serial.println("[Playgroup] Pending snapshot allocation failed.");
        return false;
    }

    snapshot->magic = PG_PENDING_MAGIC;
    snapshot->version = PG_PENDING_VERSION;
    snapshot->session_id = current_session_id;
    snapshot->playgroup_id = current_seed.playgroup_id;
    snapshot->remote_game_id = 0;
    snapshot->game_started_epoch = current_game_started_epoch;
    snapshot->game_started_tick_ms = current_game_started_tick_ms;
    snapshot->starting_life = (int16_t)nvs_get_life_total();
    snapshot->player_count = current_seed.player_count;
    snapshot->winner = current_result_confirmed ? current_result_winner : (int8_t)winner;
    snapshot->starting_player = current_starting_player;
    snapshot->result_confirmed = current_result_confirmed ? 1U : 0U;
    snapshot->went_infinite = current_went_infinite ? 1U : 0U;
    snapshot->sync_phase = 0;
    strlcpy(snapshot->win_condition, current_win_condition,
            sizeof(snapshot->win_condition));

    for (int i = 0; i < current_seed.player_count; i++) {
        snapshot->players[i].user_id = current_seed.players[i].user_id;
        snapshot->players[i].deck_id = current_seed.players[i].deck_id;
        snapshot->players[i].commander_id = current_seed.players[i].commander_id;
        snapshot->players[i].mulligans = current_seed.players[i].mulligans;
        snapshot->players[i].eliminated = player_eliminated[i] ? 1U : 0U;
        snapshot->players[i].final_life = (int16_t)player_life[i];
        snapshot->players[i].poison =
            (int16_t)player_counters[i][COUNTER_TYPE_POISON];
        strlcpy(snapshot->players[i].name, current_seed.players[i].name,
                sizeof(snapshot->players[i].name));
        strlcpy(snapshot->players[i].deck_name,
                current_seed.players[i].deck_name,
                sizeof(snapshot->players[i].deck_name));
        strlcpy(snapshot->players[i].commander_name,
                current_seed.players[i].commander_name,
                sizeof(snapshot->players[i].commander_name));
    }

    log_count = damage_log_record_count();
    snapshot->raw_event_count = (uint16_t)log_count;
    start = (log_count > PG_PENDING_MAX_EVENTS)
                ? (log_count - PG_PENDING_MAX_EVENTS)
                : 0;

    for (int i = start; i < log_count && copied < PG_PENDING_MAX_EVENTS; i++) {
        if (damage_log_record_get_oldest(i, &snapshot->events[copied]))
            copied++;
    }
    snapshot->event_count = (uint16_t)copied;

    if (snapshot->starting_player < 0) {
        for (int i = 0; i < copied; i++) {
            if (snapshot->events[i].turn_player >= 0 &&
                snapshot->events[i].turn_player < current_seed.player_count) {
                snapshot->starting_player = snapshot->events[i].turn_player;
                break;
            }
        }
    }

    snapshot_path(current_session_id, path, sizeof(path));
    file = SPIFFS.open(path, FILE_WRITE);
    if (!file) {
        Serial.println("[Playgroup] Could not create pending game snapshot.");
        goto cleanup;
    }

    {
        size_t written = file.write((const uint8_t *)snapshot, sizeof(*snapshot));
        file.close();

        if (written != sizeof(*snapshot)) {
            SPIFFS.remove(path);
            Serial.println("[Playgroup] Pending game snapshot write incomplete.");
            goto cleanup;
        }
    }

    was_saved = current_snapshot_saved;
    current_snapshot_saved = true;
    Serial.print(was_saved
                     ? "[Playgroup] Finished game snapshot refreshed. Winner P"
                     : "[Playgroup] Game finished. Winner P");
    Serial.print(winner + 1);
    Serial.print("; snapshot has ");
    Serial.print(copied);
    Serial.print(" reconciled event(s). Pending queue: ");
    Serial.println(was_saved ? queue_count : queue_count + 1);
    ok = true;

cleanup:
    heap_caps_free(snapshot);
    return ok;
}

void playgroup_pending_note_starting_player(int player)
{
    if (!current_active || player < 0 || player >= current_seed.player_count)
        return;
    if (current_starting_player < 0)
        current_starting_player = (int8_t)player;
}

void playgroup_pending_set_result(int winner, const char *win_condition,
                                  bool went_infinite)
{
    if (!current_active || winner < 0 || winner >= current_seed.player_count)
        return;

    current_result_winner = (int8_t)winner;
    current_result_confirmed = true;
    current_went_infinite = went_infinite;
    strlcpy(current_win_condition,
            win_condition != NULL ? win_condition : "",
            sizeof(current_win_condition));

    /* Refresh an already queued inferred result with the user's confirmed
       questionnaire answer. Older queued games are untouched. */
    if (current_snapshot_saved)
        write_finished_snapshot(winner);
}

void playgroup_pending_reconcile_outcome(void)
{
    int alive_count = 0;
    int winner = -1;

    if (!current_active)
        return;

    for (int i = 0; i < current_seed.player_count; i++) {
        if (!player_eliminated[i]) {
            alive_count++;
            winner = i;
        }
    }

    if (alive_count == 1) {
        write_finished_snapshot(winner);
    } else if (current_snapshot_saved) {
        /* An undo/revival reopened the game. Only remove this session's
           snapshot; older unsynced games remain untouched. */
        remove_current_snapshot();
    }
}

void playgroup_pending_print_status(void)
{
    int count = playgroup_pending_count();

    Serial.print("[Playgroup] Pending game snapshots: ");
    Serial.println(count);

    if (current_active) {
        Serial.print("[Playgroup] Current tracked session: ");
        Serial.print((unsigned long)current_session_id, HEX);
        Serial.print(current_snapshot_saved ? " (finished/queued)" : " (in progress)");
        Serial.println();
    } else {
        Serial.println("[Playgroup] Current game is not Playgroup-tracked.");
    }
}


bool playgroup_pending_selftest(void)
{
    static const char *test_path = "/pgq_FFFFFFFE.bin";
    pg_pending_snapshot_t *written_snapshot = NULL;
    pg_pending_snapshot_t *read_snapshot = NULL;
    File file;
    int before_count;
    int during_count;
    int after_count;
    bool ok = true;

    Serial.println("[Playgroup] SELFTEST: pending-game storage");

    if (!pending_fs_ready()) {
        Serial.println("[Playgroup] SELFTEST FAIL: SPIFFS unavailable.");
        return false;
    }

    written_snapshot = (pg_pending_snapshot_t *)heap_caps_calloc(
        1, sizeof(pg_pending_snapshot_t),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    read_snapshot = (pg_pending_snapshot_t *)heap_caps_calloc(
        1, sizeof(pg_pending_snapshot_t),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (written_snapshot == NULL || read_snapshot == NULL) {
        Serial.println("[Playgroup] SELFTEST FAIL: snapshot allocation failed.");
        ok = false;
        goto cleanup_memory;
    }

    /* The reserved FFFFFFFE id is used only by this test. Always clean up
       a stale test artifact first, e.g. after a power loss mid-self-test. */
    if (SPIFFS.exists(test_path) && !SPIFFS.remove(test_path)) {
        Serial.println("[Playgroup] SELFTEST FAIL: stale test file could not be removed.");
        ok = false;
        goto cleanup_memory;
    }

    before_count = playgroup_pending_count();

    written_snapshot->magic = PG_PENDING_MAGIC;
    written_snapshot->version = PG_PENDING_VERSION;
    written_snapshot->session_id = 0xFFFFFFFEUL;
    written_snapshot->playgroup_id = 63005;
    written_snapshot->remote_game_id = 0;
    written_snapshot->game_started_epoch = 1791370000U;
    written_snapshot->game_started_tick_ms = 1234U;
    written_snapshot->starting_life = 40;
    written_snapshot->player_count = 4;
    written_snapshot->winner = 2;
    written_snapshot->starting_player = 1;
    written_snapshot->result_confirmed = 1;
    written_snapshot->went_infinite = 0;
    strlcpy(written_snapshot->win_condition, "combat",
            sizeof(written_snapshot->win_condition));
    written_snapshot->event_count = 2;
    written_snapshot->raw_event_count = 2;

    written_snapshot->players[0].user_id = 101;
    written_snapshot->players[0].deck_id = 1001;
    written_snapshot->players[0].commander_id = 2001;
    written_snapshot->players[0].mulligans = 1;
    written_snapshot->players[0].eliminated = 1;
    written_snapshot->players[0].final_life = -2;
    strlcpy(written_snapshot->players[0].name, "P1",
            sizeof(written_snapshot->players[0].name));
    strlcpy(written_snapshot->players[0].deck_name, "Deck 1",
            sizeof(written_snapshot->players[0].deck_name));
    strlcpy(written_snapshot->players[0].commander_name, "Commander 1",
            sizeof(written_snapshot->players[0].commander_name));

    written_snapshot->players[2].user_id = 103;
    written_snapshot->players[2].deck_id = 1003;
    written_snapshot->players[2].mulligans = 0;
    written_snapshot->players[2].eliminated = 0;
    written_snapshot->players[2].final_life = 17;
    strlcpy(written_snapshot->players[2].name, "Winner",
            sizeof(written_snapshot->players[2].name));

    written_snapshot->events[0].event_type = LOG_EVT_LIFE;
    written_snapshot->events[0].player = 0;
    written_snapshot->events[0].delta = -5;
    written_snapshot->events[0].turn_number = 1;
    written_snapshot->events[0].action_id = 7;

    written_snapshot->events[1].event_type = LOG_EVT_TURN_END;
    written_snapshot->events[1].player = 2;
    written_snapshot->events[1].turn_number = 1;
    written_snapshot->events[1].duration_ms = 12345;
    written_snapshot->events[1].action_id = 8;

    file = SPIFFS.open(test_path, FILE_WRITE);
    if (!file) {
        Serial.println("[Playgroup] SELFTEST FAIL: could not create test snapshot.");
        ok = false;
        goto cleanup_file;
    }

    if (file.write((const uint8_t *)written_snapshot,
                   sizeof(*written_snapshot)) != sizeof(*written_snapshot)) {
        ok = false;
        Serial.println("[Playgroup] SELFTEST FAIL: incomplete write.");
    }
    file.close();

    during_count = playgroup_pending_count();
    if (during_count != before_count + 1) {
        ok = false;
        Serial.print("[Playgroup] SELFTEST FAIL: queue count ");
        Serial.print(before_count);
        Serial.print(" -> ");
        Serial.print(during_count);
        Serial.println(" (expected +1).");
    }

    file = SPIFFS.open(test_path, FILE_READ);
    if (!file) {
        ok = false;
        Serial.println("[Playgroup] SELFTEST FAIL: could not reopen snapshot.");
    } else {
        size_t read_count =
            file.read((uint8_t *)read_snapshot, sizeof(*read_snapshot));
        file.close();

        if (read_count != sizeof(*read_snapshot)) {
            ok = false;
            Serial.println("[Playgroup] SELFTEST FAIL: incomplete read.");
        } else if (memcmp(written_snapshot, read_snapshot,
                          sizeof(*written_snapshot)) != 0) {
            ok = false;
            Serial.println("[Playgroup] SELFTEST FAIL: round-trip data mismatch.");
        }
    }

    if (read_snapshot->magic != PG_PENDING_MAGIC ||
        read_snapshot->version != PG_PENDING_VERSION ||
        read_snapshot->playgroup_id != 63005 ||
        read_snapshot->player_count != 4 ||
        read_snapshot->winner != 2 ||
        read_snapshot->event_count != 2 ||
        read_snapshot->players[2].deck_id != 1003 ||
        read_snapshot->events[1].duration_ms != 12345) {
        ok = false;
        Serial.println("[Playgroup] SELFTEST FAIL: snapshot fields invalid.");
    }

cleanup_file:
    if (SPIFFS.exists(test_path) && !SPIFFS.remove(test_path)) {
        ok = false;
        Serial.println("[Playgroup] SELFTEST FAIL: cleanup failed.");
    }

    after_count = playgroup_pending_count();
    if (after_count != before_count) {
        ok = false;
        Serial.print("[Playgroup] SELFTEST FAIL: queue count after cleanup is ");
        Serial.print(after_count);
        Serial.print(" (expected ");
        Serial.print(before_count);
        Serial.println(").");
    }

cleanup_memory:
    if (written_snapshot != NULL) heap_caps_free(written_snapshot);
    if (read_snapshot != NULL) heap_caps_free(read_snapshot);

    Serial.print("[Playgroup] SELFTEST ");
    Serial.println(ok ? "PASS" : "FAIL");
    return ok;
}
