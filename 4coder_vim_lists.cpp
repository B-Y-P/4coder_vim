
function void
vim__fill_command_lister(Arena *arena, Lister *lister, i32 *command_ids, i32 command_id_count, Command_Lister_Status_Rule *status_rule){
	if(command_ids == 0){ command_id_count = command_one_past_last_id; }

	for(i32 i=0; i<command_id_count; i++){
		i32 j = (command_ids ? command_ids[i] : i);
		j = clamp(0, j, command_one_past_last_id);

		Custom_Command_Function *proc = fcoder_metacmd_table[j].proc;

		Command_Trigger_List triggers = map_get_triggers_recursive(arena, status_rule->mapping, status_rule->map_id, proc);

		List_String_Const_u8 list = {};
		if(triggers.first == 0){
			string_list_push(arena, &list, string_u8_litexpr(""));
		}
		for(Command_Trigger *node=triggers.first; node; node=node->next){
			command_trigger_stringize(arena, &list, node);
			if(node->next){
				string_list_push(arena, &list, string_u8_litexpr(" "));
			}
		}

		String_Const_u8 key_bind = string_list_flatten(arena, list);
		String_Const_u8 description = SCu8(fcoder_metacmd_table[j].description);
		String_Const_u8 status = push_stringf(arena, "%.*s\n%.*s", string_expand(key_bind), string_expand(description));

		lister_add_item(lister, SCu8(fcoder_metacmd_table[j].name), status, (void*)proc, 0);
	}
}

function Custom_Command_Function*
vim_get_command_from_user(Application_Links *app, i32 *command_ids, i32 command_id_count, Command_Lister_Status_Rule *status_rule){

	Scratch_Block scratch(app);
	Lister_Block lister(app, scratch);
	vim_lister_set_default_handlers(lister);
	lister_set_query(lister, string_u8_litexpr("Command:"));
	vim__fill_command_lister(scratch, lister, command_ids, command_id_count, status_rule);

#if VIM_USE_BOTTOM_LISTER
	vim_reset_bottom_text();
	string_append(&vim_bot_text, string_u8_litexpr(":"));
#endif
	Lister_Result l_result = vim_run_lister(app, lister);

	return (l_result.canceled ? 0 : (Custom_Command_Function *)l_result.user_data);
}

CUSTOM_UI_COMMAND_SIG(vim_command_mode)
CUSTOM_DOC("Enter Command Mode")
{
	View_ID view = get_this_ctx_view(app, Access_Always);
	if(view == 0){ return; }
	Command_Lister_Status_Rule rule = {};
	Buffer_ID buffer = view_get_buffer(app, view, Access_Visible);
	Managed_Scope buffer_scope = buffer_get_managed_scope(app, buffer);
	Command_Map_ID *map_id_ptr = scope_attachment(app, buffer_scope, buffer_map_id, Command_Map_ID);
	if(map_id_ptr){
		rule = command_lister_status_bindings(&framework_mapping, *map_id_ptr);
	}else{
		rule = command_lister_status_descriptions();
	}

	Custom_Command_Function *func = vim_get_command_from_user(app, 0, 0, &rule);
	if(func != 0){
		view_enqueue_command_function(app, view, func);
	}
}

