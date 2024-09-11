
VIM_COMMAND_SIG(vim_begin_line){
	Vim_Motion_Block vim_motion_block(app);
	right_adjust_view(app);
}

VIM_COMMAND_SIG(vim_line_start){
	Vim_Motion_Block vim_motion_block(app);
	seek_beginning_of_line(app);
}

VIM_COMMAND_SIG(vim_end_line){
	Vim_Motion_Block vim_motion_block(app);
	View_ID view = get_active_view(app, Access_ReadVisible);
	Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);
	i64 pos = view_get_cursor_pos(app, view);
	i64 new_pos = get_line_side_pos_from_pos(app, buffer, pos, Side_Max);
	if(vim_state.params.request == REQUEST_Change){
		new_pos -= (buffer_get_char(app, buffer, new_pos) == '\n');
		new_pos -= (buffer_get_char(app, buffer, new_pos) == '\r');
	}
	view_set_cursor_and_preferred_x(app, view, seek_pos(new_pos));
}

function void vim_scroll_inner(Application_Links *app, f32 ratio){
	View_ID view = get_active_view(app, Access_ReadVisible);
	Rect_f32 region = view_get_buffer_region(app, view);
	f32 view_height = rect_height(region);
	f32 line_height = get_view_line_height(app, view);

	i64 pos = view_get_cursor_pos(app, view);
	Buffer_Cursor cursor = view_compute_cursor(app, view, seek_pos(pos));
	Buffer_Scroll scroll = view_get_buffer_scroll(app, view);
	scroll.target.line_number = cursor.line;
	scroll.target.pixel_shift.y = ratio*(view_height - line_height);
	view_set_buffer_scroll(app, view, scroll, SetBufferScroll_SnapCursorIntoView);
}

VIM_COMMAND_SIG(vim_file_top){
	vim_push_jump(app, get_active_view(app, Access_ReadVisible));
	Vim_Motion_Block vim_motion_block(app);
	vim_state.params.edit_type = EDIT_LineWise;
	goto_beginning_of_file(app);
	move_vertical_lines(app, vim_consume_number()-1);
}


VIM_COMMAND_SIG(vim_goto_line){
	vim_push_jump(app, get_active_view(app, Access_ReadVisible));
	Vim_Motion_Block vim_motion_block(app);
	vim_state.params.edit_type = EDIT_LineWise;
	if(vim_state.number == 0){ goto_end_of_file(app); }
	else{
		View_ID view = get_active_view(app, Access_ReadVisible);
		view_set_cursor_and_preferred_x(app, view, seek_line_col(vim_consume_number(), 0));
	}
}

VIM_COMMAND_SIG(vim_goto_column){
	Vim_Motion_Block vim_motion_block(app);
	if(vim_state.number == 0){ right_adjust_view(app); }
	else{
		View_ID view = get_active_view(app, Access_ReadVisible);
		Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);
		i64 pos = view_get_cursor_pos(app, view);
		Buffer_Cursor cursor = buffer_compute_cursor(app, buffer, seek_pos(pos));
		view_set_cursor_and_preferred_x(app, view, seek_line_col(cursor.line, vim_consume_number()));
	}
}

VIM_COMMAND_SIG(vim_percent_file){
	vim_push_jump(app, get_active_view(app, Access_ReadVisible));
	Vim_Motion_Block vim_motion_block(app);
	vim_state.params.edit_type = EDIT_LineWise;
	View_ID view = get_active_view(app, Access_ReadVisible);
	Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);

	i64 percent = vim_consume_number();
	percent = Min(percent, 100);
	i64 target_line = i64(0.01f*percent*buffer_get_line_count(app, buffer));
	view_set_cursor_and_preferred_x(app, view, seek_line_col(target_line, 0));;
}

VIM_COMMAND_SIG(vim_if_read_only_goto_position)
{
    View_ID view = get_active_view(app, Access_ReadVisible);
    Buffer_ID buffer = view_get_buffer(app, view, Access_ReadWriteVisible);
    if (buffer == 0){
		buffer = view_get_buffer(app, view, Access_ReadVisible);
		if (buffer != 0){
			vim_push_jump(app, view);
			goto_jump_at_cursor(app);
			lock_jump_buffer(app, buffer);
		}
    }
    else{
		leave_current_input_unhandled(app);
    }
}

