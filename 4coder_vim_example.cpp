
#include "4coder_default_include.cpp"

#include "4coder_vimrc.h"
#include "4coder_vim_include.h"

#if !defined(META_PASS)
#include "generated/managed_id_metadata.cpp"
#endif

function void
EXAMPLE_render_buffer(Application_Links *app, View_ID view_id, Face_ID face_id, Buffer_ID buffer, Text_Layout_ID text_layout_id, Rect_f32 rect){
	ProfileScope(app, "render buffer");
	View_ID active_view = get_active_view(app, Access_Always);
	b32 is_active_view = (active_view == view_id);
	Rect_f32 prev_clip = draw_set_clip(app, rect);

	Range_i64 visible_range = text_layout_get_visible_range(app, text_layout_id);

	Face_Metrics metrics = get_face_metrics(app, face_id);
	u64 cursor_roundness_100 = def_get_config_u64(app, vars_save_string_lit("cursor_roundness"));
	f32 cursor_roundness = metrics.normal_advance*cursor_roundness_100*0.01f;
	f32 mark_thickness = (f32)def_get_config_u64(app, vars_save_string_lit("mark_thickness"));

	Token_Array token_array = get_token_array_from_buffer(app, buffer);
	if(token_array.tokens == 0){
		paint_text_color_fcolor(app, text_layout_id, visible_range, fcolor_id(defcolor_text_default));
	}else{
		draw_cpp_token_colors(app, text_layout_id, &token_array);
		b32 use_comment_keyword = def_get_config_b32(vars_save_string_lit("use_comment_keyword"));
		if(use_comment_keyword){
			Comment_Highlight_Pair pairs[] = {
				{string_u8_litexpr("NOTE"), finalize_color(defcolor_comment_pop, 0)},
				{string_u8_litexpr("TODO"), finalize_color(defcolor_comment_pop, 1)},
			};
			draw_comment_highlights(app, buffer, text_layout_id, &token_array, pairs, ArrayCount(pairs));
		}
	}

	i64 cursor_pos = view_correct_cursor(app, view_id);
	view_correct_mark(app, view_id);

	b32 highlight_line_at_cursor = def_get_config_b32(vars_save_string_lit("highlight_line_at_cursor"));
	if(highlight_line_at_cursor && is_active_view){
		i64 line_number = get_line_number_from_pos(app, buffer, cursor_pos);
		draw_line_highlight(app, text_layout_id, line_number, fcolor_id(defcolor_highlight_cursor_line));
	}

	b32 use_scope_highlight = def_get_config_b32(vars_save_string_lit("use_scope_highlight"));
	if(use_scope_highlight){
		Color_Array colors = finalize_color_array(defcolor_back_cycle);
		draw_scope_highlight(app, buffer, text_layout_id, cursor_pos, colors.vals, colors.count);
	}

	b32 use_error_highlight = def_get_config_b32(vars_save_string_lit("use_error_highlight"));
	b32 use_jump_highlight  = def_get_config_b32(vars_save_string_lit("use_jump_highlight"));
	if(use_error_highlight || use_jump_highlight){
		Buffer_ID comp_buffer = get_buffer_by_name(app, string_u8_litexpr("*compilation*"), Access_Always);
		if(use_error_highlight){
			draw_jump_highlights(app, buffer, text_layout_id, comp_buffer, fcolor_id(defcolor_highlight_junk));
		}

		if(use_jump_highlight){
			Buffer_ID jump_buffer = get_locked_jump_buffer(app);
			if(jump_buffer != comp_buffer){
				draw_jump_highlights(app, buffer, text_layout_id, jump_buffer, fcolor_id(defcolor_highlight_white));
			}
		}
	}

	b32 use_paren_helper = def_get_config_b32(vars_save_string_lit("use_paren_helper"));
	if(use_paren_helper){
		Color_Array colors = finalize_color_array(defcolor_text_cycle);
		draw_paren_highlight(app, buffer, text_layout_id, cursor_pos, colors.vals, colors.count);
	}

	b64 show_whitespace = false;
	view_get_setting(app, view_id, ViewSetting_ShowWhitespace, &show_whitespace);
	if(show_whitespace){
		if(token_array.tokens == 0){
			draw_whitespace_highlight(app, buffer, text_layout_id, cursor_roundness);
		}else{
			draw_whitespace_highlight(app, text_layout_id, &token_array, cursor_roundness);
		}
	}

	if(is_active_view && vim_state.mode == VIM_Visual){
		vim_draw_visual_mode(app, view_id, buffer, face_id, text_layout_id);
	}

	vim_draw_search_highlight(app, view_id, buffer, text_layout_id, cursor_roundness);

	switch(fcoder_mode){
		case FCoderMode_Original:
		vim_draw_cursor(app, view_id, is_active_view, buffer, text_layout_id, cursor_roundness, mark_thickness); break;
		case FCoderMode_NotepadLike:
		draw_notepad_style_cursor_highlight(app, view_id, buffer, text_layout_id, cursor_roundness); break;
	}

	paint_fade_ranges(app, text_layout_id, buffer);
	draw_text_layout_default(app, text_layout_id);

	vim_draw_after_text(app, view_id, is_active_view, buffer, text_layout_id, cursor_roundness, mark_thickness);

	draw_set_clip(app, prev_clip);
}



