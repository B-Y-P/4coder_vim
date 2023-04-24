
// TODO(BYP): Might want meta-data on these, for now I prefer the simplicity
function void vim_map_set_binding(u32 mode, u32 sub_mode, void *func, u64 key){
	if((mode & bitmask_3) == 0){
		mode |= bitmask_3;
		foreach(s,VIM_SUBMODE_COUNT){ vim_map_set_binding(mode, s, func, key); }
	}
	if(mode & bit_1){ table_insert(vim_maps + 0 + sub_mode*VIM_MODE_COUNT, key, PtrAsInt(func)); }
	if(mode & bit_2){ table_insert(vim_maps + 1 + sub_mode*VIM_MODE_COUNT, key, PtrAsInt(func)); }
	if(mode & bit_3){ table_insert(vim_maps + 2 + sub_mode*VIM_MODE_COUNT, key, PtrAsInt(func)); }
}
function void VimBind(u32 mode, Custom_Command_Function *custom, Vim_Sub_Mode sub_mode, u64 key){
	vim_map_set_binding(mode, sub_mode, (void *)custom, key);
}
function void VimBind(u32 mode, Custom_Command_Function *custom, u64 key){
	vim_map_set_binding(mode, SUB_None, (void *)custom, key);
}


function void vim_default_bindings(Application_Links *app, Key_Code leader){

	// Normal, Insert, Visual
	// These could be local variables but I like the highlighting
#define N bit_1
#define I bit_2
#define V bit_3
#define MAP 0
	u32 Ctl = KeyMod_Ctl;
	u32 Sft = KeyMod_Sft;
	u32 Alt = KeyMod_Alt;

	VimBind(MAP, vim_normal_mode,                          KeyCode_Escape);
	VimBind(MAP, vim_inc_buffer_peek,                 (Ctl|KeyCode_RightBracket));
	VimBind(MAP, vim_dec_buffer_peek,                 (Ctl|KeyCode_LeftBracket));
	VimBind(MAP, vim_toggle_show_buffer_peek,         (Ctl|KeyCode_Return));
	VimBind(MAP, vim_toggle_show_buffer_peek,     (Ctl|Sft|KeyCode_Return));
	VimBind(MAP, vim_scoll_buffer_peek_up,        (Ctl|Sft|KeyCode_U));
	VimBind(MAP, vim_scoll_buffer_peek_down,      (Ctl|Sft|KeyCode_D));

	/// Rebinds
	VimBind(N|MAP, undo,                                   KeyCode_U);
	VimBind(N|MAP, undo,                              (Ctl|KeyCode_Z));
	VimBind(N|MAP, redo,                              (Ctl|KeyCode_R));
	VimBind(N|V|MAP, set_mark,                        (Ctl|KeyCode_Space));
	VimBind(N|MAP, save,                              (Ctl|KeyCode_S));
	VimBind(N|MAP, vim_open_file_in_quotes,         SUB_G, KeyCode_F);
	VimBind(N|MAP, vim_interactive_open_or_new,   (Ctl|Sft|KeyCode_O));
	VimBind(N|MAP, quick_swap_buffer,             (Sft|Ctl|KeyCode_6));
	VimBind(N|MAP, vim_goto_definition,               (Ctl|KeyCode_W));
	VimBind(N|MAP, vim_next_4coder_jump,              (Alt|KeyCode_N));
	VimBind(N|MAP, vim_prev_4coder_jump,          (Sft|Alt|KeyCode_N));
	VimBind(N|MAP, vim_first_4coder_jump,         (Sft|Alt|KeyCode_M));
	VimBind(N|I|MAP, view_buffer_other_panel,         (Ctl|KeyCode_1));
	VimBind(N|I|MAP, swap_panels,                     (Ctl|KeyCode_2));
	VimBind(I|MAP, word_complete_drop_down,           (Ctl|KeyCode_N));
	//VimBind(I|MAP, word_complete_drop_down,           (Ctl|KeyCode_P));

	/// Mode Binds
	VimBind(N|V|MAP, vim_modal_i,                          KeyCode_I);
	VimBind(N|V|MAP, vim_modal_a,                          KeyCode_A);
	VimBind(N|V|MAP, vim_command_mode,                (Sft|KeyCode_Semicolon));
	VimBind(N|MAP,   vim_insert_begin,                (Sft|KeyCode_I));
	VimBind(N|MAP,   vim_insert_end,                  (Sft|KeyCode_A));
	VimBind(N|MAP,   vim_replace_mode,                (Sft|KeyCode_R));
	VimBind(N|V|MAP, vim_visual_mode,                      KeyCode_V);
	VimBind(N|V|MAP, vim_visual_mode,                 (Sft|KeyCode_V));
	VimBind(N|V|MAP, vim_visual_mode,                 (Ctl|KeyCode_V));
	VimBind(N|MAP,   vim_prev_visual,               SUB_G, KeyCode_V);
	VimBind(N|MAP,   vim_newline_below,                    KeyCode_O);
	VimBind(N|MAP,   vim_newline_above,               (Sft|KeyCode_O));
	VimBind(V|MAP,   vim_visual_insert,               (Sft|KeyCode_A));
	VimBind(V|MAP,   vim_visual_insert,               (Sft|KeyCode_I));
	VimBind(I|MAP,   vim_insert_command,          (Ctl|Sft|KeyCode_O));

	/// Sub Mode Binds
	VimBind(N|V|MAP, vim_submode_g,                        KeyCode_G);
	VimBind(N|V|MAP, vim_submode_z,                        KeyCode_Z);
	VimBind(N|V|MAP, vim_submode_leader,                   leader);

	/// Request Binds
	VimBind(N|V|MAP, vim_request_yank,                     KeyCode_Y);
	VimBind(N|V|MAP, vim_request_delete,                   KeyCode_D);
	VimBind(N|V|MAP, vim_request_change,                   KeyCode_C);
	VimBind(N|V|MAP, vim_delete_end,                  (Sft|KeyCode_D));
	VimBind(N|V|MAP, vim_change_end,                  (Sft|KeyCode_C));
	VimBind(N|V|MAP, vim_yank_end,                    (Sft|KeyCode_Y));
	VimBind(N|V|MAP, vim_request_auto_indent,              KeyCode_Equal);
	VimBind(N|V|MAP, vim_lowercase,            SUB_G,      KeyCode_U);
	VimBind(N|V|MAP, vim_uppercase,            SUB_G, (Sft|KeyCode_U));
	VimBind(N|V|MAP, vim_toggle_case,          SUB_G, (Sft|KeyCode_Tick));
	VimBind(N|V|MAP, vim_request_indent,              (Sft|KeyCode_Period));
	VimBind(N|V|MAP, vim_request_outdent,             (Sft|KeyCode_Comma));
	VimBind(N|V|MAP, vim_request_fold,              SUB_Z, KeyCode_F);
	VimBind(N|V|MAP, fold_toggle_cursor,            SUB_Z, KeyCode_A);
	VimBind(N|V|MAP, fold_pop_cursor,               SUB_Z, KeyCode_D);
	VimBind(V|MAP,   vim_toggle_case,                 (Sft|KeyCode_Tick));
	VimBind(V|MAP,   vim_lowercase,                        KeyCode_U);
	VimBind(V|MAP,   vim_uppercase,                   (Sft|KeyCode_U));
	VimBind(V|MAP,   vim_replace_range_next,               KeyCode_R);

	/// Edit Binds
	VimBind(N|MAP,   vim_paste_before,               (Sft|KeyCode_P));
	VimBind(N|MAP,   vim_paste_after,                     KeyCode_P);
	VimBind(N|MAP,   vim_backspace_char,             (Sft|KeyCode_X));
	VimBind(N|MAP,   vim_delete_char,                     KeyCode_X);
	VimBind(N|MAP,   vim_replace_next_char,               KeyCode_R);
	VimBind(N|V|MAP, vim_combine_line,                (Sft|KeyCode_J));
	VimBind(N|V|MAP, vim_combine_line,         SUB_G, (Sft|KeyCode_J));
	VimBind(N|MAP,   vim_last_command,                     KeyCode_Period);
	VimBind(N|V|MAP, vim_select_register,             (Sft|KeyCode_Quote));
	VimBind(N|MAP,   vim_toggle_char,                 (Sft|KeyCode_Tick));
	VimBind(I|MAP,   vim_select_register,             (Ctl|KeyCode_R));
	VimBind(I|MAP,   vim_delete_to_begin,             (Ctl|KeyCode_U));
	VimBind(V|MAP,   vim_move_selection_up,           (Alt|KeyCode_K));
	VimBind(V|MAP,   vim_move_selection_down,         (Alt|KeyCode_J));

	/// Digit Binds
	VimBind(N|V|MAP, vim_modal_0,                          KeyCode_0);
	VimBind(N|V|MAP, vim_digit,                            KeyCode_1);
	VimBind(N|V|MAP, vim_digit,                            KeyCode_2);
	VimBind(N|V|MAP, vim_digit,                            KeyCode_3);
	VimBind(N|V|MAP, vim_digit,                            KeyCode_4);
	VimBind(N|V|MAP, vim_digit,                            KeyCode_5);
	VimBind(N|V|MAP, vim_digit,                            KeyCode_6);
	VimBind(N|V|MAP, vim_digit,                            KeyCode_7);
	VimBind(N|V|MAP, vim_digit,                            KeyCode_8);
	VimBind(N|V|MAP, vim_digit,                            KeyCode_9);
	VimBind(N|V|MAP, vim_digit_del,                        KeyCode_Backspace);
	VimBind(N|V|MAP, vim_digit_del,                        KeyCode_Delete);

	/// Movement Binds
	VimBind(N|V|MAP, vim_left,                             KeyCode_H);
	VimBind(N|V|MAP, vim_down,                             KeyCode_J);
	VimBind(N|V|MAP, vim_up,                               KeyCode_K);
	VimBind(N|V|MAP, vim_right,                            KeyCode_L);
	VimBind(N|V|MAP, vim_end_line,                    (Sft|KeyCode_4));
	VimBind(N|V|MAP, vim_begin_line,                  (Sft|KeyCode_6));
	VimBind(N|V|MAP, vim_forward_word,                     KeyCode_W);
	VimBind(N|V|MAP, vim_forward_WORD,                (Sft|KeyCode_W));
	VimBind(N|V|MAP, vim_backward_word,                    KeyCode_B);
	VimBind(N|V|MAP, vim_backward_WORD,               (Sft|KeyCode_B));
	VimBind(N|V|MAP, vim_forward_end,                      KeyCode_E);
	VimBind(N|V|MAP, vim_forward_END,                 (Sft|KeyCode_E));
	VimBind(N|V|MAP, vim_backward_end,         SUB_G,      KeyCode_E);
	VimBind(N|V|MAP, vim_backward_END,         SUB_G, (Sft|KeyCode_E));

	VimBind(N|V|MAP, vim_file_top,             SUB_G,     KeyCode_G);
	VimBind(N|V|MAP, vim_goto_line,                   (Sft|KeyCode_G));
	VimBind(N|V|MAP, vim_goto_column,                 (Sft|KeyCode_BackwardSlash));
	VimBind(N|V|MAP, vim_modal_percent,               (Sft|KeyCode_5));
	VimBind(N|V|MAP, vim_bounce,                      (Ctl|KeyCode_5));
	VimBind(N|V|MAP, vim_set_seek_char,                    KeyCode_F);
	VimBind(N|V|MAP, vim_set_seek_char,                    KeyCode_T);
	VimBind(N|V|MAP, vim_set_seek_char,               (Sft|KeyCode_F));
	VimBind(N|V|MAP, vim_set_seek_char,               (Sft|KeyCode_T));
	VimBind(N|V|MAP, vim_seek_char_forward,                KeyCode_Semicolon);
	VimBind(N|V|MAP, vim_seek_char_backward,               KeyCode_Comma);
	VimBind(N|V|MAP, vim_paragraph_up,                (Sft|KeyCode_LeftBracket));
	VimBind(N|V|MAP, vim_paragraph_down,              (Sft|KeyCode_RightBracket));
	VimBind(N|V|MAP, vim_screen_top,                  (Sft|KeyCode_H));
	VimBind(N|V|MAP, vim_screen_bot,                  (Sft|KeyCode_L));
	VimBind(N|V|MAP, vim_screen_mid,                  (Sft|KeyCode_M));
	VimBind(V|MAP,   cursor_mark_swap,                     KeyCode_O);
	VimBind(V|MAP,   vim_block_swap,                  (Sft|KeyCode_O));

	VimBind(N|MAP, vim_search_identifier,         (Ctl|Sft|KeyCode_8));
	VimBind(N|MAP, vim_search_identifier,             (Sft|KeyCode_8));
	VimBind(N|MAP, vim_clear_search,          SUB_Leader,  KeyCode_Space);
	VimBind(N|MAP, vim_start_search_forward,               KeyCode_ForwardSlash);
	VimBind(N|MAP, vim_start_search_backward,         (Sft|KeyCode_ForwardSlash));
	VimBind(N|MAP, vim_to_next_pattern,                    KeyCode_N);
	VimBind(N|MAP, vim_to_prev_pattern,               (Sft|KeyCode_N));
	VimBind(N|MAP, vim_in_next_pattern,        SUB_G,      KeyCode_N);
	VimBind(N|MAP, vim_in_prev_pattern,        SUB_G, (Sft|KeyCode_N));

	VimBind(N|MAP, vim_prev_jump,                     (Ctl|KeyCode_O));
	VimBind(N|MAP, vim_next_jump,                     (Ctl|KeyCode_I));
	VimBind(N|MAP, vim_next_jump,                     (Sft|KeyCode_I));
	//VimBind(N|MAP, vim_next_jump,                 (Ctl|Sft|KeyCode_I));

	/// Screen Adjust Binds
	VimBind(N|V|MAP, vim_half_page_up,                (Ctl|KeyCode_U));
	VimBind(N|V|MAP, vim_half_page_down,              (Ctl|KeyCode_D));
	VimBind(N|V|MAP, vim_whole_page_up,               (Ctl|KeyCode_B));
	VimBind(N|V|MAP, vim_whole_page_down,             (Ctl|KeyCode_F));
	VimBind(N|V|MAP, vim_line_down,               (Ctl|Sft|KeyCode_Y));
	VimBind(N|V|MAP, vim_line_down,                   (Ctl|KeyCode_Y));
	VimBind(N|V|MAP, vim_line_up,                 (Ctl|Sft|KeyCode_E));
	VimBind(N|V|MAP, vim_line_up,                     (Ctl|KeyCode_E));
	VimBind(N|V|MAP, vim_scroll_screen_top,         SUB_Z, KeyCode_T);
	VimBind(N|V|MAP, vim_scroll_screen_mid,         SUB_Z, KeyCode_Z);
	VimBind(N|V|MAP, vim_scroll_screen_bot,         SUB_Z, KeyCode_B);

	/// Miscellaneous Binds
	VimBind(N|V|MAP, vim_set_mark,                         KeyCode_M);
	VimBind(N|V|MAP, vim_goto_mark,                        KeyCode_Tick);
	VimBind(N|V|MAP, vim_goto_mark,                        KeyCode_Quote);
	VimBind(N|V|MAP, vim_toggle_macro,                     KeyCode_Q);
	VimBind(N|V|MAP, vim_play_macro,                  (Sft|KeyCode_2));

	/// Window Binds (TODO(BYP): Do the rest of these, and do them properly)
	VimBind(N|MAP, change_active_panel_backwards,     (Ctl|KeyCode_H));
	VimBind(N|MAP, change_active_panel_backwards,     (Ctl|KeyCode_J));
	VimBind(N|MAP, change_active_panel,               (Ctl|KeyCode_K));
	VimBind(N|MAP, change_active_panel,               (Ctl|KeyCode_L));


#undef I
#undef N
#undef V
#undef MAP
}
