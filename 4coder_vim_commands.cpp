

CUSTOM_COMMAND_SIG(vim_toggle_relative_line_num)
CUSTOM_DOC("Toggles relative line numbers")
{ vim_relative_numbers ^= 1; }

CUSTOM_COMMAND_SIG(vim_toggle_show_buffer_peek)
CUSTOM_DOC("Toggles buffer peek")
{
	vim_show_buffer_peek ^= 1;
	f32 screen_height = rect_height(global_get_screen_rectangle(app));
	f32 height = Min(0.3f, VIM_LISTER_MAX_RATIO)*screen_height;
	Input_Event event = get_current_input(app).event;
	if(event.kind == InputEventKind_KeyStroke && has_modifier(&event, KeyCode_Shift)){
		height = VIM_LISTER_MAX_RATIO*screen_height;
	}

	vim_nxt_filebar_offset = vim_show_buffer_peek*height;
}

CUSTOM_COMMAND_SIG(vim_inc_buffer_peek)
CUSTOM_DOC("Incremenets buffer peek index")
{
	vim_buffer_peek_index = ArrayInc(vim_buffer_peek_list, vim_buffer_peek_index);
	vim_show_buffer_peek = 0;
	vim_toggle_show_buffer_peek(app);
}

CUSTOM_COMMAND_SIG(vim_dec_buffer_peek)
CUSTOM_DOC("Decrements buffer peek index")
{
	vim_buffer_peek_index = ArrayDec(vim_buffer_peek_list, vim_buffer_peek_index);
	vim_show_buffer_peek = 0;
	vim_toggle_show_buffer_peek(app);
}

function void
vim_scoll_buffer_peek_inner(Application_Links *app, f32 ratio){
	Vim_Buffer_Peek_Entry *entry = vim_buffer_peek_list + vim_buffer_peek_index;
	Buffer_ID buffer = buffer_identifier_to_id(app, entry->buffer_id);
	Face_ID face_id = get_face_id(app, 0);
	Face_Metrics metrics = get_face_metrics(app, face_id);
	f32 line_height = metrics.line_height;
	i64 total_lines = buffer_get_line_count(app, buffer);

	Rect_f32 back_rect = vim_get_bottom_rect(app);
	f32 lines_per_page = rect_height(back_rect) / line_height;
	f32 current_line = entry->nxt_ratio*total_lines;
	entry->nxt_ratio = (current_line + ratio*lines_per_page) / total_lines;
	entry->nxt_ratio = clamp(f32(lines_per_page / total_lines), entry->nxt_ratio, 1.f);
}

CUSTOM_COMMAND_SIG(vim_scoll_buffer_peek_up)
CUSTOM_DOC("Scrolls buffer peek up")
{ vim_scoll_buffer_peek_inner(app, -0.75f); }

CUSTOM_COMMAND_SIG(vim_scoll_buffer_peek_down)
CUSTOM_DOC("Scrolls buffer peek down")
{ vim_scoll_buffer_peek_inner(app,  0.75f); }

CUSTOM_COMMAND_SIG(right_adjust_view)
CUSTOM_DOC("Sets the right size of the view near the x position of the cursor.")
{
	View_ID view = get_active_view(app, Access_ReadVisible);
	Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);
	i64 pos = view_get_cursor_pos(app, view);
	if(!line_is_valid_and_blank(app, buffer, get_line_number_from_pos(app, buffer, pos))){
		i64 new_pos = get_line_side_pos_from_pos(app, buffer, pos, Side_Min);
		if(character_is_whitespace(buffer_get_char(app, buffer, new_pos))){
			new_pos = buffer_seek_character_class_change_1_0(app, buffer, &character_predicate_whitespace, Scan_Forward, new_pos);
		}
		view_set_cursor_and_preferred_x(app, view, seek_pos(new_pos));
	}
	Buffer_Scroll scroll = view_get_buffer_scroll(app, view);
	scroll.target.pixel_shift.x = 0.f;
	view_set_buffer_scroll(app, view, scroll, SetBufferScroll_NoCursorChange);
}

#if VIM_USE_REIGSTER_BUFFER
CUSTOM_COMMAND_SIG(reg)
CUSTOM_DOC("Vim: Display registers"){
	vim_show_buffer_peek = 0;
	vim_buffer_peek_index = 1;
	view_enqueue_command_function(app, get_active_view(app, Access_ReadVisible), vim_toggle_show_buffer_peek);
}
#endif


VIM_COMMAND_SIG(vim_normal_mode){
	if(vim_state.mode == VIM_Insert){
		vim_set_insert_register(app);
		View_ID view = get_active_view(app, Access_ReadVisible);
		Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);
		const i32 N = Max(0,vim_state.params.number-1);
		foreach(i, N){
			vim_paste(app, view, buffer, &vim_registers.insert);
		}
		move_horizontal_lines(app, -1);
	}
	if(vim_state.mode == VIM_Visual){
		vim_set_prev_visual(app, get_active_view(app, Access_ReadVisible));
	}
	vim_reset_state();
}