function void
vim_generate_hot_directory_file_list(Application_Links *app, Lister *lister){
	Scratch_Block scratch(app, lister->arena);

	Temp_Memory temp = begin_temp(lister->arena);
	String_Const_u8 hot = push_hot_directory(app, lister->arena);
	if (!character_is_slash(string_get_character(hot, hot.size - 1))){
		hot = push_u8_stringf(lister->arena, "%.*s/", string_expand(hot));
	}
	lister_set_text_field(lister, hot);
	lister_set_key(lister, string_front_of_path(hot));

	File_List file_list = system_get_file_list(scratch, hot);
	end_temp(temp);

	File_Info **one_past_last = file_list.infos + file_list.count;

	lister_begin_new_item_set(app, lister);

	hot = push_hot_directory(app, lister->arena);
	push_align(lister->arena, 8);
	if (hot.str != 0){
		String_Const_u8 empty_string = string_u8_litexpr("");
		Lister_Prealloced_String empty_string_prealloced = lister_prealloced(empty_string);
		for (File_Info **info = file_list.infos;
			 info < one_past_last;
			 info += 1){
			if (!HasFlag((**info).attributes.flags, FileAttribute_IsDirectory)) continue;
			String_Const_u8 file_name = push_u8_stringf(lister->arena, "%.*s/",
														string_expand((**info).file_name));
			lister_add_item(lister, lister_prealloced(file_name), empty_string_prealloced, file_name.str, 0);
			;
		}

		for (File_Info **info = file_list.infos;
			 info < one_past_last;
			 info += 1){
			if (HasFlag((**info).attributes.flags, FileAttribute_IsDirectory)) continue;
			String_Const_u8 file_name = push_string_copy(lister->arena, (**info).file_name);
			char *is_loaded = "";
			char *status_flag = "";

			Buffer_ID buffer = {};

			{
				Temp_Memory path_temp = begin_temp(lister->arena);
				List_String_Const_u8 list = {};
				string_list_push(lister->arena, &list, hot);
				string_list_push_overlap(lister->arena, &list, '/', (**info).file_name);
				String_Const_u8 full_file_path = string_list_flatten(lister->arena, list);
				buffer = get_buffer_by_file_name(app, full_file_path, Access_Always);
				end_temp(path_temp);
			}

			if (buffer != 0){
				is_loaded = "LOADED";
				Dirty_State dirty = buffer_get_dirty_state(app, buffer);
				switch (dirty){
					case DirtyState_UnsavedChanges:  status_flag = " *"; break;
					case DirtyState_UnloadedChanges: status_flag = " !"; break;
					case DirtyState_UnsavedChangesAndUnloadedChanges: status_flag = " *!"; break;
				}
			}
			String_Const_u8 status = push_u8_stringf(lister->arena, "%s%s", is_loaded, status_flag);
			lister_add_item(lister, lister_prealloced(file_name), lister_prealloced(status), file_name.str, 0);
		}
	}
}

function Lister_Choice*
vim_get_choice_from_user(Application_Links *app, String_Const_u8 query, Lister_Choice_List list){
    Scratch_Block scratch(app);
    Lister_Block lister(app, scratch);
    for(Lister_Choice *choice = list.first; choice; choice = choice->next){
        u64 code_size = sizeof(choice->key_code);
        void *extra = lister_add_item(lister, choice->string, choice->status, choice, code_size);
        block_copy(extra, &choice->key_code, code_size);
    }
    lister_set_query(lister, query);
    Lister_Handlers handlers = {};
    handlers.navigate        = lister__navigate__default;
    handlers.key_stroke      = lister__key_stroke__choice_list;
    lister_set_handlers(lister, &handlers);

    Lister_Result l_result = vim_run_lister(app, lister);
    Lister_Choice *result = 0;
    if(!l_result.canceled){
        result = (Lister_Choice*)l_result.user_data;
    }
    return result;
}

