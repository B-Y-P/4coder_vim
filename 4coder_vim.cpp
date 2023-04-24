
VIM_REQUEST_SIG(vim_apply_none){}
VIM_REQUEST_SIG(vim_apply_yank){
	if(vim_state.params.selected_reg){
		vim_state.params.selected_reg->edit_type = vim_state.params.edit_type;
		vim_copy(app, view, buffer, range, vim_state.params.selected_reg);
	}
}

VIM_REQUEST_SIG(vim_apply_delete){
	vim_apply_yank(app, view, buffer, range);
	if(vim_state.params.edit_type != EDIT_Block && vim_state.params.selected_reg){
		Vim_Register *dst = &vim_registers.small_delete;
		if(vim_state.params.edit_type != EDIT_CharWise){
			dst = vim_registers.cycle;
			vim_push_reg_cycle(app);
		}
		vim_register_copy(dst, vim_state.params.selected_reg);
		vim_update_registers(app);
	}
	buffer_replace_range(app, buffer, range, string_u8_empty);
	if(vim_state.params.edit_type == EDIT_LineWise){
		i64 pos = get_line_side_pos_from_pos(app, buffer, view_get_cursor_pos(app, view), Side_Min);
		view_set_cursor_and_preferred_x(app, view, seek_pos(pos));
	}
}

VIM_REQUEST_SIG(vim_apply_change){
	vim_enter_insert_mode(app);
	vim_apply_delete(app, view, buffer, range);
}

VIM_REQUEST_SIG(vim_apply_upper){
	Scratch_Block scratch(app);
	String_Const_u8 string = push_buffer_range(app, scratch, buffer, range);
	string = string_mod_upper(string);
	buffer_replace_range(app, buffer, range, string);
	buffer_post_fade(app, buffer, 0.667f, range, fcolor_resolve(fcolor_id(defcolor_paste)));
}

VIM_REQUEST_SIG(vim_apply_lower){
	Scratch_Block scratch(app);
	String_Const_u8 string = push_buffer_range(app, scratch, buffer, range);
	string = string_mod_lower(string);
	buffer_replace_range(app, buffer, range, string);
	buffer_post_fade(app, buffer, 0.667f, range, fcolor_resolve(fcolor_id(defcolor_paste)));
}

VIM_REQUEST_SIG(vim_apply_replace){
	Scratch_Block scratch(app);
	String_Const_u8 string = push_buffer_range(app, scratch, buffer, range);
	foreach(i, range_size(range)){
		if(string.str[i] != '\n'){ string.str[i] = vim_state.params.seek.character; }
	}
	buffer_replace_range(app, buffer, range, string);
	buffer_post_fade(app, buffer, 0.667f, range, fcolor_resolve(fcolor_id(defcolor_paste)));
}

VIM_REQUEST_SIG(vim_apply_indent){
	i64 line0 = get_line_number_from_pos(app, buffer, range.min);
	i64 line1 = get_line_number_from_pos(app, buffer, range.max);
	line1 += (line0 == line1);
	History_Group history_group = history_group_begin(app, buffer);
	for(i64 l=line0; l<line1; l++){
		i64 pos = get_line_start_pos(app, buffer, l);
		buffer_replace_range(app, buffer, Ii64(pos), string_u8_litexpr("\t"));
	}
	history_group_end(history_group);
}

VIM_REQUEST_SIG(vim_apply_outdent){
	i64 line0 = get_line_number_from_pos(app, buffer, range.min);
	i64 line1 = get_line_number_from_pos(app, buffer, range.max);
	line1 += (line0 == line1);
	History_Group history_group = history_group_begin(app, buffer);
	for(i64 l=line0; l<line1; l++){
		i64 pos = get_line_start_pos(app, buffer, l);
		Range_i64 tab_range = Ii64(pos, pos + (buffer_get_char(app, buffer, pos) == '\t'));
		buffer_replace_range(app, buffer, tab_range, string_u8_empty);
	}
	history_group_end(history_group);
}

VIM_REQUEST_SIG(vim_apply_auto_indent){
	auto_indent_buffer(app, buffer, range);
	buffer_post_fade(app, buffer, 0.667f, range, fcolor_resolve(fcolor_id(defcolor_paste)));
}


VIM_REQUEST_SIG(vim_apply_toggle_case){
	Scratch_Block scratch(app);
	String_Const_u8 string = push_buffer_range(app, scratch, buffer, range);
	foreach(i, string.size){
		string.str[i] = character_toggle_case(string.str[i]);
	}
	buffer_replace_range(app, buffer, range, string);
	buffer_post_fade(app, buffer, 0.667f, range, fcolor_resolve(fcolor_id(defcolor_paste)));
}