VIM_COMMAND_SIG(vim_insert_mode_after){
	View_ID view = get_active_view(app, Access_ReadVisible);
	Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);
	i64 line = get_line_number_from_pos(app, buffer, view_get_cursor_pos(app, view));
	if(!line_is_valid_and_blank(app, buffer, line)){
		move_right(app);
	}
	vim_enter_insert_mode(app);
}
VIM_COMMAND_SIG(vim_insert_mode){ vim_enter_insert_mode(app); }
VIM_COMMAND_SIG(vim_insert_begin){ vim_begin_line(app); vim_enter_insert_mode(app); }
VIM_COMMAND_SIG(vim_insert_end){ vim_end_line(app); vim_insert_mode_after(app); }

VIM_COMMAND_SIG(vim_modal_i){
	if(vim_state.mode == VIM_Visual || vim_state.params.request != REQUEST_None){
		vim_state.params.clusivity = VIM_Exclusive;
		u8 key = vim_query_user_key(app, string_u8_litexpr("-- TEXT OBJECT --"));
		if(key){
			vim_state.params.seek.character = key;
			vim_state.active_command = vim_text_object;
			vim_text_object(app);
		}
	}
	else{ vim_insert_mode(app); }
}

VIM_COMMAND_SIG(vim_modal_a){
	if(vim_state.mode == VIM_Visual || vim_state.params.request != REQUEST_None){
		vim_state.params.clusivity = VIM_Inclusive;
		u8 key = vim_query_user_key(app, string_u8_litexpr("-- TEXT OBJECT --"));
		if(key){
			vim_state.params.seek.character = key;
			vim_state.active_command = vim_text_object;
			vim_text_object(app);
		}
	}
	else{ vim_insert_mode_after(app); }
}

VIM_COMMAND_SIG(vim_newline_below){
	vim_insert_end(app);
	vim_state.insert_index++;
	write_text(app, string_u8_litexpr("\n"));
}

VIM_COMMAND_SIG(vim_newline_above){
	vim_line_start(app);
	vim_enter_insert_mode(app);
	vim_state.insert_index++;
	write_text(app, string_u8_litexpr("\n"));
	move_vertical_lines(app, -1);
}

VIM_COMMAND_SIG(vim_visual_mode){
	if(vim_state.mode != VIM_Visual){
		set_mark(app);
		vim_state.mode = VIM_Visual;
	}
	vim_state.params.edit_type = EDIT_CharWise;
	Input_Event event = get_current_input(app).event;
	if(event.kind == InputEventKind_KeyStroke){
		if(0){}
		else if(has_modifier(&event, KeyCode_Shift)){   vim_state.params.edit_type = EDIT_LineWise; }
		else if(has_modifier(&event, KeyCode_Control)){ vim_state.params.edit_type = EDIT_Block; }
	}
}

VIM_COMMAND_SIG(vim_prev_visual){
	View_ID view = get_active_view(app, Access_ReadVisible);
	Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);
	Managed_Scope scope = buffer_get_managed_scope(app, buffer);

	Vim_Prev_Visual *prev_visual = scope_attachment(app, scope, vim_buffer_prev_visual, Vim_Prev_Visual);
	if(prev_visual && prev_visual->cursor_pos != 0 && prev_visual->mark_pos != 0){
		view_set_cursor_and_preferred_x(app, view, seek_pos(prev_visual->cursor_pos));
		view_set_mark(app, view, seek_pos(prev_visual->mark_pos));
		vim_state.params.edit_type = prev_visual->edit_type;
		vim_state.mode = VIM_Visual;
	}
}

VIM_COMMAND_SIG(vim_block_swap){
	View_ID view = get_active_view(app, Access_ReadVisible);
	Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);
	i64 c = view_get_cursor_pos(app, view);
	i64 m = view_get_mark_pos(app, view);
	i64 line = get_line_number_from_pos(app, buffer, c);
	Vec2_f32 c_p = view_relative_xy_of_pos(app, view, line, c);
	Vec2_f32 m_p = view_relative_xy_of_pos(app, view, line, m);
	Swap(f32, c_p.x, m_p.x);
	c = view_pos_at_relative_xy(app, view, line, c_p);
	m = view_pos_at_relative_xy(app, view, line, m_p);
	view_set_cursor(app, view, seek_pos(c));
	view_set_mark(app, view, seek_pos(m));
}

VIM_COMMAND_SIG(vim_replace_mode){
	vim_state.mode = VIM_Replace;
	set_mark(app);
	View_ID view = get_active_view(app, Access_ReadVisible);
	Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);
	vim_history_group = history_group_begin(app, buffer);
}

VIM_COMMAND_SIG(vim_visual_insert){
	if(vim_state.params.edit_type == EDIT_Block){
		View_ID view = get_active_view(app, Access_ReadVisible);
		Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);

		vim_visual_insert_inner(app, view, buffer);

		i64 cursor_pos = view_get_cursor_pos(app, view);
		if(line_is_valid_and_blank(app, buffer, get_line_number_from_pos(app, buffer, cursor_pos))){
			auto_indent_buffer(app, buffer, Ii64(cursor_pos));
			view_set_cursor(app, view, seek_pos(get_pos_past_lead_whitespace(app, buffer, cursor_pos)));
		}
		i64 mark_pos = view_get_mark_pos(app, view);
		if(line_is_valid_and_blank(app, buffer, get_line_number_from_pos(app, buffer, mark_pos))){
			auto_indent_buffer(app, buffer, Ii64(mark_pos));
			view_set_mark(app, view, seek_pos(get_pos_past_lead_whitespace(app, buffer, mark_pos)));
		}

		User_Input input = get_current_input(app);
		if(input.event.kind == InputEventKind_KeyStroke && input.event.key.code == KeyCode_A){
			vim_visual_insert_after = true;
		}
	}
}