function void
EXAMPLE_render_caller(Application_Links *app, Frame_Info frame_info, View_ID view_id){
	Buffer_ID buffer = view_get_buffer(app, view_id, Access_Always);
	Face_ID face_id = get_face_id(app, 0);
	Face_Metrics face_metrics = get_face_metrics(app, face_id);
	f32 line_height = face_metrics.line_height;
	f32 digit_advance = face_metrics.decimal_digit_advance;

	Rect_f32 region = view_get_screen_rect(app, view_id);
	Rect_f32 prev_clip = draw_set_clip(app, region);

	Rect_f32 global_rect = global_get_screen_rectangle(app);
	f32 filebar_y = global_rect.y1 - 2.f*line_height - vim_cur_filebar_offset;
	if(region.y1 >= filebar_y){ region.y1 = filebar_y; }

	draw_rectangle_fcolor(app, region, 0.f, fcolor_id(defcolor_back));

	region = vim_draw_query_bars(app, region, view_id, face_id);

	{
		Rect_f32_Pair pair = layout_file_bar_on_bot(region, line_height);
		pair.b = rect_split_top_bottom(pair.b, line_height).a;
		vim_draw_filebar(app, view_id, buffer, frame_info, face_id, pair.b);
		region = pair.a;
	}

	// Draw borders
	if(region.x0 > global_rect.x0){
		Rect_f32_Pair border_pair = rect_split_left_right(region, 2.f);
		draw_rectangle_fcolor(app, border_pair.a, 0.f, fcolor_id(defcolor_margin));
		region = border_pair.b;
	}
	if(region.x1 < global_rect.x1){
		Rect_f32_Pair border_pair = rect_split_left_right_neg(region, 2.f);
		draw_rectangle_fcolor(app, border_pair.b, 0.f, fcolor_id(defcolor_margin));
		region = border_pair.a;
	}
	region.y0 += 3.f;

	if(show_fps_hud){
		Rect_f32_Pair pair = layout_fps_hud_on_bottom(region, line_height);
		draw_fps_hud(app, frame_info, face_id, pair.max);
		region = pair.min;
		animate_in_n_milliseconds(app, 1000);
	}

	b32 show_line_number_margins = def_get_config_b32(vars_save_string_lit("show_line_number_margins"));
	Rect_f32 line_number_rect = {};
	if(show_line_number_margins){
		Rect_f32_Pair pair = (vim_relative_numbers ?
							  vim_line_number_margin(app, buffer, region, digit_advance) :
							  layout_line_number_margin(app, buffer, region, digit_advance));
		line_number_rect = pair.min;
		region = pair.max;
	}

	Buffer_Scroll scroll = view_get_buffer_scroll(app, view_id);
	Buffer_Point_Delta_Result delta = delta_apply(app, view_id, frame_info.animation_dt, scroll);
	if(!block_match_struct(&scroll.position, &delta.point)){
		block_copy_struct(&scroll.position, &delta.point);
		view_set_buffer_scroll(app, view_id, scroll, SetBufferScroll_NoCursorChange);
	}
	if(delta.still_animating){ animate_in_n_milliseconds(app, 0); }
	Buffer_Point buffer_point = scroll.position;
	Text_Layout_ID text_layout_id = text_layout_create(app, buffer, region, buffer_point);

	if(show_line_number_margins){
		if(vim_relative_numbers)
			vim_draw_rel_line_number_margin(app, view_id, buffer, face_id, text_layout_id, line_number_rect);
		else
			vim_draw_line_number_margin(app, view_id, buffer, face_id, text_layout_id, line_number_rect);
	}

	EXAMPLE_render_buffer(app, view_id, face_id, buffer, text_layout_id, region);

	text_layout_free(app, text_layout_id);
	draw_set_clip(app, prev_clip);
}

