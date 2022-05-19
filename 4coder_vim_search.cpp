
function i64 vim_pattern_inner_v(Application_Links *app, Buffer_Seek_String_Flags seek_flags){
	String_u8 *pattern = &vim_registers.search.data;
	if(pattern->size == 0){ return -1; }
	View_ID view = get_active_view(app, Access_ReadVisible);
	Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);
	i64 pos = view_get_cursor_pos(app, view);
	i64 new_pos = -1;
	seek_string(app, buffer, pos, 0, 0, pattern->string, &new_pos, seek_flags);
	return new_pos;

}

function void vim_in_pattern_inner(Application_Links *app, Buffer_Seek_String_Flags seek_flags){
	String_u8 *pattern = &vim_registers.search.data;
	i64 new_pos = vim_pattern_inner_v(app, seek_flags);
	View_ID view = get_active_view(app, Access_ReadVisible);
	Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);
	i64 buffer_size = buffer_get_size(app, buffer);
	if(new_pos > 0 && new_pos != buffer_size){
		vim_push_jump(app, view);
		Vim_Motion_Block vim_motion_block(app, new_pos + pattern->size-1);
		view_set_cursor_and_preferred_x(app, view, seek_pos(new_pos));
	}
}

function void vim_to_pattern_inner(Application_Links *app, Buffer_Seek_String_Flags seek_flags){
	View_ID view = get_active_view(app, Access_ReadVisible);
	Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);
	i64 buffer_size = buffer_get_size(app, buffer);
	i64 new_pos = vim_pattern_inner_v(app, seek_flags);
	if(new_pos > 0 && new_pos != buffer_size){
		vim_push_jump(app, view);
		Vim_Motion_Block vim_motion_block(app);
		view_set_cursor_and_preferred_x(app, view, seek_pos(new_pos));
	}
}


