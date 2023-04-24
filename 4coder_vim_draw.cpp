
function void
vim_draw_visual_mode(Application_Links *app, View_ID view, Buffer_ID buffer, Face_ID face_id, Text_Layout_ID text_layout_id){
	Range_i64 range = get_view_range(app, view);
	
	ARGB_Color text_color = fcolor_resolve(fcolor_id(defcolor_at_highlight));
	switch(vim_state.params.edit_type){
		case EDIT_Block:{
			Rect_f32 block_rect = vim_get_abs_block_rect(app, view, buffer, text_layout_id, range);
			draw_rectangle_fcolor(app, block_rect, 5.f, fcolor_id(defcolor_highlight));
			
			i64 line_min = get_line_number_from_pos(app, buffer, range.min);
			i64 line_max = get_line_number_from_pos(app, buffer, range.max);
			f32 line_advance = rect_height(block_rect)/f32(Max(1, line_max-line_min));
			f32 wid = rect_width(block_rect);
			
			Range_i64 visible_range = text_layout_get_visible_range(app, text_layout_id);
			Range_i64 test_range = range_intersect(visible_range, range);
			i64 test_line_min = get_line_number_from_pos(app, buffer, test_range.min);
			i64 test_line_max = get_line_number_from_pos(app, buffer, test_range.max);
			
			ARGB_Color helper_color = fcolor_resolve(fcolor_id(defcolor_mark));
			for(i64 i=test_line_min; i<=test_line_max; i++){
				if(line_is_valid_and_blank(app, buffer, i) && i != line_min && i != line_max){ continue; }
				Vec2_f32 min_point = block_rect.p0 + V2f32(0, line_advance*(i-line_min));
				Vec2_f32 max_point = min_point + V2f32(wid,0);
				i64 min_pos = view_pos_from_xy(app, view, min_point);
				i64 max_pos = view_pos_from_xy(app, view, max_point);
				paint_text_color(app, text_layout_id, Ii64(min_pos, max_pos), text_color);
				
				if(!vim_show_block_helper || min_pos == max_pos){ continue; }
				Rect_f32 min_rect = text_layout_character_on_screen(app, text_layout_id, min_pos);
				Rect_f32 max_rect = text_layout_character_on_screen(app, text_layout_id, max_pos-1);
				draw_rectangle(app, rect_split_top_bottom_neg(min_rect, 3.f).b, 3.0f, helper_color);
				draw_rectangle(app, rect_split_top_bottom(max_rect,     3.f).a, 3.0f, helper_color);
				draw_rectangle(app, rect_split_left_right(min_rect,     3.f).a, 3.0f, helper_color);
				draw_rectangle(app, rect_split_left_right_neg(max_rect, 3.f).b, 3.0f, helper_color);
			}
		} break;
		
		case EDIT_LineWise:{
			range.min = get_line_side_pos_from_pos(app, buffer, range.min, Side_Min);
			range.max = get_line_side_pos_from_pos(app, buffer, range.max, Side_Max) + 1; // highlight newlines
			
			if(vim_do_full_line){
				Range_i64 line_range = Ii64(get_line_number_from_pos(app, buffer, range.min),
											get_line_number_from_pos(app, buffer, range.max)-1);
				draw_line_highlight(app, text_layout_id, line_range, fcolor_id(defcolor_highlight));
				paint_text_color(app, text_layout_id, range, text_color);
				break;
			}
			
			// TODO(BYP): There may still be weird edge cases here
			/// Virtual Whitespace handling
			Managed_Scope scope = buffer_get_managed_scope(app, buffer);
			Command_Map_ID *map_id_ptr = scope_attachment(app, scope, buffer_map_id, Command_Map_ID);
			b32 is_code = *map_id_ptr == i64(vars_save_string_lit("keys_code"));
			b32 virtual_enabled = def_get_config_b32(vars_save_string_lit("enable_virtual_whitespace"));
			if((virtual_enabled && is_code) && character_is_whitespace(buffer_get_char(app, buffer, range.min))){
				//range.min = get_pos_past_lead_whitespace(app, buffer, range.min);
				i64 line_end = get_line_end_pos(app, buffer, get_line_number_from_pos(app, buffer, range.min));
				line_end -= (line_end > 0 && buffer_get_char(app, buffer, line_end) == '\n' && buffer_get_char(app, buffer, line_end-1) == '\r');
				i64 non_ws = buffer_seek_character_class_change_1_0(app, buffer, &character_predicate_whitespace, Scan_Forward, range.min);
				range.min = Min(line_end, non_ws);
			}
			// Fall through
		}
		
		case EDIT_CharWise:{
			if(vim_state.params.edit_type != EDIT_LineWise){ range.max++; }
			draw_character_block(app, text_layout_id, range, 5.f, fcolor_id(defcolor_highlight));
			paint_text_color(app, text_layout_id, range, text_color);
		} break;
	}
}