VIM_COMMAND_SIG(vim_submode_g){ vim_state.sub_mode = SUB_G; vim_state.chord_resolved = false; }
VIM_COMMAND_SIG(vim_submode_z){ vim_state.sub_mode = SUB_Z; vim_state.chord_resolved = false; }
VIM_COMMAND_SIG(vim_submode_leader){ vim_state.sub_mode = SUB_Leader; vim_state.chord_resolved = false; }

VIM_COMMAND_SIG(vim_replace_next_char){
	u8 key = vim_query_user_key(app, string_u8_litexpr("-- REPLACE NEXT --"));
	if(key){
		View_ID view = get_active_view(app, Access_ReadVisible);
		Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);
		i64 pos = view_get_cursor_pos(app, view);
		buffer_replace_range(app, buffer, Ii64(pos, pos+1), SCu8(&key, 1));
	}
}


VIM_COMMAND_SIG(vim_replace_range_next){
	u8 key = vim_query_user_key(app, string_u8_litexpr("-- RANGE REPLACE NEXT --"));
	if(key){
		vim_state.params.seek.character = key;
		vim_make_request(app, REQUEST_Replace);
	}
}

VIM_COMMAND_SIG(vim_request_yank){    vim_make_request(app, REQUEST_Yank); }
VIM_COMMAND_SIG(vim_request_delete){  vim_make_request(app, REQUEST_Delete); }
VIM_COMMAND_SIG(vim_request_change){  vim_make_request(app, REQUEST_Change); }
VIM_COMMAND_SIG(vim_uppercase){       vim_make_request(app, REQUEST_Upper); }
VIM_COMMAND_SIG(vim_lowercase){       vim_make_request(app, REQUEST_Lower); }
VIM_COMMAND_SIG(vim_toggle_case){     vim_make_request(app, REQUEST_ToggleCase); }
VIM_COMMAND_SIG(vim_request_indent){  vim_make_request(app, REQUEST_Indent); }
VIM_COMMAND_SIG(vim_request_outdent){ vim_make_request(app, REQUEST_Outdent); }
VIM_COMMAND_SIG(vim_request_fold){    vim_make_request(app, REQUEST_Fold); }
VIM_COMMAND_SIG(vim_request_auto_indent){ vim_make_request(app, REQUEST_AutoIndent); }

VIM_COMMAND_SIG(vim_toggle_char){
	View_ID view = get_active_view(app, Access_ReadVisible);
	Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);
	i64 pos = view_get_cursor_pos(app, view);
	u8 c = buffer_get_char(app, buffer, pos);
	c = character_toggle_case(c);
	buffer_replace_range(app, buffer, Ii64_size(pos, 1), SCu8(&c, 1));
}

VIM_COMMAND_SIG(vim_delete_end){
	View_ID view = get_active_view(app, Access_ReadVisible);
	Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);
	i64 pos = view_get_cursor_pos(app, view);
	i64 line_num = get_line_number_from_pos(app, buffer, pos);
	if(line_is_valid_and_blank(app, buffer, line_num)){ vim_reset_state(); return; }
	Vim_Motion_Block vim_motion_block(app);
	vim_state.params.request = REQUEST_Delete;
	vim_state.params.clusivity = VIM_Exclusive;
	seek_end_of_line(app);
}
VIM_COMMAND_SIG(vim_delete_to_begin){
	View_ID view = get_active_view(app, Access_ReadVisible);
	Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);
	i64 pos = view_get_cursor_pos(app, view);
	i64 line_num = get_line_number_from_pos(app, buffer, pos);
	if(line_is_valid_and_blank(app, buffer, line_num)){ vim_reset_state(); return; }
	Vim_Motion_Block vim_motion_block(app);
	vim_state.params.request = REQUEST_Delete;
	vim_state.params.clusivity = VIM_Exclusive;
	seek_beginning_of_line(app);
}

VIM_COMMAND_SIG(vim_change_end){
	Vim_Motion_Block vim_motion_block(app);
	vim_state.params.request = REQUEST_Change;
	vim_state.params.clusivity = VIM_Exclusive;
	seek_end_of_line(app);
}

VIM_COMMAND_SIG(vim_yank_end){
	Vim_Motion_Block vim_motion_block(app);
	vim_state.params.request = REQUEST_Yank;
	vim_state.params.clusivity = VIM_Exclusive;
	seek_end_of_line(app);
}


VIM_COMMAND_SIG(vim_leader_d){ vim_state.params.selected_reg=0; vim_request_delete(app); }
VIM_COMMAND_SIG(vim_leader_D){ vim_state.params.selected_reg=0; vim_delete_end(app); }
VIM_COMMAND_SIG(vim_leader_c){ vim_state.params.selected_reg=0; vim_request_change(app); }
VIM_COMMAND_SIG(vim_leader_C){ vim_state.params.selected_reg=0; vim_change_end(app); }