VIM_COMMAND_SIG(vim_if_read_only_goto_position_same_panel)
{
    View_ID view = get_active_view(app, Access_ReadVisible);
    Buffer_ID buffer = view_get_buffer(app, view, Access_ReadWriteVisible);
    if (buffer == 0){
		buffer = view_get_buffer(app, view, Access_ReadVisible);
		if (buffer != 0){
	    	vim_push_jump(app, view);
			goto_jump_at_cursor_same_panel(app);
			lock_jump_buffer(app, buffer);
		}
    }
    else{
		leave_current_input_unhandled(app);
    }
}

function void vim_screen_inner(Application_Links *app, f32 ratio, i32 offset){
	Vim_Motion_Block vim_motion_block(app);
	View_ID view = get_active_view(app, Access_ReadVisible);
	vim_push_jump(app, view);
	vim_state.params.edit_type = EDIT_LineWise;

	Rect_f32 region = view_get_buffer_region(app, view);
	i64 new_pos = view_pos_from_xy(app, view, V2f32(0.f, ratio*rect_height(region)));
	Buffer_Cursor cursor = view_compute_cursor(app, view, seek_pos(new_pos));
	cursor = view_compute_cursor(app, view, seek_line_col(cursor.line + offset, 0));
	view_set_cursor_and_preferred_x(app, view, seek_pos(cursor.pos));
}


global Character_Predicate character_predicate_non_word;
global Character_Predicate character_predicate_word;

function void init_vim_boundaries(){
	Character_Predicate character_predicate_non_alpha_num = character_predicate_not(&character_predicate_alpha_numeric_underscore_utf8);
	character_predicate_non_word = character_predicate_and(&character_predicate_non_alpha_num, &character_predicate_non_whitespace);
	character_predicate_word = character_predicate_not(&character_predicate_non_word);
}

function i64
vim_boundary_word(Application_Links *app, Buffer_ID buffer, Side side, Scan_Direction direction, i64 pos){
	return(boundary_predicate(app, buffer, side, direction, pos, &character_predicate_alpha_numeric_underscore_utf8));
}

function i64
vim_boundary_non_word(Application_Links *app, Buffer_ID buffer, Side side, Scan_Direction direction, i64 pos){
	return(boundary_predicate(app, buffer, side, direction, pos, &character_predicate_non_word));
}

function i64
boundary_whitespace(Application_Links *app, Buffer_ID buffer, Side side, Scan_Direction direction, i64 pos){
	return(boundary_predicate(app, buffer, side, direction, pos, &character_predicate_whitespace));
}


// NOTE(BYP): To this day, I still don't know _exactly_ what's going on here, but it works so *shrug*
function i64 vim_word_boundary(Application_Links *app, Buffer_ID buffer, Scan_Direction direction, i64 pos){
	Scratch_Block scratch(app);
	u8 c = buffer_get_char(app, buffer, pos);
	if(direction == Scan_Forward){
		if(character_is_whitespace(c)){
			pos = buffer_seek_character_class_change_1_0(app, buffer, &character_predicate_whitespace, direction, pos);
		}else{
			pos = scan(app, push_boundary_list(scratch, vim_boundary_word, vim_boundary_non_word, boundary_whitespace), buffer, direction, pos);
			if(character_is_whitespace(buffer_get_char(app, buffer, pos))){
				pos = buffer_seek_character_class_change_1_0(app, buffer, &character_predicate_whitespace, direction, pos);
			}
		}
	}else{
		if(character_is_whitespace(c)){
			pos = scan(app, push_boundary_list(scratch, boundary_whitespace),   buffer, direction, pos+1);
		}
		i64 p1  = scan(app, push_boundary_list(scratch, vim_boundary_word),     buffer, direction, pos);
		i64 p2  = scan(app, push_boundary_list(scratch, vim_boundary_non_word), buffer, direction, pos);
		pos = Max(p1, p2);
		if(character_is_whitespace(buffer_get_char(app, buffer, pos))){
			pos = buffer_seek_character_class_change_1_0(app, buffer, &character_predicate_whitespace, direction, pos);
		}
	}
	return pos;
}