function void
vim_draw_filebar(Application_Links *app, View_ID view_id, Buffer_ID buffer, Frame_Info frame_info, Face_ID face_id, Rect_f32 bar){
	Scratch_Block scratch(app);
	String_Const_u8 unique_name = push_buffer_unique_name(app, scratch, buffer);
	
	draw_rectangle_fcolor(app, bar, 0.f, fcolor_id(defcolor_bar));
	
	f32 char_wid = get_face_metrics(app, face_id).normal_advance;
	Rect_f32 title_rect = bar;
	title_rect.x1 = bar.x0 + char_wid*unique_name.size;
	draw_rectangle_fcolor(app, title_rect, 0.f, fcolor_id(defcolor_vim_filebar_pop));
	
	Rect_f32 triangle_rect = title_rect;
	f32 radius_fudge = 5.f;
	triangle_rect.x0 = title_rect.x1 - radius_fudge*char_wid;
	triangle_rect.x1 = title_rect.x1 + radius_fudge*char_wid;
	draw_rectangle_fcolor(app, triangle_rect, radius_fudge*char_wid, fcolor_id(defcolor_vim_filebar_pop));
	
	FColor base_color = fcolor_id(defcolor_base);
	FColor pop2_color = fcolor_id(defcolor_pop2);
	
	i64 cursor_position = view_get_cursor_pos(app, view_id);
	Buffer_Cursor cursor = view_compute_cursor(app, view_id, seek_pos(cursor_position));
	
	u8 space[5];
	String_u8 str = Su8(space, 0, 4);
	
	Managed_Scope scope = buffer_get_managed_scope(app, buffer);
	Line_Ending_Kind *eol_kind = scope_attachment(app, scope, buffer_eol_setting, Line_Ending_Kind);
	switch(*eol_kind){
		case LineEndingKind_Binary:{ string_append(&str, string_u8_litexpr("bin"));  } break;
		case LineEndingKind_LF:    { string_append(&str, string_u8_litexpr("lf"));   } break;
		case LineEndingKind_CRLF:  { string_append(&str, string_u8_litexpr("crlf")); } break;
	}
	
	
	Vec2_f32 p = V2f32(title_rect.x1 + 4.5f*char_wid, bar.y0 + 3.f);
	p = draw_string(app, face_id, str.string, p, base_color);
	
	str = Su8(space, 0, 5);
	Dirty_State dirty = buffer_get_dirty_state(app, buffer);
	if(dirty != 0){
		string_append(&str, string_u8_litexpr(" ["));
		if(HasFlag(dirty, DirtyState_UnsavedChanges))
			string_append(&str, string_u8_litexpr("+"));
		if(HasFlag(dirty, DirtyState_UnloadedChanges))
			string_append(&str, string_u8_litexpr("!"));
		string_append(&str, string_u8_litexpr("]"));
		draw_string(app, face_id, str.string, p, pop2_color);
	}
	
	p.x = Max(p.x + 5.f*char_wid, bar.x1 - char_wid*15.f);
	draw_string(app, face_id, push_stringf(scratch, "%d,%d", cursor.line, cursor.col), p, base_color);
	
	p.x = bar.x0 + 2.f;
	draw_string(app, face_id, unique_name, p, base_color);
	
	p.x = bar.x1 - char_wid*3.5f;
	i64 buffer_size = buffer_get_size(app, buffer);
	String_Const_u8 PosText;
	if(cursor_position == 0){
		PosText = string_u8_litexpr("Top");
	}else if(cursor_position ==  buffer_size){
		PosText = string_u8_litexpr("Bot");
	}else{
		PosText = push_stringf(scratch, "%d%%", i64(100.f*cursor_position/(buffer_size)));
	}
	draw_string(app, face_id, PosText, p, base_color);
}