VIM_REQUEST_SIG(vim_apply_fold){
	fold_push(app, buffer, range);
}

function void
vim_init(Application_Links *app){
	init_keycode_lut();
	init_vim_boundaries();
	vim_reset_bottom_text();
	vim_reset_state();

    {
		default_color_table.arrays[defcolor_vim_filebar_pop]      = default_color_table.arrays[defcolor_mark];
		default_color_table.arrays[defcolor_vim_chord_text]       = default_color_table.arrays[defcolor_text_default];
		default_color_table.arrays[defcolor_vim_chord_unresolved] = default_color_table.arrays[defcolor_mark];
		default_color_table.arrays[defcolor_vim_chord_error]      = default_color_table.arrays[defcolor_special_character];
	}

#if VIM_USE_REIGSTER_BUFFER
	Buffer_ID reg_buffer = create_buffer(app, string_u8_litexpr("*registers*"),
										 BufferCreate_NeverAttachToFile|BufferCreate_AlwaysNew);
	buffer_set_setting(app, reg_buffer, BufferSetting_ReadOnly, true);
	buffer_set_setting(app, reg_buffer, BufferSetting_Unkillable, true);
	buffer_set_setting(app, reg_buffer, BufferSetting_Unimportant, true);
#endif

	vim_state.arena = make_arena_system();
	heap_init(&vim_state.heap, &vim_state.arena);
	vim_state.alloc = base_allocator_on_heap(&vim_state.heap);

	vim_request_vtable[REQUEST_None]       = vim_apply_none;
	vim_request_vtable[REQUEST_Yank]       = vim_apply_yank;
	vim_request_vtable[REQUEST_Delete]     = vim_apply_delete;
	vim_request_vtable[REQUEST_Change]     = vim_apply_change;
	vim_request_vtable[REQUEST_Upper]      = vim_apply_upper;
	vim_request_vtable[REQUEST_Lower]      = vim_apply_lower;
	vim_request_vtable[REQUEST_Replace]    = vim_apply_replace;
	vim_request_vtable[REQUEST_ToggleCase] = vim_apply_toggle_case;
	vim_request_vtable[REQUEST_Indent]     = vim_apply_indent;
	vim_request_vtable[REQUEST_Outdent]    = vim_apply_outdent;
	vim_request_vtable[REQUEST_AutoIndent] = vim_apply_auto_indent;
	vim_request_vtable[REQUEST_Fold]       = vim_apply_fold;

	vim_text_object_vtable[TEXT_OBJECT_para] = {'p', (Vim_Text_Object_Func *)vim_object_para};
	vim_text_object_vtable[TEXT_OBJECT_word] = {'w', (Vim_Text_Object_Func *)vim_object_word};
	vim_text_object_vtable[TEXT_OBJECT_Word] = {'W', (Vim_Text_Object_Func *)vim_object_WORD};

	foreach(i,ArrayCount(vim_request_vtable)){
#if 1
		if(vim_request_vtable[i] == 0){ Assert(false); }
#else
		if(vim_request_vtable[i] == 0){ vim_request_vtable[i] = vim_apply_none; }
#endif
	}

	foreach(i,ArrayCount(vim_text_object_vtable)){
#if 1
		if(vim_text_object_vtable[i].func == 0){ Assert(false); }
#else
		if(vim_text_object_vtable[i].func == 0){
			vim_text_object_vtable[i] = {0, (Vim_Text_Object_Func *)vim_object_none};
		}
#endif
	}

	foreach(i, ArrayCount(vim_default_peek_list)){
		vim_buffer_peek_list[i] = vim_default_peek_list[i];
	}
	foreach(i, ArrayCount(vim_buffer_peek_list)){
		Assert(vim_buffer_peek_list[i].buffer_id.name != 0);
	}

	vim_register_copy(&vim_registers.small_delete, string_u8_empty);
	vim_register_copy(&vim_registers.insert, string_u8_empty);
	for(i32 i=i32(vim_registers.digit - vim_registers.r); i<ArrayCount(vim_registers.r); i++){
		vim_registers.r[i].flags |= REGISTER_ReadOnly;
	}

	Base_Allocator *base = get_base_allocator_system();
	foreach(i,ArrayCount(vim_maps)){
		vim_maps[i] = make_table_u64_u64(base, 100);
	}
}