VIM_COMMAND_SIG(vim_digit){
	User_Input input = get_current_input(app);
	if(input.event.kind == InputEventKind_KeyStroke){
		int digit = input.event.key.code - KeyCode_0;
		if(in_range(0, digit, 10)){
			vim_state.number *= 10;
			vim_state.number += digit;
		}
		vim_state.chord_resolved = false;
	}
}

VIM_COMMAND_SIG(vim_digit_del){
	if(vim_state.number != 0){
		vim_state.number /= 10;
		vim_keystroke_text.size = vim_pre_keystroke_size-1;
		vim_state.chord_resolved = false;
	}else{
		vim_reset_state();
	}
}

VIM_COMMAND_SIG(vim_modal_0){
	if(vim_state.number){ vim_digit(app); }
	else{ vim_begin_line(app); }
	//else{ vim_line_start(app); }
}


VIM_COMMAND_SIG(vim_paragraph_up){
	vim_push_jump(app, get_active_view(app, Access_ReadVisible));
	Vim_Motion_Block vim_motion_block(app);
	vim_state.params.edit_type = EDIT_LineWise;
	vim_state.params.clusivity = VIM_Exclusive;
	const i32 N = vim_consume_number();
	foreach(i,N)
		move_up_to_blank_line_end(app);
}

VIM_COMMAND_SIG(vim_paragraph_down){
	vim_push_jump(app, get_active_view(app, Access_ReadVisible));
	Vim_Motion_Block vim_motion_block(app);
	vim_state.params.edit_type = EDIT_LineWise;
	vim_state.params.clusivity = VIM_Exclusive;
	const i32 N = vim_consume_number();
	foreach(i,N)
		move_down_to_blank_line_end(app);
}

VIM_COMMAND_SIG(vim_whole_page_up){   vim_page_scroll_inner(app, -1.0f); }
VIM_COMMAND_SIG(vim_whole_page_down){ vim_page_scroll_inner(app,  1.0f); }
VIM_COMMAND_SIG(vim_half_page_up){    vim_page_scroll_inner(app, -0.5f); }
VIM_COMMAND_SIG(vim_half_page_down){  vim_page_scroll_inner(app,  0.5f); }

VIM_COMMAND_SIG(vim_scroll_screen_top){ vim_scroll_inner(app,  0.0f); }
VIM_COMMAND_SIG(vim_scroll_screen_mid){ vim_scroll_inner(app, -0.5f); }
VIM_COMMAND_SIG(vim_scroll_screen_bot){ vim_scroll_inner(app, -1.0f); }

VIM_COMMAND_SIG(vim_screen_top){ vim_screen_inner(app, 0.0f,  vim_consume_number()); }
VIM_COMMAND_SIG(vim_screen_mid){ vim_screen_inner(app, 0.5f,  vim_consume_number()); }
VIM_COMMAND_SIG(vim_screen_bot){ vim_screen_inner(app, 1.0f, -vim_consume_number()); }

VIM_COMMAND_SIG(vim_line_up){
	View_ID view = get_active_view(app, Access_ReadVisible);
	f32 line_height = get_face_metrics(app, get_face_id(app, 0)).line_height;
	Buffer_Scroll scroll = view_get_buffer_scroll(app, view);
	scroll.target = view_move_buffer_point(app, view, scroll.target, V2f32(0.f, line_height));
	view_set_buffer_scroll(app, view, scroll, SetBufferScroll_SnapCursorIntoView);
}

VIM_COMMAND_SIG(vim_line_down){
	View_ID view = get_active_view(app, Access_ReadVisible);
	f32 line_height = get_face_metrics(app, get_face_id(app, 0)).line_height;
	Buffer_Scroll scroll = view_get_buffer_scroll(app, view);
	scroll.target = view_move_buffer_point(app, view, scroll.target, V2f32(0.f, -line_height));
	view_set_buffer_scroll(app, view, scroll, SetBufferScroll_SnapCursorIntoView);
}

// TODO(BYP): Decide how I want to clamp word deletion in newline cases
VIM_COMMAND_SIG(vim_forward_word){
	Vim_Motion_Block vim_motion_block(app);
	vim_state.params.clusivity = VIM_Exclusive;
	View_ID view = get_active_view(app, Access_ReadVisible);
	i64 prev_pos = -1;
	i64 pos = vim_scan_word(app, view, Scan_Forward, &prev_pos, vim_consume_number());
	view_set_cursor_and_preferred_x(app, view, seek_pos(pos));
	if(prev_pos != pos){
		Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);
		i64 line0 = get_line_number_from_pos(app, buffer, prev_pos);
		i64 line1 = get_line_number_from_pos(app, buffer, pos);
		if(line0 != line1){
			vim_motion_block.clamp_end = get_line_side_pos(app, buffer, line0, Side_Max);
		}
	}
}

VIM_COMMAND_SIG(vim_backward_word){
	Vim_Motion_Block vim_motion_block(app);
	View_ID view = get_active_view(app, Access_ReadVisible);
	i64 pos = vim_scan_word(app, view, Scan_Backward, 0, vim_consume_number());
	view_set_cursor_and_preferred_x(app, view, seek_pos(pos));
}