function void
vim_draw_search_highlight(Application_Links *app, View_ID view, Buffer_ID buffer, Text_Layout_ID text_layout_id, f32 roundness){
	String_u8 *pattern = &vim_registers.search.data;
	if(pattern->size == 0){ return; }
	Range_i64 visible_range = text_layout_get_visible_range(app, text_layout_id);
	i64 buffer_size = buffer_get_size(app, buffer);
	i64 cur_pos = visible_range.min;
	while(cur_pos < visible_range.max){
		i64 new_pos = 0;
		seek_string_forward(app, buffer, cur_pos, 0, pattern->string, &new_pos);
		if(new_pos == 0 || new_pos == buffer_size){ break; }
		else{
			cur_pos = new_pos;
			Rect_f32 rect = text_layout_character_on_screen(app, text_layout_id, cur_pos);
			rect.x1 = rect.x0 + pattern->size*(rect_width(rect));
			draw_rectangle_fcolor(app, rect, roundness, fcolor_id(defcolor_highlight));
		}
	}
}

function void
vim_draw_cursor(Application_Links *app, View_ID view, b32 is_active_view, Buffer_ID buffer, Text_Layout_ID text_layout_id, f32 roundness, f32 thickness){
	
	if(is_active_view && vim_state.mode == VIM_Visual_Insert){
		animate_in_n_milliseconds(app, 0);
		
		if(ACTIVE_BLINK(vim_cursor_blink)){
			Range_i64 range = get_view_range(app, view);
			Rect_f32 block_rect = vim_get_abs_block_rect(app, view, buffer, text_layout_id, range);
			
			// TODO(BYP): Could use vim_show_block_helper to have nicer, more precise cursors like in vim_draw_visual_mode
			if(vim_visual_insert_after){
				block_rect.x0 = block_rect.x1 - 2.f;
			}else{
				block_rect.x1 = block_rect.x0 + 2.f;
			}
			draw_rectangle_fcolor(app, block_rect, 1.f, fcolor_id(defcolor_cursor));
		}
		return;
	}
	
	b32 has_highlight_range = draw_highlight_range(app, view, buffer, text_layout_id, roundness);
	if(!has_highlight_range){
		i32 cursor_sub_id = default_cursor_sub_id();
		
		i64 cursor_pos = view_get_cursor_pos(app, view);
		i64 mark_pos = view_get_mark_pos(app, view);
		if(is_active_view && vim_lister_view_id == 0){
			animate_in_n_milliseconds(app, 0);
			
			Rect_f32 rect = text_layout_character_on_screen(app, text_layout_id, cursor_pos);
			
			if(ACTIVE_BLINK(vim_cursor_blink) && !vim_is_selecting_register){
				if(vim_state.mode == VIM_Insert){ rect = rect_split_top_bottom_neg(rect, 5.f).b; }
				//if(vim_state.mode == VIM_Insert){ rect = rect_split_left_right(rect, 2.f).a; }
				if(rect.p1 != V2f32(0,0)){
					vim_nxt_cursor_pos = rect.p1;
				}
				
				Rect_f32 cursor_rect = Rf32_xy_wh(vim_cur_cursor_pos - rect_dim(rect), rect_dim(rect));
				draw_rectangle_fcolor(app, cursor_rect, roundness, fcolor_id(defcolor_cursor, cursor_sub_id));
				
				//Rect_f32 larger = rect_inner(rect, -1.f);
				//FColor fade_color = fcolor_resolve(fcolor_id(defcolor_cursor, cursor_sub_id));
				//fade_color = fcolor_change_alpha(fade_color, 255.f*(0.5f*sinf(vim_cursor_blink/3.f) + 0.5f));
				//draw_rectangle_fcolor(app, larger, roundness, fade_color);
				
				if(vim_state.mode != VIM_Insert){
					paint_text_color_pos(app, text_layout_id, cursor_pos, fcolor_id(defcolor_at_cursor));
				}
			}
			if(vim_state.mode == VIM_Insert){
				draw_character_wire_frame(app, text_layout_id, mark_pos,roundness, thickness, fcolor_id(defcolor_mark));
			}
		}else{
			draw_character_wire_frame(app, text_layout_id, cursor_pos, roundness, thickness, fcolor_id(defcolor_cursor, cursor_sub_id));
			if(vim_state.mode == VIM_Insert){
				draw_character_wire_frame(app, text_layout_id, mark_pos, roundness, thickness, fcolor_id(defcolor_mark));
			}
		}
	}
}