function Rect_f32
EXAMPLE_buffer_region(Application_Links *app, View_ID view_id, Rect_f32 region){

	Buffer_ID buffer = view_get_buffer(app, view_id, Access_Always);
	Face_ID face_id = get_face_id(app, buffer);
	Face_Metrics metrics = get_face_metrics(app, face_id);
	f32 line_height = metrics.line_height;
	f32 digit_advance = metrics.decimal_digit_advance;

	Rect_f32 global_rect = global_get_screen_rectangle(app);
	f32 filebar_y = global_rect.y1 - 2.f*line_height - vim_cur_filebar_offset;
	if(region.y1 >= filebar_y){
		region.y1 = filebar_y;
	}

	Query_Bar *space[32];
	Query_Bar_Ptr_Array query_bars = {};
	query_bars.ptrs = space;
	if(get_active_query_bars(app, view_id, ArrayCount(space), &query_bars)){
		region = layout_query_bar_on_bot(region, line_height, query_bars.count).min;
	}

	b64 showing_file_bar = false;
	if(view_get_setting(app, view_id, ViewSetting_ShowFileBar, &showing_file_bar) && showing_file_bar){
		region = layout_file_bar_on_bot(region, line_height).min;
	}

	if(region.x0 > global_rect.x0){
		region = rect_split_left_right(region, 2.f).b;
	}
	if(region.x1 < global_rect.x1){
		region = rect_split_left_right_neg(region, 2.f).a;
	}
	region.y0 += 3.f;

	if(show_fps_hud){ region = layout_fps_hud_on_bottom(region, line_height).min; }

	b32 show_line_number_margins = def_get_config_b32(vars_save_string_lit("show_line_number_margins"));
	if(show_line_number_margins){
		region = (vim_relative_numbers ?
				  vim_line_number_margin(app, buffer, region, digit_advance) :
				  layout_line_number_margin(app, buffer, region, digit_advance)).max;
	}

	return region;
}

VIM_TEXT_OBJECT_SIG(EXAMPLE_object_camel){
	Range_i64 range = {};
	Scratch_Block scratch(app);
	Range_i64 line_range = get_line_range_from_pos(app, buffer, cursor_pos);
	i64 s = line_range.min;
	u8 *line_text = push_buffer_range(app, scratch, buffer, line_range).str;
	u8 c = line_text[cursor_pos-s];
	if(!character_is_alpha_numeric(c)){ return {}; }
	cursor_pos += line_text[cursor_pos-s] == '_';
	range.min = range.max = cursor_pos;

	b32 valid=false;
	for(; range.min>0; range.min--){
		c = line_text[range.min-s];
		if(!character_is_alpha_numeric(c) || c == '_'){ valid = true; break; }
	}
	if(!valid){ return {}; }

	valid=false;
	for(; range.max>0; range.max++){
		c = line_text[range.max-s];
		if(!character_is_alpha_numeric(c) || c == '_'){ valid = true; break; }
	}
	if(!valid){ return {}; }

	range.min += (vim_state.params.clusivity != VIM_Inclusive || line_text[range.min-s] != '_');
	range.max -= (vim_state.params.clusivity != VIM_Inclusive || line_text[range.max-s] != '_');
	if(range.min >= range.max){ range = {}; }

	return range;
}

function void EXAMPLE_make_vim_request(Application_Links *app, EXAMPLE_Vim_Request request){
	vim_make_request(app, Vim_Request_Type(VIM_REQUEST_COUNT + request));
}