VIM_COMMAND_SIG(vim_forward_WORD){
	Vim_Motion_Block vim_motion_block(app);
	vim_state.params.clusivity = VIM_Exclusive;
	View_ID view = get_active_view(app, Access_ReadVisible);
	i64 prev_pos = -1;
	i64 pos = vim_scan_WORD(app, view, Scan_Forward, &prev_pos, vim_consume_number());
	view_set_cursor_and_preferred_x(app, view, seek_pos(pos));
	if(prev_pos != pos){
		Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);
		i64 line0 = get_line_number_from_pos(app, buffer, prev_pos);
		i64 line1 = get_line_number_from_pos(app, buffer, pos);
		if(line0 != line1){
			vim_motion_block.clamp_end = get_line_side_pos(app, buffer, line0, Side_Max);
		}
	}
}

VIM_COMMAND_SIG(vim_backward_WORD){
	View_ID view = get_active_view(app, Access_ReadVisible);
	i64 cursor_pos = view_get_cursor_pos(app, view);
	Vim_Motion_Block vim_motion_block(app, cursor_pos-1);
	i64 pos = vim_scan_WORD(app, view, Scan_Backward, 0, vim_consume_number());
	view_set_cursor_and_preferred_x(app, view, seek_pos(pos));
}

VIM_COMMAND_SIG(vim_forward_end){
	Vim_Motion_Block vim_motion_block(app);
	View_ID view = get_active_view(app, Access_ReadVisible);
	i64 pos = vim_scan_end(app, view, Scan_Forward, vim_consume_number());
	view_set_cursor_and_preferred_x(app, view, seek_pos(pos));
}

VIM_COMMAND_SIG(vim_backward_end){
	Vim_Motion_Block vim_motion_block(app);
	View_ID view = get_active_view(app, Access_ReadVisible);
	i64 pos = vim_scan_end(app, view, Scan_Backward, vim_consume_number());
	view_set_cursor_and_preferred_x(app, view, seek_pos(pos));
}

VIM_COMMAND_SIG(vim_forward_END){
	Vim_Motion_Block vim_motion_block(app);
	View_ID view = get_active_view(app, Access_ReadVisible);
	i64 pos = vim_scan_END(app, view, Scan_Forward, vim_consume_number());
	view_set_cursor_and_preferred_x(app, view, seek_pos(pos));
}

VIM_COMMAND_SIG(vim_backward_END){
	Vim_Motion_Block vim_motion_block(app);
	View_ID view = get_active_view(app, Access_ReadVisible);
	i64 pos = vim_scan_END(app, view, Scan_Backward, vim_consume_number());
	view_set_cursor_and_preferred_x(app, view, seek_pos(pos));
}

VIM_COMMAND_SIG(vim_bounce){
	vim_push_jump(app, get_active_view(app, Access_ReadVisible));
	Vim_Motion_Block vim_motion_block(app);
	Scan_Direction direction = Scan_Forward;
	Input_Event event = get_current_input(app).event;
	if(event.kind == InputEventKind_KeyStroke && has_modifier(&event, KeyCode_Control)){ direction=Scan_Backward; }
	View_ID view = get_active_view(app, Access_ReadVisible);
	Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);
	i64 pos = view_get_cursor_pos(app, view);
	pos = vim_scan_bounce(app, buffer, pos, direction);
	view_set_cursor_and_preferred_x(app, view, seek_pos(pos));
}

VIM_COMMAND_SIG(vim_modal_percent){
	if(vim_state.number){ vim_percent_file(app); }
	else{ vim_bounce(app); }
}

VIM_COMMAND_SIG(vim_paste_after){
	if(vim_state.params.selected_reg){
		View_ID view = get_active_view(app, Access_ReadVisible);
		Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);
		if(vim_state.params.selected_reg->edit_type == EDIT_LineWise){
			seek_end_of_line(app);
			move_right(app);
			vim_paste(app, view, buffer, vim_state.params.selected_reg);
			move_up(app);
		}else{
			move_right(app);
			vim_paste(app, view, buffer, vim_state.params.selected_reg);
		}

		Vim_Register *reg = vim_state.prev_params.selected_reg;
		vim_state.params.command = vim_paste_after;
		vim_state.prev_params = vim_state.params;
		vim_state.prev_params.selected_reg = reg;
	}
}

VIM_COMMAND_SIG(vim_paste_before){
	if(vim_state.params.selected_reg){
		if(vim_state.params.selected_reg->edit_type == EDIT_LineWise){
			seek_beginning_of_line(app);
		}
		View_ID view = get_active_view(app, Access_ReadVisible);
		Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);
		vim_paste(app, view, buffer, vim_state.params.selected_reg);

		Vim_Register *reg = vim_state.prev_params.selected_reg;
		vim_state.params.command = vim_paste_before;
		vim_state.prev_params = vim_state.params;
		vim_state.prev_params.selected_reg = reg;
	}
}