function void
vim_draw_after_text(Application_Links *app, View_ID view, b32 is_active_view, Buffer_ID buffer, Text_Layout_ID text_layout_id, f32 cursor_roundness, f32 mark_thickness){
	if(is_active_view && vim_is_selecting_register && vim_state.mode == VIM_Insert){
		i64 cursor_pos = view_get_cursor_pos(app, view);
		Rect_f32 cursor_rect = text_layout_character_on_screen(app, text_layout_id, cursor_pos);
		draw_rectangle_fcolor(app, cursor_rect, 0.f, fcolor_id(defcolor_back));
		if(!def_get_config_b32(vars_save_string_lit("highlight_line_at_cursor"))){
			draw_rectangle_fcolor(app, cursor_rect, 0.f, fcolor_id(defcolor_highlight_cursor_line));
		}
		vim_draw_cursor(app, view, is_active_view, buffer, text_layout_id, cursor_roundness, mark_thickness);
		draw_string(app, get_face_id(app, 0), string_u8_litexpr("\""), cursor_rect.p0, fcolor_id(defcolor_text_default));
	}
}

function Rect_f32
vim_draw_query_bars(Application_Links *app, Rect_f32 region, View_ID view_id, Face_ID face_id){
	Face_Metrics face_metrics = get_face_metrics(app, face_id);
	f32 line_height = face_metrics.line_height;
	
	Query_Bar *space[32];
	Query_Bar_Ptr_Array query_bars = {};
	query_bars.ptrs = space;
	if(get_active_query_bars(app, view_id, ArrayCount(space), &query_bars)){
		foreach(i,query_bars.count){
			Rect_f32_Pair pair = layout_query_bar_on_bot(region, line_height, 1);
			draw_rectangle_fcolor(app, pair.max, 0.f, fcolor_id(defcolor_back));
			draw_query_bar(app, query_bars.ptrs[i], face_id, pair.max);
			region = pair.min;
		}
	}
	return region;
}

function Rect_f32_Pair
vim_line_number_margin(Application_Links *app, Buffer_ID buffer, Rect_f32 rect, f32 digit_advance){
	i64 line_count = buffer_get_line_count(app, buffer);
	i64 digit_count = digit_count_from_integer(line_count, 10) + i64(vim_relative_numbers != 0);
	
	f32 margin_width = (f32)digit_count*digit_advance + 6.f;
	Rect_f32_Pair pair = rect_split_left_right(rect, margin_width);
	pair.a = rect_split_left_right(pair.a, 6.f).b;
	pair.b = rect_split_left_right(pair.b, 4.f).b;
	return pair;
}


