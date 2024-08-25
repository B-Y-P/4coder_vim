
CUSTOM_COMMAND_SIG(vim_view_input_handler)
CUSTOM_DOC("Input consumption loop for vim behavior")
{
	Scratch_Block scratch(app);
	default_input_handler_init(app, scratch);

	View_ID view = get_this_ctx_view(app, Access_Always);
	Managed_Scope scope = view_get_managed_scope(app, view);

	for(;;){
		User_Input input = get_next_input(app, EventPropertyGroup_Any, 0);
		if(input.abort){ break; }

#if VIM_USE_BOTTOM_LISTER
		// Clicking on lister items outside of original view panel is a hack
		if(vim_lister_view_id != 0 && view != vim_lister_view_id){
			view_set_active(app, vim_lister_view_id);
			leave_current_input_unhandled(app);
			continue;
		}
#endif

		ProfileScopeNamed(app, "before view input", view_input_profile);

		// NOTE(allen): Mouse Suppression
		Event_Property event_properties = get_event_properties(&input.event);
		if((suppressing_mouse && (event_properties & EventPropertyGroup_AnyMouseEvent) != 0)){
			continue;
		}

		if(vim_handle_input(app, &input.event)){
			vim_cursor_blink = 0;
			continue;
		}

		// NOTE(allen): Get binding
		if(implicit_map_function == 0){
			implicit_map_function = default_implicit_map;
		}
		Implicit_Map_Result map_result = implicit_map_function(app, 0, 0, &input.event);
		if(map_result.command == 0){
			leave_current_input_unhandled(app);
			continue;
		}

		if(!(event_properties & EventPropertyGroup_AnyMouseEvent) && input.event.kind != InputEventKind_None){
			vim_keystroke_text.size = 0;
			vim_cursor_blink = 0;
		}

		// NOTE(allen): Run the command and pre/post command stuff
		default_pre_command(app, scope);
		ProfileCloseNow(view_input_profile);

		map_result.command(app);

		ProfileScope(app, "after view input");
		default_post_command(app, scope);
	}
}

// TODO(BYP): Reorganize and move this somewhere better
VIM_COMMAND_SIG(vim_insert_command){
	vim_state.mode = VIM_Normal;
	vim_state.chord_resolved = false;

	Scratch_Block scratch(app);
	default_input_handler_init(app, scratch);

	View_ID view = get_this_ctx_view(app, Access_Always);
	Managed_Scope scope = view_get_managed_scope(app, view);

	while(!vim_state.chord_resolved){
		vim_set_bottom_text(string_u8_litexpr("-- INSERT NORMAL --"));

		// Basically another view_input handling/mapping
		User_Input input = get_next_input(app, EventPropertyGroup_Any, EventProperty_Escape);
		if(input.abort){ break; }

		Event_Property event_properties = get_event_properties(&input.event);
		if((suppressing_mouse && (event_properties & EventPropertyGroup_AnyMouseEvent) != 0)){
			continue;
		}

		if(vim_handle_input(app, &input.event)){
			vim_cursor_blink = 0;
			continue;
		}

		if(implicit_map_function == 0){
			implicit_map_function = default_implicit_map;
		}
		Implicit_Map_Result map_result = implicit_map_function(app, 0, 0, &input.event);
		if(map_result.command == 0){
			leave_current_input_unhandled(app);
			continue;
		}

		if(!(event_properties & EventPropertyGroup_AnyMouseEvent) && input.event.kind != InputEventKind_None){
			vim_keystroke_text.size = 0;
			vim_cursor_blink = 0;
		}

		default_pre_command(app, scope);
		map_result.command(app);
		default_post_command(app, scope);
	}

	vim_reset_bottom_text();
	vim_state.mode = VIM_Insert;
}

function void
vim_file_save(Application_Links *app, Buffer_ID buffer_id){
	Scratch_Block scratch(app);
	String_Const_u8 unique_name = push_buffer_unique_name(app, scratch, buffer_id);
	i64 line_count = buffer_get_line_count(app, buffer_id);
	i64 bytes = buffer_get_size(app, buffer_id);
	String_Const_u8 msg = push_stringf(scratch, "\"%.*s\"  %dL, %dC written", string_expand(unique_name), line_count, bytes);
	vim_set_bottom_text(msg);
}

BUFFER_HOOK_SIG(vim_file_save_hook){
	default_file_save(app, buffer_id);
	vim_file_save(app, buffer_id);
	return 0;
}

function void vim_set_file_register(Application_Links *app, Buffer_ID buffer){
	Scratch_Block scratch(app);
	String_Const_u8 unique_name = push_buffer_unique_name(app, scratch, buffer);
	Vim_Register *reg = &vim_registers.file;
	b32 valid = vim_register_copy(reg, unique_name);
	if(!valid){ return; }
	vim_update_registers(app);
}