// TODO(BYP): Paste/registers and tab-completion (very unlikely with current implementation)
// If it's a pressing feature switch to the more standard (less responsive) implementation
function b32
vim_handle_visual_insert_mode(Application_Links *app, Input_Event *event){
	View_ID view = get_active_view(app, Access_ReadVisible);
	Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);

	local_persist i32 count=0;

	if(event->kind == InputEventKind_KeyStroke){

		if(event->key.code == KeyCode_Escape || event->key.code == KeyCode_Return){
			vim_normal_mode(app);
			history_group_end(vim_history_group);
			if(vim_visual_insert_flags & bit_1){ toggle_line_wrap(app); }
			if(vim_visual_insert_flags & bit_2){ toggle_virtual_whitespace(app); }
			vim_visual_insert_flags = 0;
			count = 0;
			return true;
		}

		if(event->key.code == KeyCode_Backspace){
			if(count > 0){ undo(app); count--; }
			if(has_modifier(event, KeyCode_Control)){

				b32 clearing_whitespace = true;
				while(count > 0){
					Range_i64 range = get_view_range(app, view);
					i64 line_min = get_line_number_from_pos(app, buffer, range.min);
					Rect_f32 block_rect = vim_get_rel_block_rect(app, view, buffer, range, line_min);

					f32 x_off = vim_visual_insert_after*rect_width(block_rect);
					Vec2_f32 point = block_rect.p0 + V2f32(x_off, 0.f);
					i64 pos = view_pos_at_relative_xy(app, view, line_min, point) + vim_visual_insert_after;

					u8 c = buffer_get_char(app, buffer, pos-1);
					if(!clearing_whitespace &&  character_is_whitespace(c)){ break; }
					if(clearing_whitespace  && !character_is_whitespace(c)){ clearing_whitespace = false; }

					undo(app); count--;
				}

			}
			return true;
		}

		// NOTE(BYP): Bit of a hack because I reflexively Ctl-S but don't want those inserted
		b32 result = has_modifier(event, KeyCode_Control);
		event->kind = InputEventKind_None;
		return result;
	}

	if(event->kind == InputEventKind_TextInsert){
		vim_visual_insert_char(app, view, buffer, event->text.string.str[0]);
		count++;
		return true;
	}

	event->kind = InputEventKind_None;
	return false;
}

function b32
vim_handle_replace_mode(Application_Links *app, Input_Event *event){
	View_ID view = get_active_view(app, Access_ReadVisible);
	Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);
	if(event->kind == InputEventKind_KeyStroke){
		if(event->key.code == KeyCode_Escape){
			vim_normal_mode(app);
			history_group_end(vim_history_group);
			return true;
		}
		if(event->key.code == KeyCode_Backspace){
			if(has_modifier(event, KeyCode_Control)){
				i64 cursor_pos = view_get_cursor_pos(app, view);
				while(cursor_pos != view_get_mark_pos(app, view) &&
					  !character_is_whitespace(buffer_get_char(app, buffer, cursor_pos-1)))
				{
					undo(app);
					cursor_pos = view_get_cursor_pos(app, view);
				}
				undo(app);
				return true;
			}else{
				if(view_get_cursor_pos(app, view) != view_get_mark_pos(app, view)){
					undo(app);
					return true;
				}
			}
		}

		// NOTE(BYP): Equally a hack for the same reason
		b32 result = has_modifier(event, KeyCode_Control);
		event->kind = InputEventKind_None;
		return result;
	}

	if(event->kind == InputEventKind_TextInsert){
		i64 pos = view_get_cursor_pos(app, view);
		Range_i64 range = Ii64(pos, pos + (buffer_get_char(app, buffer, pos) != '\n'));
		buffer_replace_range(app, buffer, range, event->text.string);
		move_right(app);
	}

	return true;
}

/// TODO(BYP): Decide what to do with Command and Menu
function void vim_append_keycode(Key_Code code){
#if VIM_USE_TRADITIONAL_CHORDS
	b32 has_mod = (code & (KeyMod_Ctl|KeyMod_Alt));
	if(has_mod){
		string_append_character(&vim_keystroke_text, '<');
		if(code & KeyMod_Ctl){ string_append_character(&vim_keystroke_text, 'C'); }
		if(code & KeyMod_Alt){ string_append_character(&vim_keystroke_text, 'M'); }
		string_append_character(&vim_keystroke_text, '-');
	}
	i32 index = (code & bitmask_8) + ((code & KeyMod_Sft) != 0)*KeyCode_COUNT;
	string_append(&vim_keystroke_text, SCu8(keycode_lut[index]));
	if(has_mod){
		string_append_character(&vim_keystroke_text, '>');
	}
#else
	if(code & KeyMod_Ctl){ string_append_character(&vim_keystroke_text, '^'); }
	if(code & KeyMod_Alt){ string_append_character(&vim_keystroke_text, '~'); }
	i32 index = (code & bitmask_8) + ((code & KeyMod_Sft) != 0)*KeyCode_COUNT;
	string_append(&vim_keystroke_text, SCu8(keycode_lut[index]));
#endif
}