function void
vim_draw_rel_line_number_margin(Application_Links *app, View_ID view, Buffer_ID buffer, Face_ID face, Text_Layout_ID text_layout_id, Rect_f32 margin){
	Rect_f32 prev_clip = draw_set_clip(app, margin);
	draw_rectangle_fcolor(app, margin, 0.f, fcolor_id(defcolor_line_numbers_back));
	
	const i64 cur_line = get_line_number_from_pos(app, buffer, view_get_cursor_pos(app, view));
	const i64 line_count = buffer_get_line_count(app, buffer);
	
	i64 cur_line_digit_count = digit_count_from_integer(cur_line, 10);
	i64 bot_line_digit_count = digit_count_from_integer(line_count, 10);
	i64 digit_count = Max(cur_line_digit_count+1, bot_line_digit_count);
	
	Range_i64 visible_range = text_layout_get_visible_range(app, text_layout_id);
	Buffer_Cursor cursor = view_compute_cursor(app, view, seek_pos(visible_range.first));
	Buffer_Cursor cursor_end = view_compute_cursor(app, view, seek_pos(visible_range.end));
	const i64 first_line_num = cursor.line;
	const i64 one_past_last = cursor_end.line;
	
	Scratch_Block scratch(app);
	u8 *digit_buffer = push_array(scratch, u8, digit_count);
	String_Const_u8 digit_string = SCu8(digit_buffer, digit_count);
	foreach(i, digit_count){ digit_buffer[i] = ' '; }
	
	u8 *small_digit = digit_buffer + (digit_count-1) - 1;
	u8 *ptr = small_digit;
	if(cur_line == 0){ *ptr = '0'; }
	else{
		for(u64 X=cur_line; X>0; X/=10){
			*ptr-- = '0' + (X%10);
		}
	}
	small_digit++;
	
	Range_f32 line_y = text_layout_line_on_screen(app, text_layout_id, cur_line);
	Vec2_f32 p = V2f32(margin.x0, line_y.min);
	
	// NOTE(BYP): This assumes background is darker than font color
	FColor text_color = fcolor_id(defcolor_line_numbers_text);
	FColor contrast_color = fcolor_blend(text_color, 0.4f, f_white);
	
	draw_string(app, face, digit_string, p, fcolor_resolve(contrast_color));
	
	i32 rel_num = 1;
	foreach(i, digit_count-1){ digit_buffer[i] = ' '; }
	digit_buffer[digit_count-1] = '1';
	
	for(;;){
		i64 bot_line = cur_line+rel_num;
		if(bot_line > one_past_last){ break; }
		
		for(;;){
			line_y = text_layout_line_on_screen(app, text_layout_id, cur_line+rel_num);
			if(line_y.min != line_y.max){ break; }
			rel_num++;
		}
		p = V2f32(margin.x0, line_y.min);
		draw_string(app, face, digit_string, p, fcolor_resolve(text_color));
		
		rel_num++;
		ptr = small_digit;
		while(ptr >= digit_buffer){
			if(*ptr == ' '){ *ptr   = '0'; }
			if(*ptr == '9'){ *ptr-- = '0'; }
			else{ (*ptr)++; break; }
		}
	}
	
	rel_num = 1;
	foreach(i, digit_count-1){ digit_buffer[i] = ' '; }
	digit_buffer[digit_count-1] = '1';
	
	for(;;){
		i64 top_line = cur_line-rel_num;
		if(top_line < first_line_num){ break; }
		
		for(;;){
			line_y = text_layout_line_on_screen(app, text_layout_id, cur_line-rel_num);
			if(line_y.min != line_y.max){ break; }
			rel_num++;
		}
		p = V2f32(margin.x0, line_y.min);
		draw_string(app, face, digit_string, p, fcolor_resolve(text_color));
		
		rel_num++;
		ptr = small_digit;
		while(ptr >= digit_buffer){
			if(*ptr == ' '){ *ptr   = '0'; }
			if(*ptr == '9'){ *ptr-- = '0'; }
			else{ (*ptr)++; break; }
		}
	}
	draw_set_clip(app, prev_clip);
}