function void
vim_start_search_inner(Application_Links *app, Scan_Direction start_direction){
	View_ID view = get_active_view(app, Access_ReadVisible);
	Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);
	if(!buffer_exists(app, buffer)){ return; }

	i64 buffer_size = buffer_get_size(app, buffer);

	Vec2_f32 old_margin = {};
	Vec2_f32 old_push_in = {};
	view_get_camera_bounds(app, view, &old_margin, &old_push_in);

	Vec2_f32 margin = old_margin;
	margin.y = clamp_bot(200.f, margin.y);
	view_set_camera_bounds(app, view, margin, old_push_in);

	Scan_Direction direction = start_direction;
	i64 pos = view_get_cursor_pos(app, view);

	/// TODO(BYP): Need a better system to have multiple types of these drawing
	vim_use_bottom_cursor = true;
	String_Const_u8 prefix = (start_direction == Scan_Forward ? string_u8_litexpr("/") : string_u8_litexpr("?"));
	vim_set_bottom_text(prefix);
	u8 *dest = vim_bot_text.str + vim_bot_text.size;
	u64 base_size, after_size;
	base_size = after_size = vim_bot_text.size;

	Vim_Register *reg = &vim_registers.search;
	if(reg->data.size < 256){ vim_realloc_string(&reg->data, 0); }
	reg->data.size = 0;
	String_u8 *query = &reg->data;

	f32 prev_offset = vim_nxt_filebar_offset;

	i64 match_size = 0;
	User_Input in = {};
	for(;;){
		if(vim_nxt_filebar_offset == 0.f){ vim_nxt_filebar_offset = 0.1f; }
		animate_in_n_milliseconds(app, 0);
		vim_set_bottom_text(prefix);
		block_copy(dest, query->str, query->size);
		vim_bot_text.size = after_size + query->size;

		in = get_next_input(app, EventPropertyGroup_Any, EventProperty_Escape);
		if(in.abort){ query->size = 0; break; }

		String_Const_u8 string = to_writable(&in);

		b32 string_change = false;
		if(match_key_code(&in, KeyCode_Return)){
			reg->flags |= (REGISTER_Set|REGISTER_Updated);
			break;
		}
		else if(string.str != 0 && string.size > 0){
			string_append(query, string);
			string_change = true;
		}
		else if(match_key_code(&in, KeyCode_Backspace)){
			u64 old_size = query->size;
			if(has_modifier(&in.event.key.modifiers, KeyCode_Control)){
				if(has_modifier(&in.event.key.modifiers, KeyCode_Shift)){
					query->string.size = 0;
				}else{
					query->string = ctrl_backspace_utf8(query->string);
				}
			}else{
				query->string = backspace_utf8(query->string);
			}
			string_change = old_size != query->size;
		}
		else{ leave_current_input_unhandled(app); }

		b32 do_scan_action = false;
		if(string_change){
			reg->flags |= (REGISTER_Set|REGISTER_Updated);
			vim_update_registers(app);
			i64 new_pos = 0;
			switch(direction){
				case Scan_Forward:{
					//seek_string(app, buffer, pos - 1, 0, query->string, &new_pos, seek_flags);
					seek_string_insensitive_forward(app, buffer, pos - 1, 0, query->string, &new_pos);
					if(new_pos < buffer_size){
						pos = new_pos;
						match_size = query->string.size;
					}
				} break;

				case Scan_Backward:{
					seek_string_insensitive_backward(app, buffer, pos + 1, 0, query->string, &new_pos);
					if(new_pos >= 0){
						pos = new_pos;
						match_size = query->string.size;
					}
				} break;
			}
			if(in_range(0, new_pos, buffer_size)){
				view_set_cursor_and_preferred_x(app, view, seek_pos(new_pos));
				isearch__update_highlight(app, view, Ii64_size(new_pos, match_size));
			}
		}
		else if(do_scan_action){
			//direction = change_scan;
			switch(direction){
				case Scan_Forward:{
					i64 new_pos = 0;
					seek_string_insensitive_forward(app, buffer, pos, 0, query->string, &new_pos);
					if (new_pos < buffer_size){
						pos = new_pos;
						match_size = query->string.size;
					}
				} break;

				case Scan_Backward:{
					i64 new_pos = 0;
					seek_string_insensitive_backward(app, buffer, pos, 0, query->string, &new_pos);
					if(new_pos >= 0){
						pos = new_pos;
						match_size = query->string.size;
					}
				} break;
			}
		}
		//else if(do_scroll_wheel){ mouse_wheel_scroll(app); }
	}

	view_disable_highlight_range(app, view);

	vim_reset_bottom_text();
	vim_use_bottom_cursor = false;
	vim_nxt_filebar_offset = prev_offset;
	view_set_camera_bounds(app, view, old_margin, old_push_in);
}

VIM_COMMAND_SIG(vim_clear_search){
	vim_registers.search.data.size = 0;
	vim_registers.search.flags &= (~REGISTER_Set);
	vim_update_registers(app);
}

VIM_COMMAND_SIG(vim_start_search_forward){  vim_start_search_inner(app, Scan_Forward); }
VIM_COMMAND_SIG(vim_start_search_backward){ vim_start_search_inner(app, Scan_Backward); }

VIM_COMMAND_SIG(vim_to_next_pattern){ vim_to_pattern_inner(app, 0); }
VIM_COMMAND_SIG(vim_to_prev_pattern){ vim_to_pattern_inner(app, BufferSeekString_Backward); }

VIM_COMMAND_SIG(vim_in_next_pattern){ vim_in_pattern_inner(app, 0); }
VIM_COMMAND_SIG(vim_in_prev_pattern){ vim_in_pattern_inner(app, BufferSeekString_Backward); }

VIM_COMMAND_SIG(vim_search_identifier){
	View_ID view = get_active_view(app, Access_ReadVisible);
	Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);
	i64 pos = view_get_cursor_pos(app, view);
	Range_i64 range = enclose_pos_alpha_numeric_underscore(app, buffer, pos);
	vim_state.params.selected_reg = &vim_registers.search;
	vim_request_vtable[REQUEST_Yank](app, view, buffer, range);
	vim_default_register();
}