function b32
vim_handle_input(Application_Links *app, Input_Event *event){
	switch(vim_state.mode){
		case VIM_Replace:       return vim_handle_replace_mode(app, event);
		case VIM_Visual_Insert: return vim_handle_visual_insert_mode(app, event);
	}


	bool result = false;
	if(event->kind == InputEventKind_KeyStroke){

		/// Translate the KeyCode
		Key_Code code = event->key.code;
		if(code != KeyCode_Control && code != KeyCode_Shift && code != KeyCode_Alt && code != KeyCode_Command && code != KeyCode_Menu){
			if(vim_state.chord_resolved){ vim_keystroke_text.size=0; vim_state.chord_resolved=false; }
			foreach(i, event->key.modifiers.count){
				Key_Code mod = event->key.modifiers.mods[i];
				if(0){}
				else if(mod == KeyCode_Control){ code |= KeyMod_Ctl; }
				else if(mod == KeyCode_Shift){   code |= KeyMod_Sft; }
				else if(mod == KeyCode_Alt){     code |= KeyMod_Alt; }
				else if(mod == KeyCode_Command){ code |= KeyMod_Cmd; }
				else if(mod == KeyCode_Menu){    code |= KeyMod_Mnu; }
			}
		}else{
			return true;
		}

		b32 was_in_sub_mode = (vim_state.sub_mode != SUB_None);
		u64 function_data=0;
		if(table_read(vim_maps + vim_state.mode + vim_state.sub_mode*VIM_MODE_COUNT, code, &function_data)){
			Custom_Command_Function *vim_func = (Custom_Command_Function *)IntAsPtr(function_data);
			if(vim_func){
				// Pre command stuff
				View_ID view = get_active_view(app, Access_ReadVisible);
				Managed_Scope scope = view_get_managed_scope(app, view);
				default_pre_command(app, scope);
				vim_pre_keystroke_size = vim_keystroke_text.size;
				vim_append_keycode(code);
				vim_state.active_command = vim_func;
				vim_state.chord_resolved = true;
				if(vim_func == no_op){ vim_state.chord_resolved = bitmask_2; }

				vim_func(app);

				// Post command stuff
				default_post_command(app, scope);
				vim_state.active_command = 0;

				result = true;
			}
		}else{
			if(vim_state.mode != VIM_Insert){
				String_ID map_id = vars_save_string_lit("keys_global");
				Command_Binding command_binding = map_get_binding_non_recursive(&framework_mapping, map_id, event);
				if(command_binding.custom){
					vim_reset_state();
					command_binding.custom(app);
					vim_keystroke_text.size = 0;
				}else{
					vim_append_keycode(code);
					vim_state.chord_resolved = bitmask_2;
				}
				result = true;
			}
		}
		if(was_in_sub_mode){ vim_state.sub_mode = SUB_None; }

		if(vim_keystroke_text.size >= vim_keystroke_text.cap){ vim_keystroke_text.size = 0; }

		return result;
	}

	return false;
}

function String_Const_u8 vim_get_bot_string(){
	String_Const_u8 result = vim_bot_text.string;

	if(vim_is_querying_user_key){ return result; }

	switch(vim_state.mode){
		case VIM_Insert:        result = string_u8_litexpr("-- INSERT --"); break;
		case VIM_Replace:       result = string_u8_litexpr("-- REPLACE --"); break;
		case VIM_Visual_Insert: result = string_u8_litexpr("-- VISUAL INSERT --"); break;
		case VIM_Visual:{
			switch(vim_state.params.edit_type){
				case EDIT_CharWise: result = string_u8_litexpr("-- VISUAL --");       break;
				case EDIT_LineWise: result = string_u8_litexpr("-- VISUAL LINE --");  break;
				case EDIT_Block:    result = string_u8_litexpr("-- VISUAL BLOCK --"); break;
			}
		} break;
	}

	if(vim_state.macro_char){
		local_persist u8 macro_string_buffer[] = "-- RECORDING   --";
		macro_string_buffer[13] = vim_state.macro_char;
		result = SCu8(macro_string_buffer, ArrayCount(macro_string_buffer)-1);
	}

	return result;
}

