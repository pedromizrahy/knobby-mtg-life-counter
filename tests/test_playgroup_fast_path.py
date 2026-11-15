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

    def test_evolution_preview_is_authenticated_and_read_only(self):
        preview = (ROOT / "knobby/src/pokemon_playgroup_preview.cpp").read_text(encoding="utf-8")
        ui = (ROOT / "knobby/src/pokemon_evolution_ui.cpp").read_text(encoding="utf-8")
        self.assertIn('pk_evolution_ui_open((uint32_t)id)',preview)
        self.assertIn('if(participants[index].has_main)',preview)
        self.assertIn('pk_starter_ui_open((uint32_t)id)',preview)
        self.assertIn('original_seats[index].user_id',preview)
        self.assertIn('playgroup_authenticated_user_id(&authenticated)',ui)
        self.assertIn('authenticated!=expected_user_id',ui)
        self.assertIn('pk_file_inspect(authenticated,&profile)',ui)
        self.assertIn('pk_evo_options(mon.species,routes,12)',ui)
        self.assertIn('pk_item_store_inspect(authenticated,&inventory,NULL)',ui)
        self.assertIn('pk_pg_preview_resume()',ui)
        self.assertIn('READ-ONLY | NO ITEMS SPENT',ui)
        self.assertNotIn('pk_file_save(',ui)
        self.assertNotIn('pk_item_store_update(',ui)
        self.assertNotIn('pk_item_reward_claim_',ui)
        self.assertNotIn('pk_evo_prepare_level(',ui)
        from math import hypot
        for x in (100,176,184,260):
            self.assertLess(hypot(x-180,335-180),180)

    def test_roster_short_tap_edits_only_selected_seat(self):
        src=(ROOT / "knobby/src/pregame.c").read_text(encoding="utf-8")
        handler=between(src,"static void event_roster_member_cycle(lv_event_t *e)\n{",
                        "\nstatic bool member_index_used_by_other_seat(")
        self.assertIn("member_picker_seat=seat;",handler)
        self.assertIn("roster_edit_mode=true;",handler)
        self.assertIn("lv_scr_load(screen_pregame_member);",handler)
        self.assertNotIn("selected_deck_id[seat] = 0;",handler)
        self.assertNotIn("candidate = (candidate + 1)",handler)
        self.assertIn("if (roster_edit_mode)",src)
        self.assertIn("roster_edit_has_pending_member",src)
        self.assertIn("roster_edit_original_deck_id",src)
        self.assertIn('Tap: edit player | Hold: decks',src)
        self.assertIn('roster_edit_decks_preloaded=true;',src)
        self.assertIn('if (!roster_edit_decks_preloaded &&',src)
        self.assertIn('roster_edit_decks_preloaded=false;',src)
        direct=between(src,"static void event_roster_open_decks(lv_event_t *e)\n{",
                       "\nstatic void event_deck_select(lv_event_t *e)")
        self.assertIn("roster_edit_mode=true;",direct)
        self.assertIn("member_picker_seat=seat;",direct)
        self.assertIn("event_roster_open_decks,",src)
        self.assertIn("LV_EVENT_LONG_PRESSED,",src)

    def test_multiplayer_hold_has_direct_life_adjust_and_more_menu(self):
        mp = (ROOT / "knobby/src/ui_mp.c").read_text(encoding="utf-8")
        menu = (ROOT / "knobby/src/ui_player_menu.c").read_text(encoding="utf-8")
        self.assertIn("open_player_life_adjust(player);", mp)
        self.assertIn("open_player_life_adjust(int player_index)", menu)
        self.assertIn("apply_life_delta(menu_player, life_edit_delta)", menu)
        self.assertIn('life_edit_more(', menu)
        self.assertIn('life_edit_cancel(', menu)
        self.assertIn('build_player_life_adjust_screen();', menu)

    def test_playgroup_turn_sync_reports_missing_timeline(self):
        pending=(ROOT / "knobby/src/playgroup_pending.cpp").read_text(encoding="utf-8")
        self.assertIn('Timeline snapshot:',pending)
        self.assertIn('Timeline import:',pending)
        self.assertIn('result can sync without turn times',pending)
        self.assertIn('full damage/turn history exceeds snapshot capacity',pending)
        self.assertIn('next_turn_player(snapshot,i)>=0',pending)
        self.assertNotIn('elapsed + event->duration_ms > max_elapsed',pending)
        self.assertIn('"PassTurn"',pending)

    def test_hold_life_editor_routes_knob_and_uses_project_fonts(self):
        knob=(ROOT / "knobby/knob.c").read_text(encoding="utf-8")
        menu=(ROOT / "knobby/src/ui_player_menu.c").read_text(encoding="utf-8")
        header=(ROOT / "knobby/src/ui_player_menu.h").read_text(encoding="utf-8")
        self.assertIn('lv_scr_act() == screen_player_life_adjust',knob)
        self.assertIn('player_life_adjust_change(-1)',knob)
        self.assertIn('player_life_adjust_change(+1)',knob)
        self.assertIn('screen == screen_player_life_adjust',knob)
        self.assertIn('screen_player_menu, screen_player_life_adjust',knob)
        self.assertIn('extern lv_obj_t *screen_player_life_adjust;',header)
        self.assertIn('void player_life_adjust_change(int delta);',header)
        self.assertIn('&lv_font_montserrat_bold_56',menu)
        self.assertIn('&lv_font_montserrat_22',menu)
        self.assertIn('Turn dial to change life',menu)
        self.assertIn('apply_life_delta(menu_player, life_edit_delta)',menu)
        self.assertIn('life_edit_delta = 0;',menu)

    def test_elimination_requires_confirmation_and_shows_bundled_skull(self):
        game=(ROOT / "knobby/src/game.c").read_text(encoding="utf-8")
        mp=(ROOT / "knobby/src/ui_mp.c").read_text(encoding="utf-8")
        menu=(ROOT / "knobby/src/ui_player_menu.c").read_text(encoding="utf-8")
        knob=(ROOT / "knobby/knob.c").read_text(encoding="utf-8")
        image=(ROOT / "knobby/src/dial_skull_icon.c").read_text(encoding="utf-8")
        self.assertIn('player_elimination_pending[player]=true',game)
        self.assertIn('player_eliminated[player]=false',game)
        self.assertIn('confirm_pending_player_elimination(int player)',game)
        self.assertIn('cancel_pending_player_elimination(int player)',game)
        self.assertIn('undo_elimination_action(player)',game)
        self.assertIn('player_elimination_pending[p] = false;',game)
        self.assertIn('mp_refresh_skull(i,spec->player_index)',mp)
        self.assertIn('lv_img_set_src(mp_state.skull_images[i], &dial_skull_icon)',mp)
        self.assertIn('LV_IMG_CF_TRUE_COLOR_ALPHA',image)
        self.assertIn('poll_player_elimination_confirm();',knob)
        self.assertIn('confirm_pending_player_elimination(player)',menu)
        self.assertIn('cancel_pending_player_elimination(player)',menu)
        self.assertIn('next_pending_player_elimination()',menu)
        self.assertIn('screen_player_elimination_confirm',knob)
        self.assertIn('player_life_adjust_change(-1)',knob)

    def test_elimination_exit_turn_advance_and_direction_colors(self):
        menu=(ROOT / "knobby/src/ui_player_menu.c").read_text(encoding="utf-8")
        knob=(ROOT / "knobby/knob.c").read_text(encoding="utf-8")
        game=(ROOT / "knobby/src/game.c").read_text(encoding="utf-8")
        timer=(ROOT / "knobby/src/timer.c").read_text(encoding="utf-8")
        timer_h=(ROOT / "knobby/src/timer.h").read_text(encoding="utf-8")
        mp=(ROOT / "knobby/src/ui_mp.c").read_text(encoding="utf-8")
        skull=(ROOT / "knobby/src/dial_skull_icon.c").read_text(encoding="utf-8")
        self.assertIn('event_eliminated_close(',menu)
        self.assertIn('"BACK",',menu)
        self.assertIn('"UNDO",',menu)
        self.assertIn('screen == screen_eliminated_player_menu',knob)
        self.assertIn('turn_advance_after_elimination(player);',game)
        self.assertIn('void turn_advance_after_elimination(int eliminated_player)',timer_h)
        self.assertIn('active_turn_player != eliminated_player',timer)
        self.assertIn('if (has_survivor)',timer)
        self.assertIn('turn_advance();',timer)
        self.assertIn('mp_delta_color(pending_life_delta)',mp)
        self.assertIn('0x06D6A0',mp)
        self.assertIn('0xFF4A64',mp)
        self.assertIn('LV_IMG_CF_TRUE_COLOR_ALPHA',skull)
        self.assertIn('Anti-aliased skull art',skull)

    def test_commander_art_retry_and_late_selected_seat_recovery(self):
        pre=(ROOT / "knobby/src/pregame.c").read_text(encoding="utf-8")
        api=(ROOT / "knobby/src/playgroup_api.cpp").read_text(encoding="utf-8")
        self.assertIn("static void selected_art_timer_cb(lv_timer_t *timer)",pre)
        self.assertIn("selected_art_id[deck_picker_seat]",pre)
        self.assertIn("selected_art_url[deck_picker_seat]",pre)
        self.assertIn("playgroup_cached_image_copy(selected_art_id[seat]",pre)
        self.assertIn("set_selected_player_art(seat,entry)",pre)
        self.assertIn("selected_art_timer = lv_timer_create",pre)
        self.assertIn("ALL decks need the delayed cache poll",pre)
        self.assertIn("schedule_deck_art();",pre)
        self.assertIn("for (int attempt=1;attempt<=3;attempt++)",api)
        self.assertIn("art_image_header_valid(data,received)",api)
        self.assertIn('not JPEG/PNG; refusing cache',api)
        self.assertIn("art_persistent_load(scryfall_id",api)
        self.assertIn("art_persistent_store(job->scryfall_id,data,size)",api)
        self.assertIn("PG_ART_CACHE_MAX_BYTES (512U * 1024U)",api)

    def test_starter_ui_authenticates_seat_and_persists_before_reveal(self):
        ui = (ROOT / "knobby/src/pokemon_starter_ui.cpp").read_text(encoding="utf-8")
        preview = (ROOT / "knobby/src/pokemon_playgroup_preview.cpp").read_text(encoding="utf-8")
        service = (ROOT / "knobby/src/pokemon_starter_service.cpp").read_text(encoding="utf-8")
        self.assertIn("playgroup_authenticated_user_id(&verified)", ui)
        self.assertIn("verified!=expected_id", ui)
        self.assertIn("pk_starter_service_open(", ui)
        self.assertIn("pk_starter_service_reroll(", ui)
        self.assertIn("pk_starter_service_keep(", ui)
        self.assertIn("if(active && sprite_visible)", ui)
        self.assertIn("pk_starter_service_open(", service)
        self.assertIn("pk_starter_store_create(", service)
        self.assertIn("pk_starter_store_update(", service)
        self.assertIn("pk_file_create_selected_starter(", service)
        self.assertIn("lv_obj_add_event_cb(c.root,start_card,LV_EVENT_CLICKED,", preview)
        self.assertIn("original_seats[index].user_id", preview)
        self.assertIn("pk_pg_preview_resume();", ui)
        from math import hypot
        for x in (90,172,188,270):
            self.assertLess(hypot(x-180,329-180),180)

    def test_starter_round_layout_and_rarity_colors(self):
        ui = (ROOT / "knobby/src/pokemon_starter_ui.cpp").read_text(encoding="utf-8")
        self.assertIn('label("STARTER POKEMON",27,22', ui)
        self.assertIn('label("POKE BALL  |  REROLLS 5/5",59,14', ui)
        self.assertIn('button("BACK",144,85,72,28', ui)
        self.assertIn('species=label("",231,22', ui)
        self.assertIn('tier=label("",260,14', ui)
        self.assertIn('status=label("VERIFYING PLAYGROUP ACCOUNT...",283,14', ui)
        self.assertIn('keep_btn=button("KEEP",100,307,76,26', ui)
        self.assertIn('reroll_btn=button("REROLL",184,307,76,26', ui)
        self.assertIn('lv_obj_set_height(o,size==22?29:21)', ui)
        self.assertIn('animate_rarity();', ui)
        self.assertIn('lv_anim_set_ready_cb(&fade,reveal_done)', ui)
        self.assertIn('0xFFFFFF', ui)
        self.assertIn('0x4CBAFF', ui)
        self.assertIn('0xC784FF', ui)
        self.assertIn('0xFF9C40', ui)
        self.assertIn('lv_obj_set_style_text_color(tier,lv_color_hex(rarity_color', ui)
        self.assertIn('lv_anim_set_values(&pop,170,sprite_zoom_target)', ui)
        self.assertIn('if(!ready || animating || reveal_fx || state.kept', ui)
        from math import hypot
        for x in (100,176,184,260):
            self.assertLess(hypot(x-180,333-180),180)
        self.assertLess(85+28,118)

    def test_owner_only_starter_test_reset(self):
        api = API
        store = (ROOT / "knobby/src/pokemon_starter_store.cpp").read_text(encoding="utf-8")
        self.assertIn('PK TEST STARTER RESET 184199 CONFIRM',api)
        self.assertIn('playgroup_authenticated_user_id(&verified)',api)
        self.assertIn('verified!=184199U',api)
        self.assertIn('pk_file_inspect(verified,NULL)!=PK_STORE_MISSING',api)
        self.assertIn('pk_store_inspect(verified,NULL)!=PK_STORE_MISSING',api)
        self.assertIn('current.kept',api)
        self.assertIn('pk_starter_store_reset_test(verified)',api)
        self.assertIn('if(id!=184199U',store)
        self.assertIn('record.state.kept',store)
        self.assertNotIn('SPIFFS.format(',store)

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