function i64 vim_WORD_boundary(Application_Links *app, Buffer_ID buffer, Scan_Direction direction, i64 pos){
	if(direction == Scan_Forward){
		pos = buffer_seek_character_class_change_1_0(app, buffer, &character_predicate_non_whitespace, direction, pos);
		pos = buffer_seek_character_class_change_0_1(app, buffer, &character_predicate_non_whitespace, direction, pos);
	}else{
		pos = buffer_seek_character_class_change_0_1(app, buffer, &character_predicate_non_whitespace,  direction, pos-1);
		pos = buffer_seek_character_class_change_1_0(app, buffer, &character_predicate_non_whitespace,  direction, pos);
		pos = buffer_seek_character_class_change_0_1(app, buffer, &character_predicate_non_whitespace, -direction, pos);
	}
	return pos;
}

// TODO(BYP): Once backwards is finished, adjust word/WORD text objects
function i64 vim_end_boundary(Application_Links *app, Buffer_ID buffer, Scan_Direction direction, i64 pos){
	i64 prev_pos = pos;
	Scratch_Block scratch(app);
	u8 c = buffer_get_char(app, buffer, pos);
	u8 c1 = buffer_get_char(app, buffer, pos+1);
	u8 c2 = buffer_get_char(app, buffer, pos+2);

	if(direction == Scan_Forward){
		if(!character_is_whitespace(c1)){
			// This is a weird edge case (single character predicate type changes), so it's somewhat hardcoded
			b32 c1_w = character_predicate_check_character(character_predicate_word, c1);
			b32 c2_w = character_predicate_check_character(character_predicate_word, c2);
			b32 c1_n = character_predicate_check_character(character_predicate_non_word, c1);
			b32 c2_n = character_predicate_check_character(character_predicate_non_word, c2);
			if((c1_n && c2_w) || (c1_w && c2_n)){ return pos+1; }
		}

		if(character_is_whitespace(c)){
			pos = buffer_seek_character_class_change_1_0(app, buffer, &character_predicate_whitespace, direction, pos);
		}
		pos = scan(app, push_boundary_list(scratch, vim_boundary_word, vim_boundary_non_word, boundary_whitespace), buffer, direction, pos)-1;
		if(pos == prev_pos){
			pos = vim_end_boundary(app, buffer, direction, pos+1);
		}
	}else{
		if(character_is_whitespace(c)){
			pos = scan(app, push_boundary_list(scratch, boundary_whitespace), buffer, direction, pos+1);
		}
		//i64 p1  = scan(app, push_boundary_list(scratch, vim_boundary_word),     buffer, direction, pos);
		//i64 p2  = scan(app, push_boundary_list(scratch, vim_boundary_non_word), buffer, direction, pos);
		pos = scan(app, push_boundary_list(scratch, vim_boundary_word, vim_boundary_non_word, boundary_whitespace), buffer, direction, pos)-1;
		//scan_any_boundary(app, Boundary_Function *func, buffer, direction, pos);
		//pos = Max(pos, Max(p1, p2));

		if(character_is_whitespace(buffer_get_char(app, buffer, pos))){
			pos--;
			//pos = buffer_seek_character_class_change_1_0(app, buffer, &character_predicate_whitespace, direction, pos+1);
			//pos = scan(app, push_boundary_list(scratch, boundary_whitespace), buffer, direction, pos);
		}
	}
	return pos;

}

function i64 vim_END_boundary(Application_Links *app, Buffer_ID buffer, Scan_Direction direction, i64 pos){
	i64 prev_pos = pos;
	Scratch_Block scratch(app);

	if(true || direction == Scan_Forward){
		pos = vim_WORD_boundary(app, buffer, direction, pos)-direction;
		if(character_is_whitespace(buffer_get_char(app, buffer, pos))){
			pos = scan(app, push_boundary_list(scratch, boundary_whitespace), buffer, -direction, pos+direction) - direction;
		}
		if(pos == prev_pos){ pos = vim_END_boundary(app, buffer, direction, pos+direction); }
	}else{
		pos = buffer_seek_character_class_change_1_0(app, buffer, &character_predicate_non_whitespace, direction, pos);
		pos = buffer_seek_character_class_change_0_1(app, buffer, &character_predicate_non_whitespace, direction, pos);
	}
	return pos;
}

function i64 vim_scan_word(Application_Links *app, View_ID view, Scan_Direction direction, i64 *prev_pos=0, const i32 N=1){
	Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);
	i64 cursor_pos = view_get_cursor_pos(app, view);
	foreach(i, N){
		if(prev_pos){ *prev_pos = cursor_pos; }
		cursor_pos = vim_word_boundary(app, buffer, direction, cursor_pos);
	}
	return cursor_pos;
}

