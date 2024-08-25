
CUSTOM_COMMAND_SIG(no_op)
CUSTOM_DOC("no op for binding keybinds to resolve without side effect")
{}

function b32 vim_show_cursor(Application_Links *app){
	if(vim_cursor_blink < 10.f){ animate_in_n_milliseconds(app, 0); }
	return ACTIVE_BLINK(vim_cursor_blink);
}

// Blocking call
function u8 vim_query_user_key(Application_Links *app, String_Const_u8 message){
	u8 result = 0;

	local_persist u8 vim_bot_temp_buffer[256];
	u64 size = vim_bot_text.size;
	block_copy(vim_bot_temp_buffer, vim_bot_text.str, size);
	vim_set_bottom_text(message);
	vim_is_querying_user_key = true;
	vim_state.chord_resolved = false;

	for(;;){
		User_Input in = get_next_input(app, EventPropertyGroup_Any, EventProperty_Escape);
		if(in.abort){ vim_state.params.request = REQUEST_None; break; }
		if(in.event.kind == InputEventKind_TextInsert){
			result = in.event.text.string.str[0];
			string_append_character(&vim_keystroke_text, result);
			break;
		}
		else if(in.event.kind == InputEventKind_KeyStroke){
			in.event.kind = InputEventKind_None;
			leave_current_input_unhandled(app);
		}
		else{
			leave_current_input_unhandled(app);
		}
	}

	vim_set_bottom_text(i32(size), (char *)vim_bot_temp_buffer);
	vim_is_querying_user_key = false;
	vim_state.chord_resolved = true;
	Scratch_Block scratch(app);
	printf_message(app, scratch, "User key was '%c' \n", result);
	return result;
}


function void vim_enter_insert_mode(Application_Links *app){
	View_ID view = get_active_view(app, Access_ReadVisible);
	Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);
	vim_state.params.number = vim_consume_number();
	vim_history_group = history_group_begin(app, buffer);
	vim_state.mode = VIM_Insert;
	vim_state.insert_index = buffer_history_get_current_state_index(app, buffer);
	vim_state.insert_cursor = buffer_compute_cursor(app, buffer, seek_pos(view_get_cursor_pos(app, view)));
}

function void vim_clamp_newline(Application_Links *app, View_ID view, Buffer_ID buffer, i64 cursor_pos){
	u8 c = buffer_get_char(app, buffer, cursor_pos);
	i64 line = get_line_number_from_pos(app, buffer, cursor_pos);
	if(!line_is_valid_and_blank(app, buffer, line) && (c == '\r' || c == '\n')){ move_left(app); }
}

function u8 character_toggle_case(u8 c){
	i32 shift = ((2*character_is_upper(c)-1)*('a'-'A'));
	return (c + u8((character_is_alpha(c) && c != '_')*shift));
}

function Range_i64 get_line_range_from_pos(Application_Links *app, Buffer_ID buffer, i64 pos){
	return get_line_pos_range(app, buffer, get_line_number_from_pos(app, buffer, pos));
}

function void move_horizontal_lines(Application_Links *app, i32 count){
	View_ID view = get_active_view(app, Access_ReadVisible);
	Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);
	i64 pos = view_get_cursor_pos(app, view);
	Range_i64 line_range = get_line_range_from_pos(app, buffer, pos);
	i64 new_pos = view_set_pos_by_character_delta(app, view, pos, count);
	new_pos = clamp(line_range.min, new_pos, line_range.max);
	view_set_cursor_and_preferred_x(app, view, seek_pos(new_pos));
}

function void seek_one_past_end(Application_Links *app){
	View_ID view = get_active_view(app, Access_ReadVisible);
	i64 pos = view_get_cursor_pos(app, view);
	i64 line = view_compute_cursor(app, view, seek_pos(pos)).line;
	Vec2_f32 p = view_relative_xy_of_pos(app, view, line, pos);
	p.x = max_f32;
	i64 new_pos = view_pos_at_relative_xy(app, view, line, p);
	view_set_cursor_and_preferred_x(app, view, seek_pos(new_pos+1));
}

function void vim_set_prev_visual(Application_Links *app, View_ID view){
	Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);
	Managed_Scope scope = buffer_get_managed_scope(app, buffer);
	Vim_Prev_Visual *prev_visual = scope_attachment(app, scope, vim_buffer_prev_visual, Vim_Prev_Visual);

	if(prev_visual){
		prev_visual->cursor_pos = view_get_cursor_pos(app, view);
		prev_visual->mark_pos = view_get_mark_pos(app, view);
		prev_visual->edit_type = vim_state.params.edit_type;
	}
}