VIM_REQUEST_SIG(EXAMPLE_apply_title){
	Scratch_Block scratch(app);
	String_Const_u8 text = push_buffer_range(app, scratch, buffer, range);
	u8 prev = buffer_get_char(app, buffer, range.min-1);
	for(i32 i=0; i<text.size; i++){
		text.str[i] += u8(i32('A' - 'a')*((!character_is_alpha(prev) || prev == '_') &&
										  character_is_lower(text.str[i])));
		prev = text.str[i];
	}
	buffer_replace_range(app, buffer, range, text);
	buffer_post_fade(app, buffer, 0.667f, range, fcolor_resolve(fcolor_id(defcolor_paste)));
}

VIM_REQUEST_SIG(EXAMPLE_apply_rot13){
	Scratch_Block scratch(app);
	String_Const_u8 text = push_buffer_range(app, scratch, buffer, range);
	for(i32 i=0; i<text.size; i++){
		if(0){}
		else if(character_is_lower(text.str[i])){
			text.str[i] = (text.str[i] + 13) - 26*(text.str[i] > ('z'-13));
		}
		else if(character_is_upper(text.str[i])){
			text.str[i] = (text.str[i] + 13) - 26*(text.str[i] > ('Z'-13));
		}
	}
	buffer_replace_range(app, buffer, range, text);
	buffer_post_fade(app, buffer, 0.667f, range, fcolor_resolve(fcolor_id(defcolor_paste)));
}
VIM_COMMAND_SIG(EXAMPLE_request_title){ EXAMPLE_make_vim_request(app, EXAMPLE_REQUEST_Title); }
VIM_COMMAND_SIG(EXAMPLE_request_rot13){ EXAMPLE_make_vim_request(app, EXAMPLE_REQUEST_Rot13); }


void
custom_layer_init(Application_Links *app){
	default_framework_init(app);
	set_all_default_hooks(app);

	vim_request_vtable[VIM_REQUEST_COUNT + EXAMPLE_REQUEST_Title] = EXAMPLE_apply_title;
	vim_request_vtable[VIM_REQUEST_COUNT + EXAMPLE_REQUEST_Rot13] = EXAMPLE_apply_rot13;

	vim_text_object_vtable[VIM_TEXT_OBJECT_COUNT + EXAMPLE_OBJECT_camel] = {'_', (Vim_Text_Object_Func *)EXAMPLE_object_camel};
	vim_init(app);


	set_custom_hook(app, HookID_ViewEventHandler, vim_view_input_handler);

	set_custom_hook(app, HookID_BufferRegion, EXAMPLE_buffer_region);
	set_custom_hook(app, HookID_RenderCaller, EXAMPLE_render_caller);
	set_custom_hook(app, HookID_WholeScreenRenderCaller, vim_draw_whole_screen);

	set_custom_hook(app, HookID_Tick, vim_tick);
	set_custom_hook(app, HookID_SaveFile, vim_file_save_hook);
	set_custom_hook(app, HookID_BeginBuffer, vim_begin_buffer);
	set_custom_hook(app, HookID_ViewChangeBuffer, vim_view_change_buffer);

	Thread_Context *tctx = get_thread_context(app);
	mapping_init(tctx, &framework_mapping);
	String_ID global_map_id = vars_save_string_lit("keys_global");
	String_ID file_map_id = vars_save_string_lit("keys_file");
	String_ID code_map_id = vars_save_string_lit("keys_code");
	setup_essential_mapping(&framework_mapping, global_map_id, file_map_id, code_map_id);
#if OS_MAC
	setup_mac_mapping(&framework_mapping, global_map_id, file_map_id, code_map_id);
#else
	setup_default_mapping(&framework_mapping, global_map_id, file_map_id, code_map_id);
#endif

	vim_default_bindings(app, KeyCode_BackwardSlash);
	{
		u32 Sft = KeyMod_Sft;
#define I bit_1
#define N bit_2
#define MAP 0
		VimBind(I|N|MAP, EXAMPLE_request_title, SUB_Leader, (Sft|KeyCode_U));
		VimBind(I|N|MAP, EXAMPLE_request_rot13, SUB_G,      (Sft|KeyCode_ForwardSlash));
#undef I
#undef N
#undef MAP
	}
}