function b32
vim_query_create_folder(Application_Links *app, String_Const_u8 folder_name){
	Scratch_Block scratch(app);
    Lister_Choice_List list = {};
    lister_choice(scratch, &list, "(N)o"  , "", KeyCode_N, SureToKill_No);
    lister_choice(scratch, &list, "(Y)es" , "", KeyCode_Y, SureToKill_Yes);

    String_Const_u8 message = push_u8_stringf(scratch, "Create the folder %.*s?", string_expand(folder_name));
    Lister_Choice *choice = vim_get_choice_from_user(app, message, list);

	b32 did_create_folder = false;
    if(choice != 0){
        switch(choice->user_data){
            case SureToCreateFolder_No:{} break;

            case SureToCreateFolder_Yes:{
				String_Const_u8 hot = push_hot_directory(app, scratch);
                String_Const_u8 fixed_folder_name = push_string_copy(scratch, folder_name);
                foreach(i, fixed_folder_name.size){
					if(fixed_folder_name.str[i] == '/'){ fixed_folder_name.str[i] = '\\'; }
				}
                if(fixed_folder_name.size > 0){
                    String_Const_u8 cmd = push_u8_stringf(scratch, "mkdir %.*s", string_expand(fixed_folder_name));
                    exec_system_command(app, 0, buffer_identifier(0), hot, cmd, 0);
                    did_create_folder = true;
				}
            } break;
        }
    }

    return(did_create_folder);
}

CUSTOM_UI_COMMAND_SIG(vim_interactive_open_or_new)
CUSTOM_DOC("Interactively open a file out of the file system.")
{
	for(;;){
		Scratch_Block scratch(app);
		View_ID view = get_this_ctx_view(app, Access_Always);
		File_Name_Result result = vim_get_file_name_from_user(app, scratch, SCu8("Open/New:"), view);
		if(result.canceled || result.path_in_text_field.str == 0){ break; }

		String_Const_u8 file_name = result.file_name_activated;
		if(file_name.size == 0){ file_name = result.file_name_in_text_field; }

		String_Const_u8 path = result.path_in_text_field;
		String_Const_u8 full_file_name =
			push_u8_stringf(scratch, "%.*s/%.*s", string_expand(path), string_expand(file_name));

		if(result.is_folder){
			set_hot_directory(app, full_file_name);
			continue;
		}

		if(character_is_slash(file_name.str[file_name.size - 1])){
			File_Attributes attribs = system_quick_file_attributes(scratch, full_file_name);
			if(HasFlag(attribs.flags, FileAttribute_IsDirectory)){
				set_hot_directory(app, full_file_name);
				continue;
			}
			if(string_looks_like_drive_letter(file_name)){
				set_hot_directory(app, file_name);
				continue;
			}
			if(vim_query_create_folder(app, full_file_name)){
				set_hot_directory(app, full_file_name);
				continue;
			}else{
				set_hot_directory(app, result.path_in_text_field);
				continue;
			}
			break;
		}

		Buffer_ID buffer = create_buffer(app, full_file_name, 0);
		if(buffer != 0){ view_set_buffer(app, view, buffer, 0); }
		break;
	}
}


CUSTOM_UI_COMMAND_SIG(vim_theme_lister)
CUSTOM_DOC("Opens an interactive list of all registered themes.")
{
	Color_Table_List *color_table_list = &global_theme_list;

	Scratch_Block scratch(app);
	Lister_Block lister(app, scratch);
	vim_lister_set_default_handlers(lister);
	vim_reset_bottom_text();

	lister_add_item(lister, string_u8_litexpr("4coder"), string_u8_litexpr(""),
					(void*)&default_color_table, 0);

	for(Color_Table_Node *node = color_table_list->first; node; node = node->next){
		lister_add_item(lister, node->name, string_u8_litexpr(""), (void*)&node->table, 0);
	}

#if VIM_USE_BOTTOM_LISTER
	string_append(&vim_bot_text, string_u8_litexpr("Theme:"));
#endif
	Lister_Result l_result = vim_run_lister(app, lister);

	Color_Table *result = 0;
	if(!l_result.canceled){ result = (Color_Table*)l_result.user_data; }

	if(result != 0){ active_color_table = *result; }
}