function void vim_push_jump(Application_Links *app, View_ID view){
	Managed_Scope scope = view_get_managed_scope(app, view);
	Vim_Jump_List *jump_list = scope_attachment(app, scope, vim_view_jumps, Vim_Jump_List);
	if(jump_list){
		u64 slot_cap = ArrayCount(jump_list->markers);
		u64 slot_count = jump_list->top - jump_list->bot;

		Point_Stack_Slot *slot = &jump_list->markers[jump_list->pos % slot_cap];
		slot->buffer = view_get_buffer(app, view, Access_ReadVisible);
		slot->object = view_get_cursor_pos(app, view);

		jump_list->bot += (slot_count == slot_cap);
		jump_list->pos += 1;
		jump_list->top = jump_list->pos;
		if(jump_list->bot > slot_cap){
			jump_list->bot -= slot_cap;
			jump_list->pos -= slot_cap;
			jump_list->top -= slot_cap;
		}
	}
}

function void vim_set_jump(Application_Links *app, View_ID view, Vim_Jump_List *jump_list, u64 pos){
	jump_list->pos = pos;
	Point_Stack_Slot *slot = &jump_list->markers[pos % ArrayCount(jump_list->markers)];
	bool recenter = false;
	Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);
	if(slot->buffer != buffer){
		view_set_buffer(app, view, slot->buffer, 0);
		recenter = true;
	}else{
		Buffer_Point buffer_point = view_get_buffer_scroll(app, view).position;
		Rect_f32 region = view_get_buffer_region(app, view);
		Text_Layout_ID text_layout_id = text_layout_create(app, buffer, region, buffer_point);
		Range_i64 visible_range = text_layout_get_visible_range(app, text_layout_id);
		text_layout_free(app, text_layout_id);
		recenter = !range_contains(visible_range, slot->object);
	}
	view_set_cursor_and_preferred_x(app, view, seek_pos(slot->object));

	if(recenter){ center_view(app); }
}

function void vim_dec_jump(Application_Links *app, View_ID view){
	Managed_Scope scope = view_get_managed_scope(app, view);
	Vim_Jump_List *jump_list = scope_attachment(app, scope, vim_view_jumps, Vim_Jump_List);
	if(jump_list){
		u64 next_pos = jump_list->pos - 1;
		if(jump_list->pos == jump_list->bot){ return; }
		if(jump_list->pos == jump_list->top){ vim_push_jump(app, view); }
		vim_set_jump(app, view, jump_list, next_pos);
	}
}

function void vim_inc_jump(Application_Links *app, View_ID view){
	Managed_Scope scope = view_get_managed_scope(app, view);
	Vim_Jump_List *jump_list = scope_attachment(app, scope, vim_view_jumps, Vim_Jump_List);
	if(jump_list){
		if(jump_list->pos == jump_list->top){ return; }
		vim_set_jump(app, view, jump_list, jump_list->pos+1);
		jump_list->top -= (jump_list->pos == jump_list->top-1);
	}
}

VIM_COMMAND_SIG(vim_prev_jump){ vim_dec_jump(app, get_active_view(app, Access_ReadVisible)); }
VIM_COMMAND_SIG(vim_next_jump){ vim_inc_jump(app, get_active_view(app, Access_ReadVisible)); }


struct Vim_Motion_Block{
	Application_Links *app;
	i64 begin_pos, end_pos;
	i64 clamp_end = -1;
	Vim_Edit_Type prev_edit;

	Vim_Motion_Block(Application_Links *a, i64 b) : app(a), begin_pos(b), prev_edit(vim_state.params.edit_type) {}
	Vim_Motion_Block(Application_Links *a) : app(a), prev_edit(vim_state.params.edit_type) {
		View_ID view = get_active_view(app, Access_ReadVisible);
		begin_pos = view_get_cursor_pos(app, view);
	}
	~Vim_Motion_Block();
};

// TODO(BYP): clamp_end is arguably a hack, but the case it approximates is even more of a hack
Vim_Motion_Block::~Vim_Motion_Block(){
	View_ID view = get_active_view(app, Access_ReadVisible);
	Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);
	Vim_Params *params = &vim_state.params;

	if(params->edit_type == EDIT_Block){
		vim_block_edit(app, view, buffer, get_view_range(app, view));
	}else{
		end_pos = view_get_cursor_pos(app, view);
		i64 buffer_size = buffer_get_size(app, buffer);

		i64 range_begin=begin_pos, range_end=end_pos;
		if(clamp_end > 0){ range_end = Min(range_end, clamp_end); }
		if(params->clusivity == VIM_Exclusive){
			if(begin_pos <= end_pos){ range_end--; }
			else{ range_begin++; }
		}
		Range_i64 range = Ii64(range_begin, range_end);
		range.max = Min(range.max+1, buffer_size);

		if(params->edit_type == EDIT_LineWise){
			range = range_union(get_line_range_from_pos(app, buffer, begin_pos),
								get_line_range_from_pos(app, buffer, end_pos));
			if(++range.max >= buffer_size){
				range.max = buffer_size;
				range.min = Max(0, range.min-1);
			}
			range.max -= (params->request == REQUEST_Change);
		}

		vim_request_vtable[params->request](app, view, buffer, range);
	}

	if(params->request == REQUEST_Yank || (params->request != REQUEST_None && clamp_end > 0)){
		Vec2_f32 v0 = view_relative_xy_of_pos(app, view, 0, begin_pos);
		Vec2_f32 v1 = view_relative_xy_of_pos(app, view, 0, end_pos);
		vim_nxt_cursor_pos += 2.f*(v1 - v0);
		view_set_cursor_and_preferred_x(app, view, seek_pos(end_pos = begin_pos));
	}

	if(params->request != REQUEST_None && vim_state.mode != VIM_Visual){
		vim_state.params.command = vim_state.active_command;
		vim_state.prev_params = vim_state.params;
	}

	Vim_Seek_Params seek = vim_state.params.seek;
	vim_state.params = {};
	vim_state.params.seek = seek;
	if(vim_state.params.selected_reg){
		vim_state.params.selected_reg->flags &= (~REGISTER_Append);
	}
	vim_default_register();

	vim_state.sub_mode = SUB_None;
	if(vim_state.mode != VIM_Insert && vim_state.mode != VIM_Visual){
		vim_clamp_newline(app, view, buffer, end_pos);
	}
	if(vim_state.mode == VIM_Visual){ vim_state.params.edit_type = prev_edit; }
}

