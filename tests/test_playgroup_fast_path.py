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

if __name__ == "__main__":
    unittest.main()