function i64 vim_scan_WORD(Application_Links *app, View_ID view, Scan_Direction direction, i64 *prev_pos=0, const i32 N=1){
	Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);
	i64 cursor_pos = view_get_cursor_pos(app, view);
	foreach(i,N){
		if(prev_pos){ *prev_pos = cursor_pos; }
		cursor_pos = vim_WORD_boundary(app, buffer, direction, cursor_pos);
	}
	return cursor_pos;
}

function i64 vim_scan_end(Application_Links *app, View_ID view, Scan_Direction direction, const i32 N=1){
	Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);
	i64 cursor_pos = view_get_cursor_pos(app, view);
	foreach(i,N){
		cursor_pos = vim_end_boundary(app, buffer, direction, cursor_pos);
	}
	return cursor_pos;
}

function i64 vim_scan_END(Application_Links *app, View_ID view, Scan_Direction direction, const i32 N=1){
	Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);
	i64 cursor_pos = view_get_cursor_pos(app, view);
	foreach(i,N){
		cursor_pos = vim_END_boundary(app, buffer, direction, cursor_pos);
	}
	return cursor_pos;
}

function b32 vim_seek_char_inner(Application_Links *app, Scan_Direction Scan){
	Vim_Seek_Params seek = vim_state.params.seek;
	if(seek.character == 0){ return false; }
	i32 direction = Scan*seek.direction;
	View_ID view = get_active_view(app, Access_ReadVisible);
	Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);
	i64 start_pos = view_get_cursor_pos(app, view);
	i64 pos = start_pos;
	pos += direction;
	Range_i64 range = get_line_range_from_pos(app, buffer, pos);
	while(seek.character != buffer_get_char(app, buffer, pos)){
		if(!range_contains(range, pos)){ return false; }
		pos += direction;
	}
	view_set_cursor_and_preferred_x(app, view, seek_pos(pos));
	return true;
}

function void vim_seek_char(Application_Links *app){
	Vim_Motion_Block vim_motion_block(app);
	b32 valid = true;
	const i32 N = vim_consume_number();
	foreach(i,N){ valid &= vim_seek_char_inner(app, Scan_Forward); }
	vim_state.params.request *= valid;
	if(valid && vim_state.params.seek.clusivity == VIM_Exclusive){
		move_horizontal_lines(app, -vim_state.params.seek.direction);
	}
}

VIM_COMMAND_SIG(vim_set_seek_char){
	User_Input input = get_current_input(app);
	if(input.event.kind == InputEventKind_KeyStroke){
		vim_state.params.seek.clusivity = (input.event.key.code == KeyCode_T ? VIM_Exclusive : VIM_Inclusive);
		vim_state.params.seek.direction = (has_modifier(&input.event, KeyCode_Shift) ? -1 : 1);

		u8 key = vim_query_user_key(app, string_u8_litexpr("-- SEEK NEXT --"));
		if(key){
			vim_state.params.seek.character = key;
			vim_state.active_command = vim_seek_char;
			vim_seek_char(app);
		}
	}
}

VIM_COMMAND_SIG(vim_seek_char_forward){
	vim_push_jump(app, get_active_view(app, Access_ReadVisible));
	Vim_Motion_Block vim_motion_block(app);
	const i32 N = vim_consume_number();
	foreach(i,N){ vim_seek_char_inner(app, Scan_Forward); }
	if(vim_state.params.seek.clusivity == VIM_Exclusive){
		move_horizontal_lines(app, -vim_state.params.seek.direction);
	}
}

VIM_COMMAND_SIG(vim_seek_char_backward){
	vim_push_jump(app, get_active_view(app, Access_ReadVisible));
	Vim_Motion_Block vim_motion_block(app);
	const i32 N = vim_consume_number();
	foreach(i,N){ vim_seek_char_inner(app, Scan_Backward); }
	if(vim_state.params.seek.clusivity == VIM_Exclusive){
		move_horizontal_lines(app, vim_state.params.seek.direction);
	}
}

bool vim_character_can_bounce(u8 c){
	return c == '[' || c == ']' || c == '{' || c == '}' || c == '(' || c == ')';
}

Scan_Direction vim_bounce_direction(u8 c){
	switch(c){
		case '[': case '(': case '{': return Scan_Forward;
		case ']': case ')': case '}': return Scan_Backward;
	}
	return 0;
}