function void
vim_visual_insert_inner(Application_Links *app, View_ID view, Buffer_ID buffer){
	auto_indent_range(app);
	vim_set_prev_visual(app, view);

	vim_visual_insert_after = false;
	vim_history_group = history_group_begin(app, buffer);

	// NOTE: For Visual Block multi-line responsiveness, temporarily turn off virtual whitespace and line wrapping
	Managed_Scope scope = buffer_get_managed_scope(app, buffer);
	b32 *wrap_lines_ptr = scope_attachment(app, scope, buffer_wrap_lines, b32);
	if(wrap_lines_ptr && *wrap_lines_ptr){
		toggle_line_wrap(app);
		vim_visual_insert_flags |= bit_1;
	}
	if(def_enable_virtual_whitespace){
		toggle_virtual_whitespace(app);
		vim_visual_insert_flags |= bit_2;
	}

	//vim_enter_insert_mode(app);
	vim_state.mode = VIM_Visual_Insert;
}

VIM_COMMAND_SIG(vim_up){
	if(vim_state.number >= 10){ vim_push_jump(app, get_active_view(app, Access_ReadVisible)); }
	Vim_Motion_Block vim_motion_block(app);
	vim_state.params.edit_type = EDIT_LineWise;
	move_vertical_lines(app, -vim_consume_number());
}

VIM_COMMAND_SIG(vim_down){
	if(vim_state.number >= 10){ vim_push_jump(app, get_active_view(app, Access_ReadVisible)); }
	Vim_Motion_Block vim_motion_block(app);
	vim_state.params.edit_type = EDIT_LineWise;
	move_vertical_lines(app, vim_consume_number());
}

VIM_COMMAND_SIG(vim_left){
	Vim_Motion_Block vim_motion_block(app);
	move_horizontal_lines(app, -vim_consume_number());
}

VIM_COMMAND_SIG(vim_right){
	Vim_Motion_Block vim_motion_block(app);
	move_horizontal_lines(app, vim_consume_number());
}

function void
vim_make_request(Application_Links *app, Vim_Request_Type request){
	if(vim_state.params.request == request){
		Vim_Motion_Block vim_motion_block(app);
		vim_state.params.edit_type = EDIT_LineWise;
		vim_state.params.edit_type = EDIT_LineWise;
		move_vertical_lines(app, vim_consume_number()-1);
	}else{
		vim_state.params.count = vim_consume_number();
		vim_state.params.request = request;
		if(vim_state.mode == VIM_Visual){
			View_ID view = get_active_view(app, Access_ReadVisible);
			b32 do_visual_insert = (vim_state.params.edit_type == EDIT_Block && request == REQUEST_Change);
			vim_set_prev_visual(app, view);
			vim_state.mode = VIM_Normal;
			{
				Vim_Motion_Block vim_motion_block(app, view_get_mark_pos(app, view));
			}
			if(do_visual_insert){
				Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);
				vim_visual_insert_inner(app, view, buffer);
			}
		}
		else{ vim_state.chord_resolved = false; }
	}
}

function void vim_page_scroll_inner(Application_Links *app, f32 ratio){
	View_ID view = get_active_view(app, Access_ReadVisible);
	vim_push_jump(app, view);

	f32 scroll_pixels = ratio*get_page_jump(app, view);
	move_vertical_pixels(app, scroll_pixels);

	Buffer_Scroll scroll = view_get_buffer_scroll(app, view);
	scroll.target = view_move_buffer_point(app, view, scroll.target, V2f32(0.f, scroll_pixels));
	view_set_buffer_scroll(app, view, scroll, SetBufferScroll_NoCursorChange);
}
