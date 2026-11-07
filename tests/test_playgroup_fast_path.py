"""Guard the fast Playgroup selection path from accidental regressions.

This is a source-level integration guard; hardware/ESP32 reset behavior must
still be verified on the device. No account credentials/network involved.
"""
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PREGAME = (ROOT / "knobby/src/pregame.c").read_text(encoding="utf-8")
API = (ROOT / "knobby/src/playgroup_api.cpp").read_text(encoding="utf-8")

def between(source: str, start: str, end: str) -> str:
    return source.split(start, 1)[1].split(end, 1)[0]

class PlaygroupFastPathTests(unittest.TestCase):
    def test_player_selection_does_not_wait_for_bulk_art(self):
        handler = between(PREGAME, "static void event_member_select(lv_event_t *e)\n{",
                          "\nstatic void clear_deck_art(void)")
        self.assertNotIn("start_commander_prepare(", handler)
        self.assertIn("refresh_deck_picker(false);", handler)
        self.assertIn("lv_scr_load(screen_pregame_deck);", handler)
        self.assertIn("schedule_deck_art();", handler)

    def test_roster_deck_reselection_does_not_wait_for_bulk_art(self):
        handler = between(PREGAME, "static void event_roster_open_decks(lv_event_t *e)\n{",
                          "\nstatic void event_deck_select(lv_event_t *e)")
        self.assertNotIn("start_commander_prepare(", handler)
        self.assertIn("lv_scr_load(screen_pregame_deck);", handler)
        self.assertIn("schedule_deck_art();", handler)

    def test_loading_account_never_syncs_pending_game(self):
        refresh = between(API, "bool playgroup_refresh_playgroups(void)\n{",
                          "\nint playgroup_cached_playgroup_count(void)")
        self.assertNotIn("playgroup_pending_sync_queue(", refresh)
        self.assertIn("parse_playgroup_object", refresh)
        self.assertIn("return cached_playgroup_count > 0;", refresh)
        self.assertIn('strcmp(line, "PG SYNC PENDING") == 0', API)
        self.assertIn("playgroup_pending_sync_queue();", API)

    def test_image_detaches_before_freeing_cache(self):
        clear = between(PREGAME, "static void clear_deck_art(void)\n{",
                        "\nstatic void deck_art_timer_cb(lv_timer_t *timer)")
        self.assertIn("lv_img_set_src(deck_image, NULL);", clear)
        member = between(PREGAME, "static void event_member_select(lv_event_t *e)\n{",
                         "\nstatic void clear_deck_art(void)")
        self.assertLess(member.index("clear_deck_art();"),
                        member.index("clear_decoded_art_cache();"))

    def test_five_popular_decks_and_more_decks(self):
        self.assertIn("#define INITIAL_DECK_LIMIT 5", PREGAME)
        self.assertIn("return initial_deck_count() + 1;", PREGAME)
        self.assertIn("deck_picker_show_all = true;", PREGAME)

    def test_background_jpegs_prefetch_only_first_five(self):
        self.assertIn("#define PG_ART_PREFETCH_TOP_DECKS 5U", API)
        refresh = between(API, "bool playgroup_refresh_decks(long user_id)\n{",
                          "\nint playgroup_cached_deck_count(void)")
        self.assertIn("playgroup_prefetch_deck_images();", refresh)
        scheduler = between(API, "void playgroup_prefetch_deck_images(void)\n{",
                            "\nvoid playgroup_free_image(uint8_t *data)")
        self.assertIn("job->count=PG_ART_PREFETCH_TOP_DECKS;", scheduler)
        self.assertIn("job->urls[i]", scheduler)
        self.assertIn("art_prefetch_queue_mutex", scheduler)
        worker = between(API, "static void playgroup_art_prefetch_task(void *param)\n{",
                         "\nstatic void playgroup_single_art_prefetch_task(void *param)")
        self.assertIn("art_persistent_valid(job->ids[i])", worker)
        self.assertIn("art_persistent_store(job->ids[i],data,bytes)", worker)
        self.assertIn("playgroup_download_deck_image(job->urls[i]", worker)
        self.assertNotIn("commander_image_decode", worker)

    def test_flash_cache_never_formats_or_exhausts_profile_space(self):
        self.assertNotIn("SPIFFS.begin(true)", API)
        self.assertIn("#define PG_ART_PERSIST_RESERVE (512U * 1024U)", API)
        self.assertIn("const size_t needed = size + sizeof(header) + PG_ART_PERSIST_RESERVE;", API)
        self.assertIn("PG_ART_CACHE_SLOTS 3", API)
        self.assertIn("Art TLS cleanup deferred", API)
        self.assertIn("art_persistent_store(job->scryfall_id,data,size)", API)

if __name__ == "__main__":
    unittest.main()