function void
vim_view_change_buffer(Application_Links *app, View_ID view_id, Buffer_ID old_buffer_id, Buffer_ID new_buffer_id){
	vim_set_file_register(app, new_buffer_id);
}

function void
vim_begin_buffer_inner(Application_Links *app, Buffer_ID buffer_id){
	Managed_Scope scope = buffer_get_managed_scope(app, buffer_id);
	Vim_Prev_Visual *prev_visual = scope_attachment(app, scope, vim_buffer_prev_visual, Vim_Prev_Visual);
	prev_visual->cursor_pos = prev_visual->mark_pos = 0;

	i64 *marks = (i64 *)managed_scope_get_attachment(app, scope, vim_buffer_marks, 26*sizeof(i64));
	block_fill_u64(marks, 26*sizeof(i64), max_u64);

	b32 *wrap_lines_ptr = scope_attachment(app, scope, buffer_wrap_lines, b32);
	*wrap_lines_ptr = false;
}

BUFFER_HOOK_SIG(vim_begin_buffer){
	//default_begin_buffer(app, buffer_id);
	fold_begin_buffer_hook(app, buffer_id);
	vim_begin_buffer_inner(app, buffer_id);
	return 0;
}

#define exp_interp(cur, nxt, dt, rate) (cur += (((nxt) - (cur))*(1.f - powf(rate, dt))))

function void
vim_animate_filebar(Application_Links *app, Frame_Info frame_info){
#if VIM_DO_ANIMATE
	exp_interp(vim_cur_filebar_offset, vim_nxt_filebar_offset, frame_info.animation_dt, 1e-14f);
	if(!near_zero(vim_cur_filebar_offset - vim_nxt_filebar_offset, 0.5f)){
		animate_in_n_milliseconds(app, 0);
	}else{
		vim_cur_filebar_offset = vim_nxt_filebar_offset;
	}
#else
	vim_cur_filebar_offset = vim_nxt_filebar_offset;
#endif
}


function void
vim_animate_cursor(Application_Links *app, Frame_Info frame_info){
#if VIM_DO_ANIMATE
	exp_interp(vim_cur_cursor_pos, vim_nxt_cursor_pos, frame_info.animation_dt, 1e-14f);
	if(!near_zero(vim_cur_cursor_pos - vim_nxt_cursor_pos, 0.5f)){
		animate_in_n_milliseconds(app, 0);
	}else{
		vim_cur_cursor_pos = vim_nxt_cursor_pos;
	}
#else
	vim_cur_cursor_pos = vim_nxt_cursor_pos;
#endif
}

BUFFER_EDIT_RANGE_SIG(vim_buffer_edit_range){
	default_buffer_edit_range(app, buffer_id, new_range, old_cursor_range);
	fold_buffer_edit_range_inner(app, buffer_id, new_range, old_cursor_range);
	// TODO(BYP): Update marks here as well
	return 0;
}

function void
vim_tick(Application_Links *app, Frame_Info frame_info){
	code_index_update_tick(app);
	if(tick_all_fade_ranges(app, frame_info.animation_dt)){
		animate_in_n_milliseconds(app, 0);
	}

	vim_animate_filebar(app, frame_info);
	vim_animate_cursor(app, frame_info);
#if VIM_DO_ANIMATE
	vim_cursor_blink += frame_info.animation_dt;
#endif

	fold_tick(app, frame_info);

	b32 enable_virtual_whitespace = def_get_config_b32(vars_save_string_lit("enable_virtual_whitespace"));
	if(enable_virtual_whitespace != def_enable_virtual_whitespace){
		def_enable_virtual_whitespace = enable_virtual_whitespace;
		clear_all_layouts(app);
	}
}

CUSTOM_COMMAND_SIG(vim_try_exit)
CUSTOM_DOC("Vim command for responding to a try-exit event")
{
	User_Input input = get_current_input(app);
	if(match_core_code(&input, CoreCode_TryExit)){
		b32 do_exit = true;
		if(!allow_immediate_close_without_checking_for_changes){
			b32 has_unsaved_changes = false;
			for(Buffer_ID buffer = get_buffer_next(app, 0, Access_Always);
				buffer;
				buffer = get_buffer_next(app, buffer, Access_Always))
			{
				Dirty_State dirty = buffer_get_dirty_state(app, buffer);
				if(HasFlag(dirty, DirtyState_UnsavedChanges)){
					has_unsaved_changes = true;
					break;
				}
			}
			if(has_unsaved_changes){
				View_ID view = get_active_view(app, Access_Always);
				do_exit = vim_do_4coder_close_user_check(app, view);
			}
		}
		if(do_exit){
			hard_exit(app);
		}
	}
}