u8 vim_corresponding_bounce(u8 c){
	switch(c){
		case '[': return ']';
		case '(': return ')';
		case '{': return '}';
		case ']': return '[';
		case ')': return '(';
		case '}': return '{';
	}
	return 0;
}

function i64 vim_bounce_pair(Application_Links *app, Buffer_ID buffer, i64 pos, u8 first){
	i32 direction = vim_bounce_direction(first);
	u8 track, close = vim_corresponding_bounce(first);
	i64 max_pos = buffer_get_size(app, buffer);
	i32 stack_count = 0;

	i64 prev_pos = pos;
	pos += direction;
	while(close != (track = buffer_get_char(app, buffer, pos)) || stack_count != 0){
		if(track == first){ stack_count++; }
		if(track == close){ stack_count--; }
		if(pos <= 0 || max_pos <= pos){ return prev_pos; }
		pos += direction;
	}
	return pos;
}

function i64 vim_scan_bounce(Application_Links *app, Buffer_ID buffer, i64 cursor_pos, Scan_Direction direction){
	i64 max_pos = buffer_get_size(app, buffer);
	u8 c = buffer_get_char(app, buffer, cursor_pos);
	i64 pos = cursor_pos - (c == '\n' || c == '\r');
	u8 track;
	while(!vim_character_can_bounce(track = buffer_get_char(app, buffer, pos))){
		pos += direction;
		if(pos <= 0 || max_pos <= pos){ return cursor_pos; }
	}

	return vim_bounce_pair(app, buffer, pos, track);
}

VIM_TEXT_OBJECT_SIG(vim_object_none){ return Ii64(0,0); }

VIM_TEXT_OBJECT_SIG(vim_object_para){
	Range_i64 range = {};
	if(line_is_blank(app, buffer, get_line_number_from_pos(app, buffer, cursor_pos))){
		range.min = range.max = cursor_pos;
	}else{
		range.min = get_pos_of_blank_line_grouped(app, buffer, Scan_Backward, cursor_pos);
		range.max = get_pos_of_blank_line_grouped(app, buffer, Scan_Forward,  cursor_pos);
	}
	vim_state.params.edit_type = EDIT_LineWise;
	return range;
}

VIM_TEXT_OBJECT_SIG(vim_object_word){
	Range_i64 range = {};
	char c = buffer_get_char(app, buffer, cursor_pos);
	if(character_is_whitespace(c)){
		range.min = scan(app, boundary_whitespace, buffer, Scan_Backward, cursor_pos+1);
		range.max = buffer_seek_character_class_change_1_0(app, buffer, &character_predicate_whitespace, Scan_Forward, cursor_pos)-1;
	}else{
		if(true || vim_state.params.clusivity == VIM_Exclusive){
			u8 c1 = buffer_get_char(app, buffer, cursor_pos-1);
			u8 c2 = buffer_get_char(app, buffer, cursor_pos+1);
			b32 boundary1 = !character_predicate_check_character(character_predicate_word, c1) || character_is_whitespace(c1);
			b32 boundary2 = !character_predicate_check_character(character_predicate_word, c2) || character_is_whitespace(c2);
			range.min = boundary1 ? cursor_pos : vim_word_boundary(app, buffer, Scan_Backward, cursor_pos);
			range.max = boundary2 ? cursor_pos : vim_end_boundary(app, buffer, Scan_Forward, cursor_pos);
		}else{
			range.min = vim_end_boundary(app, buffer, Scan_Backward, cursor_pos)+1;
			range.max = vim_word_boundary(app, buffer, Scan_Forward, cursor_pos);
		}
	}
	return range;
}


VIM_TEXT_OBJECT_SIG(vim_object_WORD){
	Range_i64 range = {};
	char c = buffer_get_char(app, buffer, cursor_pos);
	if(character_is_whitespace(c)){
		range.min = scan(app, boundary_whitespace, buffer, Scan_Backward, cursor_pos+1);
		range.max = buffer_seek_character_class_change_1_0(app, buffer, &character_predicate_whitespace, Scan_Forward, cursor_pos)-1;
	}else{
		if(true || vim_state.params.clusivity == VIM_Exclusive){
			b32 boundary1 = character_is_whitespace(buffer_get_char(app, buffer, cursor_pos-1));
			b32 boundary2 = character_is_whitespace(buffer_get_char(app, buffer, cursor_pos+1));
			range.min = boundary1 ? cursor_pos : vim_WORD_boundary(app, buffer, Scan_Backward, cursor_pos);
			range.max = boundary2 ? cursor_pos : vim_END_boundary(app, buffer, Scan_Forward, cursor_pos);
		}else{
			range.min = vim_END_boundary(app, buffer, Scan_Backward, cursor_pos)+1;
			range.max = vim_WORD_boundary(app, buffer, Scan_Forward, cursor_pos);
		}
	}
	return range;
}