// NOTE: This intentionally doesn't update selected register
function void vim_backspace_char_inner(Application_Links *app, i32 offset){
	View_ID view = get_active_view(app, Access_ReadWriteVisible);
	if(!if_view_has_highlighted_range_delete_range(app, view)){
		Buffer_ID buffer = view_get_buffer(app, view, Access_ReadWriteVisible);
		i64 pos = view_get_cursor_pos(app, view);
		i64 buffer_size = buffer_get_size(app, buffer);
		if(in_range(0, pos, buffer_size)){
			Buffer_Cursor cursor = view_compute_cursor(app, view, seek_pos(pos));
			i64 character = view_relative_character_from_pos(app, view, cursor.line, cursor.pos);
			i64 start = view_pos_from_relative_character(app, view, cursor.line, character + offset);
			u8 c = buffer_get_char(app, buffer, start);
			vim_register_copy(&vim_registers.small_delete, SCu8(&c, 1));
			vim_registers.small_delete.edit_type = EDIT_CharWise;
			vim_update_registers(app);
			buffer_replace_range(app, buffer, Ii64(start, pos), string_u8_empty);
		}
	}
}

VIM_COMMAND_SIG(vim_backspace_char){ vim_backspace_char_inner(app, -1); }
VIM_COMMAND_SIG(vim_delete_char){    vim_backspace_char_inner(app, 1); }

VIM_COMMAND_SIG(vim_last_command){
	const i32 N = vim_consume_number();
	Custom_Command_Function *command = vim_state.prev_params.command;
	if(command){
		View_ID view = get_active_view(app, Access_ReadVisible);
		Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);
		History_Group history_group = history_group_begin(app, buffer);
		foreach(i,N){
			vim_state.params = vim_state.prev_params;
			vim_state.active_command = command;
			vim_state.number = vim_state.params.number;
			b32 do_insert = vim_state.params.do_insert;

			if((command == vim_paste_before || command == vim_paste_after) &&
			   in_range(0, (vim_state.params.selected_reg - vim_registers.cycle), 8))
			{
				vim_state.params.selected_reg++;
			}

			command(app);

			if(do_insert){
				vim_paste(app, view, buffer, &vim_registers.insert);
				vim_state.mode = VIM_Normal;
			}
		}
		history_group_end(history_group);
	}
}

function b32
vim_combine_line_inner(Application_Links *app, View_ID view, Buffer_ID buffer, i64 line_num){
	if(!is_valid_line(app, buffer, line_num+1)){ return true; }
	i64 pos = get_line_end_pos(app, buffer, line_num);
	Range_i64 range = {};
	range.min = pos;

	i64 new_pos = pos + 1;
	String_Const_u8 delimiter = (vim_state.sub_mode == SUB_G ? string_u8_empty : string_u8_litexpr(" "));
	if(!line_is_valid_and_blank(app, buffer, line_num+1)){
		if(character_is_whitespace(buffer_get_char(app, buffer, new_pos))){
			new_pos = buffer_seek_character_class_change_1_0(app, buffer, &character_predicate_whitespace, Scan_Forward, new_pos);
		}
	}else{
		new_pos = get_line_end_pos(app, buffer, line_num+1);
		delimiter.size = 0;
	}
	i64 end_pos = get_line_side_pos_from_pos(app, buffer, pos, Side_Max);
	view_set_cursor_and_preferred_x(app, view, seek_pos(end_pos));
	move_right(app);

	range.max = new_pos;

	buffer_replace_range(app, buffer, range, delimiter);

	return false;
}

VIM_COMMAND_SIG(vim_combine_line){
	View_ID view = get_active_view(app, Access_ReadWriteVisible);
	Buffer_ID buffer = view_get_buffer(app, view, Access_ReadWriteVisible);
	if(buffer == 0){ return; }
	i64 pos = view_get_cursor_pos(app, view);
	i64 line = buffer_compute_cursor(app, buffer, seek_pos(pos)).line;

	i32 N = vim_consume_number();
	if(vim_state.mode == VIM_Visual){
		Range_i64 range = get_view_range(app, view);
		i64 line_min = get_line_number_from_pos(app, buffer, range.min);
		i64 line_max = get_line_number_from_pos(app, buffer, range.max);
		N = Max(1, i32(line_max-line_min));
		view_set_cursor_and_preferred_x(app, view, seek_pos(range.min));
		view_set_mark(app, view, seek_pos(range.max));
		line = line_min;
	}

	History_Group history_group = history_group_begin(app, buffer);
	foreach(i,N){
		if(vim_combine_line_inner(app, view, buffer, line)){
			break;
		}
	}
	if(N > 1){ history_group_end(history_group); }
}


VIM_COMMAND_SIG(vim_set_mark){
	View_ID view = get_active_view(app, Access_ReadVisible);
	Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);
	i64 pos = view_get_cursor_pos(app, view);
	Scratch_Block scratch(app);
	u8 character = vim_query_user_key(app, string_u8_litexpr("-- SET MARK NEXT --"));
	if(in_range('a', character, 'z'+1)){
		Managed_Scope scope = buffer_get_managed_scope(app, buffer);
		i64 *marks = (i64 *)managed_scope_get_attachment(app, scope, vim_buffer_marks, 26*sizeof(i64));
		if(marks){
			marks[character-'a'] = pos;
			vim_set_bottom_text(push_stringf(scratch, "Mark %c set", character));
		}
	}
	else if(in_range('A', character, 'Z'+1)){
		vim_global_marks[character-'A'] = {buffer_identifier(buffer), pos};
		vim_set_bottom_text(push_stringf(scratch, "Global mark %c set", character));
	}
}