function void
vim_draw_line_number_margin(Application_Links *app, View_ID view, Buffer_ID buffer, Face_ID face, Text_Layout_ID text_layout_id, Rect_f32 margin){
	
	Scratch_Block scratch(app);
	Rect_f32 prev_clip = draw_set_clip(app, margin);
	draw_rectangle_fcolor(app, margin, 0.f, fcolor_id(defcolor_line_numbers_back));
	
	Range_i64 visible_range = text_layout_get_visible_range(app, text_layout_id);
    i64 line_count = buffer_get_line_count(app, buffer);
    i64 digit_count = digit_count_from_integer(line_count, 10);
	
	u8 *digit_buffer = push_array(scratch, u8, digit_count);
	String_Const_u8 digit_string = SCu8(digit_buffer, digit_count);
	foreach(i, digit_count){ digit_buffer[i] = ' '; }
	
	i64 cur_line = view_compute_cursor(app, view, seek_pos(visible_range.min)).line;
	i64 end_line = view_compute_cursor(app, view, seek_pos(visible_range.max)).line+1;
	
	u8 *small_digit = digit_buffer + (digit_count-1) - 1;
	u8 *ptr = small_digit;
	if(cur_line == 0){ *ptr = '0'; }
	else{
		for(u64 X=cur_line; X>0; X/=10){
			*ptr-- = '0' + (X%10);
		}
	}
	
	Range_f32 line_y = text_layout_line_on_screen(app, text_layout_id, cur_line);
	Vec2_f32 p = V2f32(margin.x0, line_y.min);
	
	// NOTE(BYP): This assumes background is darker than font color
	FColor text_color = fcolor_id(defcolor_line_numbers_text);
	FColor contrast_color = fcolor_blend(text_color, 0.4f, f_white);
	
	draw_string(app, face, digit_string, p, fcolor_resolve(contrast_color));
	
	for(;;){
		if(cur_line > end_line){ break; }
		
		line_y = text_layout_line_on_screen(app, text_layout_id, cur_line);
		if(line_y.min != line_y.max){
			p = V2f32(margin.x0, line_y.min);
			draw_string(app, face, digit_string, p, fcolor_resolve(text_color));
		}
		
		cur_line++;
		ptr = small_digit;
		while(ptr >= digit_buffer){
			if(*ptr == ' '){ *ptr   = '0'; }
			if(*ptr == '9'){ *ptr-- = '0'; }
			else{ (*ptr)++; break; }
		}
	}
	
	draw_set_clip(app, prev_clip);
}