// NOTE(BYP): Default vim behavior for Around includes leading whitespace before quote objects
// I have chosen to ignore this, since it's both easier, and aligns better with expectation
VIM_TEXT_OBJECT_SIG(vim_scan_object_quotes){
	Range_i64 range = {};
	u8 character = vim_state.params.seek.character;
	Range_i64 line_range = get_line_range_from_pos(app, buffer, cursor_pos);
	if(range_size(line_range) >= 2){
		Scratch_Block scratch(app);
		u8 *line_text = push_buffer_range(app, scratch, buffer, line_range).str;
		i64 s = line_range.min;
		b32 inside_string = 0;
		for(i64 i=line_range.min; i<cursor_pos; i++){
			if(character == line_text[i-s]){
				range.min = i;
				inside_string ^= 1;
			}
		}

		if(inside_string){
			for(range.max = cursor_pos; range.max < line_range.max; range.max++){
				if(character == line_text[range.max-s]){ break; }
			}
			if(character != line_text[range.max-s]){ range = {}; }
		}else{
			range.min = 0;
			for(i64 i=cursor_pos; i<line_range.max; i++){
				if(character == line_text[i-s]){
					range.min = i;
					break;
				}
			}
			if(range.min){
				range.max = line_range.max;
				for(i64 i=range.min+1; i<line_range.max; i++){
					if(character == line_text[i-s]){
						range.max = i;
						break;
					}
				}
				if(character != line_text[range.max-s]){ range = {}; }
			}
		}

		if(vim_state.params.clusivity == VIM_Exclusive){
			range.min++;
			range.max--;
			if(range.min >= range.max){ range = {}; }
		}
	}
	return range;
}

// NOTE(BYP): Default vim behavior for Visual Block on Text Objects is to do nothing.
// I have chosen to ignore this, since it takes exactly no effort on my part to make it work.
function void vim_text_object(Application_Links *app){
	View_ID view = get_active_view(app, Access_ReadVisible);
	Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);
	i64 cursor_pos = view_get_cursor_pos(app, view);
	u8 character = vim_state.params.seek.character;
	Vim_Clusivity object_clusivity = vim_state.params.clusivity;

	Range_i64 range = {};

	b32 did_range = false;
	foreach(i, ArrayCount(vim_text_object_vtable)){
		Vim_Text_Object *text_object = vim_text_object_vtable + i;
		if(character == text_object->character){
			range = text_object->func(app, buffer, cursor_pos);
			did_range = true;
			break;
		}
	}

	if(!did_range){

		/// Scope objects
		if(vim_character_can_bounce(character)){
			u8 c = buffer_get_char(app, buffer, cursor_pos);
			u8 b_character = vim_corresponding_bounce(character);
			if(c == character || c == b_character){
				range = Ii64(vim_bounce_pair(app, buffer, cursor_pos, c), cursor_pos);
			}else{
				range = Ii64(vim_bounce_pair(app, buffer, cursor_pos, character),
							 vim_bounce_pair(app, buffer, cursor_pos, b_character));
			}
			if(vim_state.params.clusivity == VIM_Exclusive){
				range.min++;
				range.max--;
				if(range.min >= range.max){ range = {}; }
				vim_state.params.clusivity = VIM_Inclusive;
			}
		}

		/// Quote objects
		else if(character == '"' || character == '\''){
			range = vim_scan_object_quotes(app, buffer, cursor_pos);
		}
	}

	if(range.min && range.max){
		vim_push_jump(app, view);
		{
			Vim_Motion_Block vim_motion_block(app, range.max);
			vim_state.params.clusivity = VIM_Inclusive;
			view_set_cursor_and_preferred_x(app, view, seek_pos(range.min));
			view_set_mark(app, view, seek_pos(range.max));
		}
		vim_state.prev_params.clusivity = object_clusivity;
	}else{
		vim_reset_state();
	}
}