VIM_COMMAND_SIG(vim_goto_mark){
	User_Input input = get_current_input(app);
	if(input.event.kind == InputEventKind_KeyStroke){
		if(input.event.key.code == KeyCode_Tick){
			vim_state.params.edit_type = EDIT_LineWise;
		}
	}
	View_ID view = get_active_view(app, Access_ReadVisible);
	Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);
	u8 c = vim_query_user_key(app, string_u8_litexpr("-- GOTO MARK NEXT --"));
	if(in_range('a', c, 'z'+1)){
		Managed_Scope scope = buffer_get_managed_scope(app, buffer);
		i64 *marks = (i64 *)managed_scope_get_attachment(app, scope, vim_buffer_marks, 26*sizeof(i64));
		if(marks){
			i64 pos = marks[c-'a'];
			if(pos > 0){
				vim_push_jump(app, view);
				Vim_Motion_Block vim_motion_block(app);
				view_set_cursor_and_preferred_x(app, view, seek_pos(pos));
			}else{
				Scratch_Block scratch(app);
				vim_set_bottom_text(push_stringf(scratch, "Mark %c not set", c));
			}
		}
	}
	else if(in_range('A', c, 'Z'+1)){
		vim_push_jump(app, view);
		Vim_Global_Mark mark = vim_global_marks[c-'A'];
		if(mark.buffer_id.id){
			vim_push_jump(app, view);
			view_set_buffer(app, view, mark.buffer_id.id, 0);
			view_set_cursor_and_preferred_x(app, view, seek_pos(mark.pos));
		}else{
			Scratch_Block scratch(app);
			vim_set_bottom_text(push_stringf(scratch, "Mark %c not set", c));
		}
	}
	else{
		// TODO(BYP): Special marks
		i64 pos = -1;
		switch(c){
			case '\'':{} break;
			case '.': {} break;
			case '`': {} break;
			case '[': {} break;
			case ']': {} break;
			case '<': {} break;
			case '>': {} break;
			//case ' ': { cursor_mark_swap(app); } break;
		}
		if(pos > 0){
			;
		}
	}
}


VIM_COMMAND_SIG(vim_open_file_in_quotes){
	vim_push_jump(app, get_active_view(app, Access_ReadVisible));
	View_ID view = get_active_view(app, Access_ReadVisible);
	Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);
	if(buffer_exists(app, buffer)){
		Scratch_Block scratch(app);

		i64 pos = view_get_cursor_pos(app, view);

		vim_state.params.consume_char = '"';
		vim_state.params.clusivity = VIM_Inclusive;
		Range_i64 range = vim_scan_object_quotes(app, buffer, pos);
		range.min++;
		String_Const_u8 quoted_name = push_buffer_range(app, scratch, buffer, range);

		String_Const_u8 file_name = push_buffer_file_name(app, scratch, buffer);
		String_Const_u8 path = string_remove_last_folder(file_name);

		if(character_is_slash(string_get_character(path, path.size-1))){
			path = string_chop(path, 1);
		}

		String_Const_u8 new_file_name = push_u8_stringf(scratch, "%.*s/%.*s", string_expand(path), string_expand(quoted_name));

		vim_push_jump(app, view);
		view = get_next_view_looped_primary_panels(app, view, Access_Always);
		if(view && view_open_file(app, view, new_file_name, true)){
			view_set_active(app, view);
		}
	}
}

VIM_COMMAND_SIG(vim_goto_definition){
	vim_push_jump(app, get_active_view(app, Access_ReadVisible));
	jump_to_definition_at_cursor(app);
}
VIM_COMMAND_SIG(vim_next_4coder_jump){
	vim_push_jump(app, get_active_view(app, Access_ReadVisible));
	goto_next_jump(app);
}
VIM_COMMAND_SIG(vim_prev_4coder_jump){
	vim_push_jump(app, get_active_view(app, Access_ReadVisible));
	goto_prev_jump(app);
}
VIM_COMMAND_SIG(vim_first_4coder_jump){
	vim_push_jump(app, get_active_view(app, Access_ReadVisible));
	goto_first_jump(app);
}

function void vim_move_selection(Application_Links *app, Scan_Direction direction){
	View_ID view = get_active_view(app, Access_ReadWriteVisible);
	Buffer_ID buffer = view_get_buffer(app, view, Access_ReadWriteVisible);

	Scratch_Block scratch(app);
	const i64 N = vim_consume_number();
	const b32 forward = direction == Scan_Forward;

	i64 cursor_pos = view_get_cursor_pos(app, view);
	i64 mark_pos = view_get_mark_pos(app, view);
	Range_i64 range = Ii64(cursor_pos, mark_pos);
	i64 min_line = get_line_number_from_pos(app, buffer, range.min);
	i64 max_line = get_line_number_from_pos(app, buffer, range.max);
	i64 line_count = buffer_get_line_count(app, buffer);

	Range_i64 copy_range = range_union(get_line_pos_range(app, buffer, min_line),
									   get_line_pos_range(app, buffer, max_line));
	copy_range.max += buffer_get_char(app, buffer, copy_range.max) == '\r';
	copy_range.max += 1;

	i64 paste_pos = (forward ?
					 get_line_pos_range(app, buffer, Min(max_line + N, line_count)).max + 1 :
					 get_line_pos_range(app, buffer, min_line - N).min);
	i64 buff_size = buffer_get_size(app, buffer);
	paste_pos = Min(paste_pos, buff_size);

	String_Const_u8 copy_string = push_buffer_range(app, scratch, buffer, copy_range);

	i64 cursor_offset = cursor_pos - copy_range.min;
	i64 mark_offset = mark_pos - copy_range.min;

	History_Group group = history_group_begin(app, buffer);
	if(forward){
		buffer_replace_range(app, buffer, Ii64(paste_pos), copy_string);

		view_set_cursor(app, view, seek_pos(paste_pos + cursor_offset));
		view_set_mark(app, view, seek_pos(paste_pos + mark_offset));

		buffer_replace_range(app, buffer, copy_range, string_u8_empty);
	}else{
		buffer_replace_range(app, buffer, copy_range, string_u8_empty);
		buffer_replace_range(app, buffer, Ii64(paste_pos), copy_string);

		view_set_cursor(app, view, seek_pos(paste_pos + cursor_offset));
		view_set_mark(app, view, seek_pos(paste_pos + mark_offset));
	}

	history_group_end(group);
}