CUSTOM_UI_COMMAND_SIG(vim_switch_lister)
CUSTOM_DOC("Opens an interactive list of all loaded buffers.")
{
	Lister_Handlers handlers = lister_get_default_handlers();
	handlers.refresh = generate_all_buffers_list;
	handlers.backspace = vim_lister__backspace;
	Scratch_Block scratch(app);
#if VIM_USE_BOTTOM_LISTER
	vim_reset_bottom_text();
	string_append(&vim_bot_text, string_u8_litexpr("Switch:"));
#endif
	Lister_Result l_result = vim_run_lister_with_refresh_handler(app, scratch, string_u8_litexpr("Switch:"), handlers);
	Buffer_ID buffer = 0;
	if (!l_result.canceled){
		buffer = (Buffer_ID)(PtrAsInt(l_result.user_data));
	}
	if (buffer != 0){
		View_ID view = get_this_ctx_view(app, Access_Always);
		view_set_buffer(app, view, buffer, 0);
	}
}


function Jump_Lister_Result
vim_get_jump_index_from_user(Application_Links *app, Marker_List *list,
                             String_Const_u8 query){
	Jump_Lister_Result result = {};
	if (list != 0){
		Scratch_Block scratch(app);
		Lister_Block lister(app, scratch);
		lister_set_query(lister, query);
		lister_set_default_handlers(lister);

		Buffer_ID list_buffer = list->buffer_id;

		i32 option_count = list->jump_count;
		Managed_Object stored_jumps = list->jump_array;
		for (i32 i = 0; i < option_count; i += 1){
			Sticky_Jump_Stored stored = {};
			managed_object_load_data(app, stored_jumps, i, 1, &stored);
			String_Const_u8 line = push_buffer_line(app, scratch, list_buffer,
													stored.list_line);
			lister_add_item(lister, line, SCu8(), IntAsPtr(i), 0);
		}

		Lister_Result l_result = vim_run_lister(app, lister);
		if (!l_result.canceled){
			result.success = true;
			result.index = (i32)PtrAsInt(l_result.user_data);
		}
	}

	return(result);
}

function Jump_Lister_Result
vim_get_jump_index_from_user(Application_Links *app, Marker_List *list, char *query){
	return(vim_get_jump_index_from_user(app, list, SCu8(query)));
}


CUSTOM_UI_COMMAND_SIG(vim_list_all_functions_current_buffer_lister)
CUSTOM_DOC("Creates a lister of locations that look like function definitions and declarations in the buffer.")
{
	Heap *heap = &global_heap;
	View_ID view = get_active_view(app, Access_ReadVisible);
	Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);
	if (buffer != 0){
		list_all_functions(app, buffer);
		view = get_active_view(app, Access_Always);
		buffer = view_get_buffer(app, view, Access_Always);
		Marker_List *list = get_or_make_list_for_buffer(app, heap, buffer);
		if (list != 0){
			Jump_Lister_Result jump = vim_get_jump_index_from_user(app, list, "Function:");
			jump_to_jump_lister_result(app, view, list, &jump);
		}
	}
}

CUSTOM_UI_COMMAND_SIG(vim_proj_cmd_lister)
CUSTOM_DOC("Opens an interactive list of all project commands.")
{
	Variable_Handle prj_var = vars_read_key(vars_get_root(), vars_save_string_lit("prj_config"));

	Scratch_Block scratch(app);
	Lister_Block lister(app, scratch);

	Lister_Handlers handlers = lister_get_default_handlers();
	handlers.backspace = vim_lister__backspace;
	lister_set_handlers(lister, &handlers);

	vim_reset_bottom_text();
	string_append(&vim_bot_text, string_u8_litexpr("Command:"));

	Variable_Handle cmd_list_var = vars_read_key(prj_var, vars_save_string_lit("commands"));
	String_ID os_id = vars_save_string_lit(OS_NAME);

	i32 i=1;
	for(Variable_Handle cmd=vars_first_child(cmd_list_var); !vars_is_nil(cmd); cmd=vars_next_sibling(cmd), i++){
		Variable_Handle os_cmd = vars_read_key(cmd, os_id);
		if(!vars_is_nil(os_cmd)){
			String8 cmd_name = vars_key_from_var(scratch, cmd);
			String8 os_cmd_str = vars_string_from_var(scratch, os_cmd);
			u8 *f_str = push_array(scratch, u8, 3);
			f_str[0] = 'F';
			f_str[1] = '0' + u8(i%10);
			f_str[2] = '0' + u8(i/10);
			//lister_add_item(lister, cmd_name, os_cmd_str, cmd.ptr, 0);
			lister_add_item(lister, cmd_name, SCu8(f_str, 2+(i>9)), cmd.ptr, 0);
		}
	}

	Variable_Handle prj_cmd = vars_get_nil();
	Lister_Result l_result = vim_run_lister(app, lister);
	if(!l_result.canceled && l_result.user_data){
		prj_cmd.ptr = (Variable*)l_result.user_data;
	}
	if(!vars_is_nil(prj_cmd)){
		prj_exec_command(app, prj_cmd);
	}
}

