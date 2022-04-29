
//// Author(BYP)
//
// You can add or even 'override' entries in vim_request_vtable and vim_text_object_vtable if you'd like
// just make sure it's declared in the #define before including this file
//
// Haven't written correct movement for end\END boundaries
//
// Clicking filebar lister items is a bit hacky. 4coder doesn't expect a lister overtop multiple views
// Visual_Block/Visual_Insert can do the basics, but wasn't written for robustness
//
///////////////////////////////////

CUSTOM_ID(colors, defcolor_vim_filebar_pop);
CUSTOM_ID(colors, defcolor_vim_chord_text);
CUSTOM_ID(colors, defcolor_vim_chord_unresolved);
CUSTOM_ID(colors, defcolor_vim_chord_error);

// [0,1]
#ifndef VIM_DO_ANIMATE
#define VIM_DO_ANIMATE 1
#endif

// {unnamed, system}
#ifndef VIM_DEFAULT_REGISTER
#define VIM_DEFAULT_REGISTER unnamed
#endif

// [0,1]
#ifndef VIM_USE_BOTTOM_LISTER
#define VIM_USE_BOTTOM_LISTER 0
#endif

// [0,1]
#ifndef VIM_USE_REIGSTER_BUFFER
#define VIM_USE_REIGSTER_BUFFER 1
#endif

// [0,1]
#ifndef VIM_USE_TRADITIONAL_CHORDS
#define VIM_USE_TRADITIONAL_CHORDS 1
#endif

// n,m : 0 < n <= m
#ifndef VIM_LISTER_RANGE
#define VIM_LISTER_RANGE 3,5
#endif

// (0.f, 1.f]
#ifndef VIM_LISTER_MAX_RATIO
#define VIM_LISTER_MAX_RATIO 0.55f
#endif

// [0,)
#ifndef VIM_ADDITIONAL_REQUESTS
#define VIM_ADDITIONAL_REQUESTS 0
#endif

// [0,)
#ifndef VIM_ADDITIONAL_TEXT_OBJECTS
#define VIM_ADDITIONAL_TEXT_OBJECTS 0
#endif

// [0,)
#ifndef VIM_ADDITIONAL_PEEK
#define VIM_ADDITIONAL_PEEK 0
#endif


/// If you _really_ want to change dynamic register allocation, go for it
#ifndef VIM_GROW_RATE
#define VIM_GROW_RATE(S) (Max(u64(128), u64(1.5*(S))))
#endif

#include "4coder_vim_keycode_lut.h"
#include "4coder_vim_base_types.h"
#include "4coder_vim.h"

#include "4coder_vim_block.cpp"
#include "4coder_vim_helper.cpp"
#include "4coder_vim_registers.cpp"
#include "4coder_vim_movement.cpp"

#include "4coder_vim_lister.cpp"
#include "4coder_vim_lists.cpp"
#include "4coder_vim_search.cpp"

#include "4coder_folds.hpp"
#include "4coder_vim_commands.cpp"
#include "4coder_vim_bindings.cpp"
#include "4coder_vim.cpp"

#include "4coder_vim_draw.cpp"
#include "4coder_vim_hooks.cpp"