VIM_COMMAND_SIG(vim_move_selection_up){   vim_move_selection(app, Scan_Backward); }
VIM_COMMAND_SIG(vim_move_selection_down){ vim_move_selection(app, Scan_Forward); }


function i32 vim_macro_index(u8 c){
	return((character_to_lower(c) - 'a') + 26*in_range('A', c, 'Z'+1));
}

VIM_COMMAND_SIG(vim_toggle_macro){
	if(vim_state.macro_char){
		Buffer_ID buffer = get_keyboard_log_buffer(app);
		i64 end = buffer_get_size(app, buffer);
		Buffer_Cursor cursor = buffer_compute_cursor(app, buffer, seek_pos(end));
		Buffer_Cursor back_cursor = buffer_compute_cursor(app, buffer, seek_line_col(cursor.line - 1, 1));
		vim_macros[vim_macro_index(vim_state.macro_char)].max = back_cursor.pos;
		vim_state.macro_char = 0;
	}else{
		if(vim_state.macro_char){ return; }
		u8 c = vim_query_user_key(app, string_u8_litexpr("-- SELECT MACRO TO RECORD --"));
		if(in_range('a', c, 'z'+1) || in_range('A', c, 'Z'+1)){
			vim_state.macro_char = c;
			i32 index = vim_macro_index(c);
			vim_macros[index].min = buffer_get_size(app, get_keyboard_log_buffer(app));
		}
	}
}

VIM_COMMAND_SIG(vim_play_macro){
	u8 c = vim_query_user_key(app, string_u8_litexpr("-- SELECT MACRO TO PLAY --"));
	if(c == '@'){ c = vim_state.prev_macro; }
	if(in_range('a', c, 'z'+1) || in_range('A', c, 'Z'+1)){
		i32 index = vim_macro_index(c);
		Range_i64 range = vim_macros[index];
		if(range.min == 0 || range.max == 0){ return; }
		vim_state.prev_macro = c;

		Buffer_ID key_buffer = get_keyboard_log_buffer(app);
		Scratch_Block scratch(app);
		String_Const_u8 macro = push_buffer_range(app, scratch, key_buffer, range);
		keyboard_macro_play(app, macro);
	}
}

VIM_COMMAND_SIG(vim_insert_command);



/// Vim commands aliases for lister
CUSTOM_COMMAND_SIG(w)
CUSTOM_DOC("Vim: Saves the buffer") { save(app); }

CUSTOM_COMMAND_SIG(wq)
CUSTOM_DOC("Vim: Saves and quits the buffer") { save(app); close_panel(app); }

CUSTOM_COMMAND_SIG(wqa)
CUSTOM_DOC("Vim: Saves and quits all buffers") { save_all_dirty_buffers(app); exit_4coder(app); }

CUSTOM_COMMAND_SIG(q)
CUSTOM_DOC("Vim: Close panel") { close_panel(app); }

CUSTOM_COMMAND_SIG(qk)
CUSTOM_DOC("Vim: Attempt to kill buffer and close panel") { vim_try_buffer_kill(app); close_panel(app); }

CUSTOM_COMMAND_SIG(qa)
CUSTOM_DOC("Vim: Attempt to exit") { exit_4coder(app); }

CUSTOM_COMMAND_SIG(e)
CUSTOM_DOC("Vim: Open file") { vim_interactive_open_or_new(app); }

CUSTOM_COMMAND_SIG(b)
CUSTOM_DOC("Vim: Open file") { vim_interactive_open_or_new(app); }

CUSTOM_COMMAND_SIG(vs)
CUSTOM_DOC("Vim: Vertical split") { open_panel_vsplit(app); }

CUSTOM_COMMAND_SIG(sp)
CUSTOM_DOC("Vim: Horizontal split") { open_panel_hsplit(app); }

CUSTOM_COMMAND_SIG(s)
CUSTOM_DOC("Vim: Substitute"){ replace_in_buffer(app); }

CUSTOM_COMMAND_SIG(jumps)
CUSTOM_DOC("Vim: Interactive jump stack lister"){ vim_jump_lister(app); }