function void vim_jump_navigate(Application_Links *app, View_ID view, Lister *lister, i32 index_delta){
	lister__navigate__default(app, view, lister, index_delta);

	Managed_Scope scope = view_get_managed_scope(app, view);
	Vim_Jump_List *jump_list = scope_attachment(app, scope, vim_view_jumps, Vim_Jump_List);
	i32 index = i32(PtrAsInt(lister_get_user_data(lister, lister->raw_item_index)));
	if(!in_range(0, index, ArrayCount(jump_list->markers))){ return; }
	Point_Stack_Slot *slot = &jump_list->markers[index];
	view_set_buffer(app, view, slot->buffer, SetBuffer_KeepOriginalGUI);
	view_set_cursor_and_preferred_x(app, view, seek_pos(slot->object));
	center_view(app);
}

// TODO(BYP): Jump lister crashed on many entries
CUSTOM_UI_COMMAND_SIG(vim_jump_lister)
CUSTOM_DOC("Opens an interactive lists of the views jumps")
{
	Scratch_Block scratch(app);
	Lister_Block lister(app, scratch);
	vim_lister_set_default_handlers(lister);
	lister.lister.current->handlers.navigate = vim_jump_navigate;

	View_ID view = get_active_view(app, Access_ReadVisible);
	Managed_Scope scope = view_get_managed_scope(app, view);
	Vim_Jump_List *jump_list = scope_attachment(app, scope, vim_view_jumps, Vim_Jump_List);
	for(i32 i=jump_list->index; i != jump_list->bot; i=ArrayDec(jump_list->markers, i)){
		Point_Stack_Slot *slot = jump_list->markers + i;
		i64 line = get_line_number_from_pos(app, slot->buffer, i64(slot->object));

		String_Const_u8 line_text = push_buffer_line(app, scratch, slot->buffer, line);
		b32 blank = true;
		foreach(j, line_text.size){
			if(!character_is_whitespace(line_text.str[j])){ blank = false; break; }
		}
		if(blank){ line_text = string_u8_litexpr("*blank*"); }
		line_text.size = Min(line_text.size, 20);
		String_Const_u8 unique_name = push_buffer_unique_name(app, scratch, slot->buffer);
		String_Const_u8 text = push_stringf(scratch, "[%.*s]:(%d)", string_expand(unique_name), line);
		lister_add_item(lister, text, line_text, IntAsPtr(i), 0);
	}
	Lister_Result l_result = vim_run_lister(app, lister);
	i32 index = (!l_result.canceled ? i32(PtrAsInt(l_result.user_data)) : jump_list->index);
	vim_set_jump(app, view, jump_list, index);
}