function void
vim_draw_whole_screen(Application_Links *app, Frame_Info frame_info){
	Rect_f32 region = global_get_screen_rectangle(app);
	Vec2_f32 center = rect_center(region);
	draw_set_clip(app, region);
	
	Face_ID face_id = get_face_id(app, 0);
	Face_Metrics metrics = get_face_metrics(app, face_id);
	f32 line_height = metrics.line_height;
	f32 char_wid = metrics.normal_advance;
	
	// NOTE(BYP): Drawing the cursor for view transitions
	if(vim_cur_cursor_pos != vim_nxt_cursor_pos){
		Vec2_f32 cursor_dim = V2f32(9.f, (vim_state.mode == VIM_Insert ? 4.f : 18.f));
		Rect_f32 cursor_rect = Rf32_xy_wh(vim_cur_cursor_pos - cursor_dim, cursor_dim);
		u64 cursor_roundness_100 = def_get_config_u64(app, vars_save_string_lit("cursor_roundness"));
		f32 roundness = char_wid*cursor_roundness_100*0.01f;
		draw_rectangle_fcolor(app, cursor_rect, roundness, fcolor_id(defcolor_cursor, default_cursor_sub_id()));
	}
	
	ARGB_Color back_color = fcolor_resolve(fcolor_id(defcolor_back));
	
	// NOTE(BYP): Drawing the back of the filebar lister
	Rect_f32 back_rect = vim_get_bottom_rect(app);
	if(vim_cur_filebar_offset > vim_nxt_filebar_offset){
		draw_rectangle(app, back_rect, 0.f, back_color);
		draw_rectangle_fcolor(app, rect_split_top_bottom_neg(back_rect, 4.f).b, 0.f, get_item_margin_color(UIHighlight_Active));
	}
	
	draw_rectangle(app, rect_split_top_bottom_neg(region, 2.f*line_height).b, 0.f, back_color);
	
	Vec2_f32 bot_left = {region.x0 + 4.f, region.y1 - 1.5f*line_height};
	String_Const_u8 bot_string = vim_get_bot_string();
	
	if(vim_use_bottom_cursor){
		Vec2_f32 p = draw_string(app, face_id, vim_bot_text.string, bot_left, finalize_color(defcolor_text_default, 0));
		if(ACTIVE_BLINK(vim_cursor_blink)){
			p.x -= 0.37f*(p.x - bot_left.x)/vim_bot_text.size;
			draw_string(app, face_id, string_u8_litexpr("|"), p, finalize_color(defcolor_text_default, 0));
		}
	}else{
		draw_string(app, face_id, bot_string, bot_left, finalize_color(defcolor_text_default, 0));
	}
	
	if(vim_lister_view_id == 0){
		/// NOTE(BYP): This is kinda a hacky pseudo-view
		if(vim_show_buffer_peek && rect_height(back_rect) > 0.f){
			Vim_Buffer_Peek_Entry *entry = vim_buffer_peek_list + vim_buffer_peek_index;
			Buffer_Identifier buffer_iden = entry->buffer_id;
			Buffer_ID buffer = buffer_identifier_to_id(app, buffer_iden);
			vim_set_bottom_text(SCu8((u8 *)buffer_iden.name, buffer_iden.name_len));
			
			f32 ratio_diff = entry->nxt_ratio - entry->cur_ratio;
			entry->cur_ratio += ratio_diff*frame_info.animation_dt*20.f;
			
			Buffer_Point buffer_point = {};
			i64 line_count = buffer_get_line_count(app, buffer);
			//buffer_point.line_number = i64(entry->cur_ratio*(line_count+1) - rect_height(back_rect)/line_height);
			buffer_point.pixel_shift.y = line_height*entry->cur_ratio*(line_count+1) - rect_height(back_rect);
			
			FColor peek_back_color = fcolor_id(defcolor_back);
			draw_rectangle_fcolor(app, back_rect, 0.f, peek_back_color);
			Rect_f32_Pair pair = rect_split_top_bottom_neg(back_rect, 4.f);
			draw_rectangle_fcolor(app, pair.b, 0.f, get_item_margin_color(UIHighlight_Active));
			back_rect = rect_split_left_right(pair.a, 4.f).b;
			Rect_f32 prev_clip = draw_set_clip(app, back_rect);
			
			Text_Layout_ID text_layout_id = text_layout_create(app, buffer, back_rect, buffer_point);
			Range_i64 visible_range = text_layout_get_visible_range(app, text_layout_id);
			
			paint_text_color_fcolor(app, text_layout_id, visible_range, fcolor_id(defcolor_text_default));
			paint_fade_ranges(app, text_layout_id, buffer);
			draw_text_layout_default(app, text_layout_id);
			
			text_layout_free(app, text_layout_id);
			draw_set_clip(app, prev_clip);
		}
	}
	
	Vec2_f32 bot_right = {region.x1 - 4.f - char_wid*vim_keystroke_text.size, bot_left.y};
	FColor chord_color = fcolor_id(defcolor_vim_chord_unresolved);
	if(vim_state.chord_resolved){
		chord_color = (vim_state.chord_resolved & bit_2 ?  fcolor_id(defcolor_vim_chord_error) : fcolor_id(defcolor_vim_chord_text));
	}
	draw_string(app, face_id, vim_keystroke_text.string, bot_right, chord_color);
}

