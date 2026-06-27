enum Edge{ EDGE_X0, EDGE_Y0, EDGE_X1, EDGE_Y1 };
struct View_Val{ View_ID view; f32 val; };
function View_Val max_view(View_Val a, View_Val b){ return a.val >= b.val ? a : b; }

function View_Val panel_max_overlap(Application_Links *app, View_ID ignore, Rect_f32 r, Panel_ID p, View_Val max){
  if(panel_is_leaf(app, p)){
    View_ID view = panel_get_view(app, p, Access_Always);
    if(view == ignore || view_get_is_passive(app, view)){ return max; }
    return max_view(max, {view, rect_area(rect_intersect(r, view_get_screen_rect(app, view)))});
  }else{
    View_Val v0 = panel_max_overlap(app, ignore, r, panel_get_child(app, p, Side_Min), max);
    View_Val v1 = panel_max_overlap(app, ignore, r, panel_get_child(app, p, Side_Max), max);
    return max_view(v0, v1);
  }
}

function void vim_change_panel(Application_Links *app, Edge edge){
  View_ID view = get_active_view(app, Access_Always);
  Rect_f32 r = view_get_screen_rect(app, view);
  Rect_f32 e = (edge == EDGE_X0 ? Rf32(If32(r.x0), rect_range_y(r)) :
                edge == EDGE_X1 ? Rf32(If32(r.x1), rect_range_y(r)) :
                edge == EDGE_Y0 ? Rf32(rect_range_x(r), If32(r.y0)) :
                edge == EDGE_Y1 ? Rf32(rect_range_x(r), If32(r.y1)) : r);
  Panel_ID panel = panel_get_root(app);
  View_Val max = panel_max_overlap(app, view, rect_inner(e, -1.f), panel, {view, 1.f});
  view_set_active(app, max.view);
}

VIM_COMMAND_SIG(vim_change_x0){ vim_change_panel(app, EDGE_X0); }
VIM_COMMAND_SIG(vim_change_y0){ vim_change_panel(app, EDGE_Y0); }
VIM_COMMAND_SIG(vim_change_x1){ vim_change_panel(app, EDGE_X1); }
VIM_COMMAND_SIG(vim_change_y1){ vim_change_panel(app, EDGE_Y1); }