function b32
vim_do_buffer_close_user_check(Application_Links *app, Buffer_ID buffer, View_ID view){
    Scratch_Block scratch(app);
    Lister_Choice_List list = {};
    lister_choice(scratch, &list, "(N)o"  , "", KeyCode_N, SureToKill_No);
    lister_choice(scratch, &list, "(Y)es" , "", KeyCode_Y, SureToKill_Yes);
    lister_choice(scratch, &list, "(S)ave", "", KeyCode_S, SureToKill_Save);

    Lister_Choice *choice = vim_get_choice_from_user(app, string_u8_litexpr("There are unsaved changes, close anyway?"), list);

    b32 do_kill = false;
    if (choice != 0){
        switch (choice->user_data){
            case SureToKill_No:{} break;

            case SureToKill_Yes:{ do_kill = true; } break;

            case SureToKill_Save:{
                String_Const_u8 file_name = push_buffer_file_name(app, scratch, buffer);
                if(buffer_save(app, buffer, file_name, BufferSave_IgnoreDirtyFlag)){
                    do_kill = true;
                }else{
#define M "Did not close '%.*s' because it did not successfully save."
                    String_Const_u8 str = push_u8_stringf(scratch, M, string_expand(file_name));
#undef M
                    print_message(app, str);
                }
            } break;
        }
    }

    return do_kill;
}

function Buffer_Kill_Result
vim_try_buffer_kill(Application_Links *app){
	View_ID view = get_active_view(app, Access_ReadVisible);
	Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);
	Buffer_Kill_Flag flags = 0;
	Buffer_Kill_Result result = buffer_kill(app, buffer, flags);
	if(result == BufferKillResult_Dirty){
		if(vim_do_buffer_close_user_check(app, buffer, view)){
			result = buffer_kill(app, buffer, BufferKill_AlwaysKill);
		}
	}
	return result;
}

function b32
vim_do_4coder_close_user_check(Application_Links *app, View_ID view){
	Scratch_Block scratch(app);
	Lister_Choice_List list = {};
	lister_choice(scratch, &list, "(N)o"  , "", KeyCode_N, SureToKill_No);
	lister_choice(scratch, &list, "(Y)es" , "", KeyCode_Y, SureToKill_Yes);
	lister_choice(scratch, &list, "(S)ave all and close", "", KeyCode_S, SureToKill_Save);

#define M "There are one or more buffers with unsave changes, close anyway?"
	Lister_Choice *choice = vim_get_choice_from_user(app, string_u8_litexpr(M), list);
#undef M

	b32 do_exit = false;
	if(choice != 0){
		switch(choice->user_data){
			case SureToKill_No:{} break;

			case SureToKill_Yes:{
				allow_immediate_close_without_checking_for_changes = true;
				do_exit = true;
			} break;

			case SureToKill_Save:{
				save_all_dirty_buffers(app);
				allow_immediate_close_without_checking_for_changes = true;
				do_exit = true;
			} break;
		}
	}

	return do_exit;
}

function void
vim_reload_external_changes_lister(Application_Links *app, Buffer_ID buffer){
	Scratch_Block scratch(app);
	Lister_Choice_List list = {};
	String_Const_u8 buffer_name = push_buffer_base_name(app, scratch, buffer);

	if(buffer_name.size == 0){ return; }
	lister_choice(scratch, &list, buffer_name, "", KeyCode_Q, u64(0));
	lister_choice(scratch, &list, "(L)oad external changes"  , "", KeyCode_L, u64(1));
	lister_choice(scratch, &list, "(K)eep current buffer" , "", KeyCode_K, u64(0));

#define M "External changes have been detected. Reload buffer from file?"
	Lister_Choice *choice = vim_get_choice_from_user(app, string_u8_litexpr(M), list);
#undef M

	if(choice != 0 && choice->user_data){
		buffer_reopen(app, buffer, 0);
	}
}

// NOTE(BYP): This hook isn't officially supported by core (it will false positive) it was just fun to write
CUSTOM_COMMAND_SIG(vim_file_externally_modified){
	User_Input input = get_current_input(app);
	if(match_core_code(&input, CoreCode_FileExternallyModified)){
		print_message(app, input.event.core.string);
		vim_reload_external_changes_lister(app, input.event.core.id);
	}
}

