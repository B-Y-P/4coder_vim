
global Vim_State vim_state;
global Table_u64_u64 vim_maps[VIM_MODE_COUNT*VIM_SUBMODE_COUNT];
global Vim_Registers vim_registers;

global Range_i64 vim_macros[26 + 26];

global Vim_Apply_Request* vim_request_vtable[VIM_REQUEST_COUNT + VIM_ADDITIONAL_REQUESTS];
global Vim_Text_Object vim_text_object_vtable[VIM_TEXT_OBJECT_COUNT + VIM_ADDITIONAL_TEXT_OBJECTS];

global Vim_Global_Mark vim_global_marks[26];

global u8 vim_bot_buffer[256];
global String_u8 vim_bot_text = Su8(vim_bot_buffer, 0, ArrayCount(vim_bot_buffer));
global bool vim_is_querying_user_key;
global bool vim_is_selecting_register;

global u8 vim_keystroke_buffer[64];
global u64 vim_pre_keystroke_size;
global String_u8 vim_keystroke_text = Su8(vim_keystroke_buffer, 0, ArrayCount(vim_keystroke_buffer));

// TODO(BYP): Once visual insert is more polished move these somewhere better
global History_Group vim_history_group;
global b32 vim_visual_insert_after;
global u32 vim_visual_insert_flags;

global b32 vim_do_full_line = false;
global b32 vim_relative_numbers = true;
global b32 vim_show_block_helper = true;

#define ACTIVE_BLINK(b) (!(((b) / 20) & 0x1))
global b32 vim_use_bottom_cursor;
global u64 vim_cursor_blink;
global Vec2_f32 vim_nxt_cursor_pos;
global Vec2_f32 vim_cur_cursor_pos;

global View_ID vim_lister_view_id;

global f32 vim_nxt_filebar_offset;
global f32 vim_cur_filebar_offset;
function Rect_f32 vim_get_bottom_rect(Application_Links *app){
	Rect_f32 result = global_get_screen_rectangle(app);
	result.y1 -= 2.f*get_face_metrics(app, get_face_id(app, 0)).line_height;
	result.y0 = result.y1 - vim_cur_filebar_offset;
	return result;
}


struct Vim_Buffer_Peek_Entry{
	Buffer_Identifier buffer_id;
	f32 cur_ratio, nxt_ratio;
};

global b32 vim_show_buffer_peek;
global i32 vim_buffer_peek_index;


global Vim_Buffer_Peek_Entry vim_default_peek_list[] = {
	{ buffer_identifier(string_u8_litexpr("*compilation*")), 1.f, 1.f },
#if VIM_USE_REIGSTER_BUFFER
	{ buffer_identifier(string_u8_litexpr("*registers*")),   1.f, 1.f },
#endif
	{ buffer_identifier(string_u8_litexpr("*messages*")),    1.f, 1.f },
};

global Vim_Buffer_Peek_Entry vim_buffer_peek_list[ArrayCount(vim_default_peek_list) + VIM_ADDITIONAL_PEEK];


CUSTOM_ID(attachment, vim_buffer_prev_visual);
CUSTOM_ID(attachment, vim_buffer_marks);
CUSTOM_ID(attachment, vim_view_jumps);

function void vim_reset_bottom_text(){ vim_bot_text.size=0; }

function void vim_set_bottom_text(i32 size, char *str){
	block_copy(vim_bot_buffer, str, size);
	vim_bot_text.size = size;
}
function void vim_set_bottom_text(String_Const_u8 msg){ vim_set_bottom_text(string_expand((msg))); }

function i32 vim_consume_number(){
	i32 result = Max(1, vim_state.number)*Max(1, vim_state.params.count);
	vim_state.params.number = vim_state.number;
	vim_state.number = 0;
	return result;
}

function void vim_default_register(){
	vim_state.params.selected_reg = &vim_registers.VIM_DEFAULT_REGISTER;
}

function void vim_reset_state(){
	vim_state.mode = VIM_Normal;
	vim_state.sub_mode = SUB_None;
	vim_state.number = 0;
	vim_keystroke_text.size = 0;
	Vim_Seek_Params seek = vim_state.params.seek;
	vim_state.params = {};
	vim_state.params.seek = seek;
	vim_default_register();
}


function b32
vim_realloc_string(String_u8 *src, u64 size){
	String_Const_u8 new_data = base_allocate(&vim_state.alloc, VIM_GROW_RATE(size));
	if(new_data.size == 0){ return false; }
	block_copy(new_data.str, src->str, src->size);
	base_free(&vim_state.alloc, src->str);
	src->str = new_data.str;
	src->cap = new_data.size;
	return true;
}

function b32
vim_register_copy(Vim_Register *dst, String_Const_u8 src){
	b32 valid = true;
	if(src.size >= dst->data.cap){ valid = vim_realloc_string(&dst->data, src.size); }
	if(!valid){ return false; }
	block_copy(dst->data.str, src.str, src.size);
	dst->data.size = src.size;
	if(dst->data.size > 0){
		dst->flags |= (REGISTER_Set|REGISTER_Updated);
	}
	return true;
}

function b32
vim_register_copy(Vim_Register *dst, Vim_Register *src){
	b32 valid = vim_register_copy(dst, src->data.string);
	if(!valid){ return false; }
	dst->edit_type = src->edit_type;
	return true;
}

function u8
vim_get_register_char(Vim_Register *reg){
	u8 result = 0;
	Vim_Registers *r = &vim_registers;
	if(0){}
	else if(reg == &r->unnamed){      result = '"'; }
	else if(reg == &r->system){       result = '*'; }
	else if(reg == &r->search){       result = '/'; }
	else if(reg == &r->small_delete){ result = '-'; }
	else if(reg == &r->insert){       result = '.'; }
	else if(reg == &r->command){      result = ':'; }
	else if(reg == &r->file){         result = '%'; }
	else if(reg == &r->expression){   result = '='; }
	else if(in_range(r->named, reg, r->named + ArrayCount(r->named))){
		result = u8(i32('a') + i32(reg - r->named));
	}
	else if(in_range(r->digit, reg, r->digit + ArrayCount(r->digit))){
		result = u8(i32('0') + i32(reg - r->digit));
	}
	return result;
}
function void vim_update_registers(Application_Links *app);
