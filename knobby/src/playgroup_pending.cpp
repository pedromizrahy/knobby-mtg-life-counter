#include <Arduino.h>
#include <SPIFFS.h>
#include <esp_random.h>

#include "playgroup_pending.h"
#include "damage_log.h"
#include "game.h"
#include "storage.h"

#define PG_PENDING_MAGIC 0x50475131UL
#define PG_PENDING_VERSION 1U
#define PG_PENDING_PATH_PREFIX "/pgq_"

typedef struct {
    long user_id;
    long deck_id;
    uint8_t mulligans;
    uint8_t eliminated;
    int16_t final_life;
    int16_t poison;
    char name[16];
} pg_pending_player_snapshot_t;

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t reserved;
    uint32_t session_id;
    long playgroup_id;
    uint8_t player_count;
    int8_t winner;
    uint16_t event_count;
    uint16_t raw_event_count;
    pg_pending_player_snapshot_t players[MAX_DISPLAY_PLAYERS];
    damage_log_record_t events[PG_PENDING_MAX_EVENTS];
} pg_pending_snapshot_t;

static pg_pending_seed_t current_seed;
static bool current_active = false;
static bool current_snapshot_saved = false;
static uint32_t current_session_id = 0;

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
    return name != NULL && strncmp(name, PG_PENDING_PATH_PREFIX,
                                   strlen(PG_PENDING_PATH_PREFIX)) == 0;
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
    memset(&current_seed, 0, sizeof(current_seed));
}

