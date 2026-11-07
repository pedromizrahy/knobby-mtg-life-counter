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

    def test_round_360_roster_buttons_fit_without_covering_last_player(self):
        # Physical 360x360 dial, center 180,180, R=180. Restrict each
        # corner to R-5 to avoid cutting off the curved display edge.
        from math import hypot
        buttons = [(75, 167, 284, 320), (179, 285, 284, 320)]
        for x1,x2,y1,y2 in buttons:
            for x in (x1,x2):
                for y in (y1,y2):
                    self.assertLessEqual(hypot(x-180, y-180), 175.01)
        self.assertEqual(buttons[1][0]-buttons[0][1],12)
        # For 4 and 6 seats, the final roster card ends at y=279.
        for seats,h,g in ((4,46,4),(6,30,3)):
            total = seats*h+(seats-1)*g
            top=max(80,84+(194-total)//2)
            last_bottom=top+(seats-1)*(h+g)+h
            self.assertLess(last_bottom,284)
        self.assertIn("lv_obj_set_size(roster_pokemon_button,92,36);",PREGAME)
        self.assertIn("lv_obj_set_size(roster_continue_button,106,36);",PREGAME)
        self.assertIn("lv_obj_align(roster_continue_button,LV_ALIGN_BOTTOM_MID,52,-40);",PREGAME)
        self.assertIn("lv_obj_align(roster_pokemon_button,LV_ALIGN_BOTTOM_MID,-59,-40);",PREGAME)

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

    def test_companion_preview_has_one_back_and_real_level(self):
        preview = (ROOT / "knobby/src/pokemon_playgroup_preview.cpp").read_text(encoding="utf-8")
        self.assertIn("lv_obj_add_flag(next_button,LV_OBJ_FLAG_HIDDEN);",preview)
        self.assertIn("lv_obj_set_pos(back_button,124,306);",preview)
        self.assertIn('back_button=make_button("BACK",93,112,exit_view);',preview)
        self.assertIn('next_button=make_button("NEXT >",215,85,next_page);',preview)
        self.assertIn("lv_obj_set_size(back_button,78,30);",preview)
        self.assertIn("lv_obj_set_pos(back_button,91,306);",preview)
        self.assertIn("lv_obj_set_pos(next_button,191,306);",preview)
        from math import hypot
        for x in (91,169,191,269):
            self.assertLess(hypot(x-180,336-180),180)

        self.assertIn("view.level",preview)
        self.assertIn('"LV %u SHINY"',preview)
        self.assertIn('"LV %u MAIN"',preview)
        self.assertNotIn('pages>1U?"NEXT  >":"BACK"',preview)

    def test_account_startup_has_no_implicit_starter_creation(self):
        route = between(API, "static void handle_command(char *line)\n{",
                        "\nvoid playgroup_process_serial(void)")
        self.assertIn('strcmp(line,"PK STARTER ME")==0', route)
        self.assertIn('strcmp(line, "PK ME")', route)
        self.assertIn("pokemon_me_account(", route)
        f = between(API, "static void pokemon_me_account(bool create_starter)\n{",
                    "\nstatic void print_status(void)")
        self.assertIn('playgroup_https_get("/me",response,http_status)', f)
        self.assertIn("!pk_player_id_valid(raw)", f)
        self.assertIn("pk_onboard_decide(raw,f,n)", f)
        self.assertIn("if(!create_starter)", f)
        self.assertIn("STARTER PAUSED", f)
        self.assertNotIn("pk_file_create_starter(", f)
        self.assertIn("PK_ONBOARD_READY", API)
        self.assertNotIn("pk_store_create_starter(", f)
        self.assertNotIn("pk_file_import_if_missing(", f)

if __name__ == "__main__":
    unittest.main()