static bool write_finished_snapshot(int winner)
{
    pg_pending_snapshot_t snapshot;
    char path[32];
    File file;
    int log_count;
    int start;
    int copied = 0;
    int queue_count;

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

    memset(&snapshot, 0, sizeof(snapshot));
    snapshot.magic = PG_PENDING_MAGIC;
    snapshot.version = PG_PENDING_VERSION;
    snapshot.session_id = current_session_id;
    snapshot.playgroup_id = current_seed.playgroup_id;
    snapshot.player_count = current_seed.player_count;
    snapshot.winner = (int8_t)winner;

    for (int i = 0; i < current_seed.player_count; i++) {
        snapshot.players[i].user_id = current_seed.players[i].user_id;
        snapshot.players[i].deck_id = current_seed.players[i].deck_id;
        snapshot.players[i].mulligans = current_seed.players[i].mulligans;
        snapshot.players[i].eliminated = player_eliminated[i] ? 1U : 0U;
        snapshot.players[i].final_life = (int16_t)player_life[i];
        snapshot.players[i].poison =
            (int16_t)player_counters[i][COUNTER_TYPE_POISON];
        strlcpy(snapshot.players[i].name, current_seed.players[i].name,
                sizeof(snapshot.players[i].name));
    }

    log_count = damage_log_record_count();
    snapshot.raw_event_count = (uint16_t)log_count;
    start = (log_count > PG_PENDING_MAX_EVENTS)
                ? (log_count - PG_PENDING_MAX_EVENTS)
                : 0;

    for (int i = start; i < log_count && copied < PG_PENDING_MAX_EVENTS; i++) {
        if (damage_log_record_get_oldest(i, &snapshot.events[copied]))
            copied++;
    }
    snapshot.event_count = (uint16_t)copied;

    snapshot_path(current_session_id, path, sizeof(path));
    file = SPIFFS.open(path, FILE_WRITE);
    if (!file) {
        Serial.println("[Playgroup] Could not create pending game snapshot.");
        return false;
    }

    size_t written = file.write((const uint8_t *)&snapshot, sizeof(snapshot));
    file.close();

    if (written != sizeof(snapshot)) {
        SPIFFS.remove(path);
        Serial.println("[Playgroup] Pending game snapshot write incomplete.");
        return false;
    }

    {
        bool was_saved = current_snapshot_saved;
        current_snapshot_saved = true;
        Serial.print(was_saved
                         ? "[Playgroup] Finished game snapshot refreshed. Winner P"
                         : "[Playgroup] Game finished. Winner P");
        Serial.print(winner + 1);
        Serial.print("; snapshot has ");
        Serial.print(copied);
        Serial.print(" reconciled event(s). Pending queue: ");
        Serial.println(was_saved ? queue_count : queue_count + 1);
    }
    return true;
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
    pg_pending_snapshot_t written_snapshot;
    pg_pending_snapshot_t read_snapshot;
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

    /* The reserved FFFFFFFE id is used only by this test. Always clean up
       a stale test artifact first, e.g. after a power loss mid-self-test. */
    if (SPIFFS.exists(test_path) && !SPIFFS.remove(test_path)) {
        Serial.println("[Playgroup] SELFTEST FAIL: stale test file could not be removed.");
        return false;
    }

    before_count = playgroup_pending_count();

    memset(&written_snapshot, 0, sizeof(written_snapshot));
    written_snapshot.magic = PG_PENDING_MAGIC;
    written_snapshot.version = PG_PENDING_VERSION;
    written_snapshot.session_id = 0xFFFFFFFEUL;
    written_snapshot.playgroup_id = 63005;
    written_snapshot.player_count = 4;
    written_snapshot.winner = 2;
    written_snapshot.event_count = 2;
    written_snapshot.raw_event_count = 2;

    written_snapshot.players[0].user_id = 101;
    written_snapshot.players[0].deck_id = 1001;
    written_snapshot.players[0].mulligans = 1;
    written_snapshot.players[0].eliminated = 1;
    written_snapshot.players[0].final_life = -2;
    strlcpy(written_snapshot.players[0].name, "P1",
            sizeof(written_snapshot.players[0].name));

    written_snapshot.players[2].user_id = 103;
    written_snapshot.players[2].deck_id = 1003;
    written_snapshot.players[2].mulligans = 0;
    written_snapshot.players[2].eliminated = 0;
    written_snapshot.players[2].final_life = 17;
    strlcpy(written_snapshot.players[2].name, "Winner",
            sizeof(written_snapshot.players[2].name));

    written_snapshot.events[0].event_type = LOG_EVT_LIFE;
    written_snapshot.events[0].player = 0;
    written_snapshot.events[0].delta = -5;
    written_snapshot.events[0].turn_number = 1;
    written_snapshot.events[0].action_id = 7;

    written_snapshot.events[1].event_type = LOG_EVT_TURN_END;
    written_snapshot.events[1].player = 2;
    written_snapshot.events[1].turn_number = 1;
    written_snapshot.events[1].duration_ms = 12345;
    written_snapshot.events[1].action_id = 8;

    file = SPIFFS.open(test_path, FILE_WRITE);
    if (!file) {
        Serial.println("[Playgroup] SELFTEST FAIL: could not create test snapshot.");
        return false;
    }

    if (file.write((const uint8_t *)&written_snapshot,
                   sizeof(written_snapshot)) != sizeof(written_snapshot)) {
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

    memset(&read_snapshot, 0, sizeof(read_snapshot));
    file = SPIFFS.open(test_path, FILE_READ);
    if (!file) {
        ok = false;
        Serial.println("[Playgroup] SELFTEST FAIL: could not reopen snapshot.");
    } else {
        size_t read_count =
            file.read((uint8_t *)&read_snapshot, sizeof(read_snapshot));
        file.close();

        if (read_count != sizeof(read_snapshot)) {
            ok = false;
            Serial.println("[Playgroup] SELFTEST FAIL: incomplete read.");
        } else if (memcmp(&written_snapshot, &read_snapshot,
                          sizeof(written_snapshot)) != 0) {
            ok = false;
            Serial.println("[Playgroup] SELFTEST FAIL: round-trip data mismatch.");
        }
    }

    if (read_snapshot.magic != PG_PENDING_MAGIC ||
        read_snapshot.version != PG_PENDING_VERSION ||
        read_snapshot.playgroup_id != 63005 ||
        read_snapshot.player_count != 4 ||
        read_snapshot.winner != 2 ||
        read_snapshot.event_count != 2 ||
        read_snapshot.players[2].deck_id != 1003 ||
        read_snapshot.events[1].duration_ms != 12345) {
        ok = false;
        Serial.println("[Playgroup] SELFTEST FAIL: snapshot fields invalid.");
    }

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

    Serial.print("[Playgroup] SELFTEST ");
    Serial.println(ok ? "PASS" : "FAIL");
    return ok;
}
