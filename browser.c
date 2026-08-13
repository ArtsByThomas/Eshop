#include "browser.h"
#include "/utils/syscall/syscalls.h"
#include "/utils/string/str_utils.h"
#include "/DOM/DOM.h"
#include "/css/css.h"
#include "/navigation/navigation.h"
#define HOME_URL "http://192.168.2.1:8000/"
extern DOMNode* alloc_node(void);
extern DOMNode* dom_find_at_point(DOMNode* node, int doc_x, int doc_y);
extern void navigate_to(const char* url);
extern void layout_dom_node(DOMNode* node, int start_x, int start_y, int avail_w, int* out_w, int* out_h);
extern void paint_dom_node(DOMNode* node);
extern int dom_collect_ancestor_chain(DOMNode* root, DOMNode* target, DOMNode** out_chain, int max_len);
extern int js_fire_click_bubbling(DOMNode* page_root, DOMNode* node);
extern int js_timers_tick(void);
extern int address_bar_focused;
extern int address_bar_len;
extern char address_bar_text[128];
extern int scroll_y;
extern int max_scroll_y;
extern int nav_history_pos;
extern int nav_history_count;
extern void nav_go_back(void);
extern void nav_go_forward(void);
extern void nav_reload(void);
extern void nav_go_home(void);
extern void submit_form(DOMNode* form);
extern DOMNode* get_clicked_link(DOMNode* node, int doc_x, int doc_y);
extern DOMNode* find_enclosing_form(DOMNode* node, DOMNode* submit_btn);
extern void resolve_relative_url(const char* href, char* out, int out_size);
extern void compute_style_tree(DOMNode* node, ComputedStyle* parent_style);
extern  void render_dom_node(DOMNode* node, int start_x, int start_y, int* out_w, int* out_h);
int update_window_size(void) {
    int w = browser_width, h = browser_height;
    sys_get_window_size(&w, &h);  

    if (w < 100) w = 100;          
    if (h < 60)  h = 60;
    if (w > BROWSER_MAX_WIDTH)  w = BROWSER_MAX_WIDTH;
    if (h > BROWSER_MAX_HEIGHT) h = BROWSER_MAX_HEIGHT;

    if (w != browser_width || h != browser_height) {
        browser_width  = w;
        browser_height = h;
        return 1;
    }
    return 0;
}


#define TAB_BAR_H      24  
#define NAV_BTN_Y      (TAB_BAR_H + 5) 
#define NAV_BTN_H      20
#define NAV_BTN_W      22
#define NAV_BTN_GAP    4
#define NAV_BTN_BACK_X    10
#define NAV_BTN_FWD_X     (NAV_BTN_BACK_X + NAV_BTN_W + NAV_BTN_GAP)
#define NAV_BTN_RELOAD_X  (NAV_BTN_FWD_X  + NAV_BTN_W + NAV_BTN_GAP)
#define NAV_BTN_HOME_X    (NAV_BTN_RELOAD_X + NAV_BTN_W + NAV_BTN_GAP)
#define ADDR_BAR_X        (NAV_BTN_HOME_X + NAV_BTN_W + NAV_BTN_GAP)

// --- GLOBÁLNÍ DATA PRO TABY ---
#define MAX_TABS 5
typedef struct {
    int active;
    char url[256];
    int scroll_y;
    char title[32];
} BrowserTab;

BrowserTab browser_tabs[MAX_TABS];
int current_tab_index = 0;
int main(int argc, char **argv) {
    
    update_window_size();

    root_node = alloc_node();
    root_node->type = NODE_DIV;
    str_copy(root_node->tag_name, "div", sizeof(root_node->tag_name));
    root_node->style.has_bg = 1;      root_node->style.bg_color = 0xFFFFFFFF;
    root_node->style.has_width = 1;   root_node->style.width = browser_width;
    root_node->style.has_height = 1;  root_node->style.height = browser_height;
    root_node->style.has_margin = 1;  root_node->style.margin = 0;
    root_node->style.has_padding = 1; root_node->style.padding = 10;
    root_node->style.has_display = 1; root_node->style.is_inline = 0;
    root_node->style.has_color = 1;   root_node->style.text_color = 0xFF000000;
    root_node->style.has_font_size = 1; root_node->style.font_scale = 1;
    root_node->style.has_text_align = 1; root_node->style.text_align = 0;
    root_node->style.has_border = 1;  root_node->style.border_w = 0;

    DOMNode* loading = alloc_node();
    loading->type = NODE_TEXT;
    loading->text_len = 22;
    char *msg = "Pripojuji se k siti...";
    for(int j=0; j<22; j++) loading->text[j] = msg[j];
    loading->style.has_color = 1;     loading->style.text_color = 0xFF000000;
    loading->style.has_font_size = 1; loading->style.font_scale = 1;
    loading->style.has_text_align = 1; loading->style.text_align = 0;
    loading->style.has_display = 1;   loading->style.is_inline = 0;
    root_node->children[root_node->child_count++] = loading;

    fill_buffer_fast(pixel_buffer, 0xFFFFFFFF, browser_width * browser_height);
    int doc_w, doc_h;
    render_dom_node(root_node, 0, 0, &doc_w, &doc_h);
    sys_fb_blit(pixel_buffer, browser_width, browser_height);
    for(int i = 0; i < MAX_TABS; i++) browser_tabs[i].active = 0;
    browser_tabs[0].active = 1;
    browser_tabs[0].scroll_y = 0;
    str_copy(browser_tabs[0].title, "Hlavni stranka", 32);
    str_copy(browser_tabs[0].url, "https://example.com/", 256);
    current_tab_index = 0;

    navigate_to(browser_tabs[0].url);

    int last_scroll = -1;
    fill_buffer_fast(pixel_buffer, 0xFFFFFFFF, browser_width * browser_height);
    int input_detected = 1;
    int mx = 0, my = 0, mbtn = 0;
    int is_dragging_scrollbar = 0; 

    while (1) {
         
            if (update_window_size()) {
                last_scroll = -1; 
                layout_dirty = 1; 
            }

            if (js_timers_tick()) {
                last_scroll = -1; 
            }

if (last_scroll == -1) {
    uint32_t clear_color = (root_node && root_node->style.has_bg) ? root_node->style.bg_color : 0xFFFFFFFF;
    if ((clear_color >> 24) == 0) clear_color = 0xFFFFFFFF; 
    fill_buffer_fast(pixel_buffer, clear_color, browser_width * browser_height);

    if (layout_dirty) {
        layout_dom_node(root_node, 0, 0, browser_width, &doc_w, &doc_h);
        layout_dirty = 0;
    }
    paint_dom_node(root_node);

}
if (input_detected == 1) {
    max_scroll_y = doc_h - browser_height;
    if (max_scroll_y < 0) max_scroll_y = 0;
    draw_scrollbar(doc_h);

    int ui_h = TAB_BAR_H + 30; 
    for (int y = 0; y < ui_h; y++) {
        fill_row_fast(y, 0, browser_width, 0xFFCCCCCC);
    }

    int tab_w = 140;
    for (int i = 0; i < MAX_TABS; i++) {
        if (!browser_tabs[i].active) continue;
        
        int tx = i * tab_w;
        uint32_t tab_bg = (i == current_tab_index) ? 0xFFEEEEEE : 0xFFAAAAAA;
        draw_rect_ui(tx, 0, tab_w - 2, TAB_BAR_H, tab_bg);
        
        if (i == current_tab_index) {
            draw_rect_ui(tx, 0, tab_w - 2, 2, 0xFF0000FF);
        }
        draw_text_ui(tx + 5, 8, browser_tabs[i].title, str_len(browser_tabs[i].title), 0xFF000000);
    }

    int can_go_back = nav_history_pos > 0;
    int can_go_forward = nav_history_pos >= 0 && (nav_history_pos + 1) < nav_history_count;

    draw_rect_ui(NAV_BTN_BACK_X,   NAV_BTN_Y, NAV_BTN_W, NAV_BTN_H, 0xFFEEEEEE);
    draw_rect_ui(NAV_BTN_FWD_X,    NAV_BTN_Y, NAV_BTN_W, NAV_BTN_H, 0xFFEEEEEE);
    draw_rect_ui(NAV_BTN_RELOAD_X, NAV_BTN_Y, NAV_BTN_W, NAV_BTN_H, 0xFFEEEEEE);
    draw_rect_ui(NAV_BTN_HOME_X,   NAV_BTN_Y, NAV_BTN_W, NAV_BTN_H, 0xFFEEEEEE);

    draw_text_ui(NAV_BTN_BACK_X   + 7, NAV_BTN_Y + 6, "<", 1, can_go_back    ? 0xFF000000 : 0xFFAAAAAA);
    draw_text_ui(NAV_BTN_FWD_X    + 7, NAV_BTN_Y + 6, ">", 1, can_go_forward ? 0xFF000000 : 0xFFAAAAAA);
    draw_text_ui(NAV_BTN_RELOAD_X + 7, NAV_BTN_Y + 6, "R", 1, 0xFF000000);
    draw_text_ui(NAV_BTN_HOME_X   + 7, NAV_BTN_Y + 6, "H", 1, 0xFF000000);

    uint32_t box_color = address_bar_focused ? 0xFFFFFFFF : 0xFFEEEEEE;
    draw_rect_ui(ADDR_BAR_X, NAV_BTN_Y, browser_width - ADDR_BAR_X - 10, NAV_BTN_H, box_color);

    if (address_bar_len > 0) {
        draw_text_ui(ADDR_BAR_X + 5, NAV_BTN_Y + 6, address_bar_text, address_bar_len, 0xFF000000);
    }

    sys_fb_blit(pixel_buffer, browser_width, browser_height);
    last_scroll = scroll_y;
}
            input_detected = 0;
        char key = 0;
        if (sys_read(0, &key, 1) > 0) {
            input_detected = 1;

            if (address_bar_focused) {
                if (key == '\n' || key == '\r') {
                    address_bar_focused = 0;
                    navigate_to(address_bar_text);
                }
                else if (key == '\b' || key == 127) {
                    if (address_bar_len > 0) {
                        address_bar_len--;
                        address_bar_text[address_bar_len] = '\0';
                    }
                }
                else if (key >= 32 && key <= 126 && address_bar_len < 127) {
                    address_bar_text[address_bar_len++] = key;
                    address_bar_text[address_bar_len] = '\0';
                }
            } else if (focused_input) {
                int len = str_len(focused_input->input_value);
                
                if (key == '\b' || key == 127) { 
                    if (len > 0) {      
                        focused_input->input_value[len - 1] = '\0';
                    }
                } else if (key >= 32 && key <= 126 && len < 127) { 
                    focused_input->input_value[len] = key;
                    focused_input->input_value[len + 1] = '\0';
                }
                last_scroll = -1; 
                
            } else {
                if (key == 's') scroll_y += 40;
                else if (key == 'w') scroll_y -= 40;
                else if (key == 'q') break;

                if (scroll_y < 0) scroll_y = 0;
                if (scroll_y > max_scroll_y) scroll_y = max_scroll_y;
            }
            last_scroll = -1;
        }

        mx = 0, my = 0, mbtn = 0;
        sys_get_mouse(&mx, &my, &mbtn);

        int ui_h = TAB_BAR_H + 30; 

       
        if (root_node && my >= ui_h) {
            DOMNode* hover_hit = dom_find_at_point(root_node, mx, my + scroll_y);
            if (hover_hit != hovered_node) {
                hovered_node = hover_hit;
                compute_style_tree(root_node, 0); 
                last_scroll = -1; 
            }
        } else if (hovered_node != 0) {
            hovered_node = 0;
            compute_style_tree(root_node, 0);
            last_scroll = -1;
        }

        if ((mbtn & 1) == 0) {
            is_dragging_scrollbar = 0;
        }

        if(mbtn != 0){
            input_detected = 1;
            
            if (mbtn & 1) {
                if (!is_dragging_scrollbar && mx >= browser_width - 24 && my >= ui_h && max_scroll_y > 0) {
                    is_dragging_scrollbar = 1;
                }

                if (is_dragging_scrollbar) {
                    address_bar_focused = 0;
                    
                    int click_y = my - ui_h;
                    int scrollbar_h = (browser_height * browser_height) / doc_h;
                    if (scrollbar_h < 20) scrollbar_h = 20; 
                    
                    int track_y = click_y - (scrollbar_h / 2);
                    int track_h = (browser_height - ui_h) - scrollbar_h;
                    if (track_h < 1) track_h = 1; 
                    
                    scroll_y = (track_y * max_scroll_y) / track_h;
                    if (scroll_y < 0) scroll_y = 0;
                    if (scroll_y > max_scroll_y) scroll_y = max_scroll_y;
                    
                    last_scroll = -1;

                } else if (my < ui_h) {
                    if (my < TAB_BAR_H) {
                        int tab_w = 140;
                        int clicked_tab = mx / tab_w;
                        
                        int active_count = 0;
                        for(int i = 0; i < MAX_TABS; i++) if(browser_tabs[i].active) active_count++;
                        
                        if (clicked_tab < active_count) {
                            if (clicked_tab != current_tab_index) {
                                browser_tabs[current_tab_index].scroll_y = scroll_y;
                                str_copy(browser_tabs[current_tab_index].url, address_bar_text, 256);
                                
                                current_tab_index = clicked_tab;
                                navigate_to(browser_tabs[current_tab_index].url);
                                scroll_y = browser_tabs[current_tab_index].scroll_y;
                            }
                        } else if (mx >= active_count * tab_w && mx <= active_count * tab_w + 20 && active_count < MAX_TABS) {
                            browser_tabs[current_tab_index].scroll_y = scroll_y;
                            str_copy(browser_tabs[current_tab_index].url, address_bar_text, 256);
                            
                            current_tab_index = active_count;
                            browser_tabs[current_tab_index].active = 1;
                            browser_tabs[current_tab_index].scroll_y = 0;
                            str_copy(browser_tabs[current_tab_index].title, "Nova karta", 32);
                            str_copy(browser_tabs[current_tab_index].url, HOME_URL, 256); 
                            
                            navigate_to(HOME_URL);
                            scroll_y = 0;
                        }
                    } else {
                        if (mx >= NAV_BTN_BACK_X && mx < NAV_BTN_BACK_X + NAV_BTN_W &&
                            my >= NAV_BTN_Y && my < NAV_BTN_Y + NAV_BTN_H) {
                            nav_go_back();
                        } else if (mx >= NAV_BTN_FWD_X && mx < NAV_BTN_FWD_X + NAV_BTN_W &&
                                   my >= NAV_BTN_Y && my < NAV_BTN_Y + NAV_BTN_H) {
                            nav_go_forward();
                        } else if (mx >= NAV_BTN_RELOAD_X && mx < NAV_BTN_RELOAD_X + NAV_BTN_W &&
                                   my >= NAV_BTN_Y && my < NAV_BTN_Y + NAV_BTN_H) {
                            nav_reload();
                        } else if (mx >= NAV_BTN_HOME_X && mx < NAV_BTN_HOME_X + NAV_BTN_W &&
                                   my >= NAV_BTN_Y && my < NAV_BTN_Y + NAV_BTN_H) {
                            nav_go_home();
                        } else if (mx >= ADDR_BAR_X) {
                            address_bar_focused = 1;
                        }
                    }
                    last_scroll = -1;

                } else {
                    address_bar_focused = 0;

                    int doc_x = mx;
                    int doc_y = my + scroll_y;

                    DOMNode* hit = dom_find_at_point(root_node, doc_x, doc_y);
                    int js_handled = hit ? js_fire_click_bubbling(root_node, hit) : 0;

                    if (hit && hit->type == NODE_INPUT && !hit->is_submit_button) {
                        if (str_eq(hit->input_type, "checkbox")) {
                            hit->is_checked = !hit->is_checked; 
                            focused_input = 0; 
                        } else {
                            focused_input = hit;
                        }
                        last_scroll = -1; 
                    } else if (hit) {
                        focused_input = 0; 
                        last_scroll = -1;
                    }

                    DOMNode* submit_btn = 0;
                    if (hit) {
                        DOMNode* chain[64];
                        int chain_len = dom_collect_ancestor_chain(root_node, hit, chain, 64);
                        for (int ci = chain_len - 1; ci >= 0; ci--) {
                            if (chain[ci]->is_submit_button) { submit_btn = chain[ci]; break; }
                        }
                    }
                    
                    int form_submitted = 0;
                    if (submit_btn) {
                        DOMNode* form = find_enclosing_form(root_node, submit_btn);
                        if (form) {
                            last_scroll = -1;
                            submit_form(form);
                            form_submitted = 1;
                        }
                    }

                    if (!form_submitted) {
                        DOMNode* clicked_node = get_clicked_link(root_node, doc_x, doc_y);

                        if (clicked_node && clicked_node->href[0] != '\0') {
                            clicked_node->style.has_bg = 1;
                            clicked_node->style.bg_color = 0xFFFFFF00;
                            last_scroll = -1;

                            char full_url[256];
                            resolve_relative_url(clicked_node->href, full_url, sizeof(full_url));
                            navigate_to(full_url);
                        } else {
                            last_scroll = -1; 
                        }
                    }
                    (void)js_handled;
                }
            }
        }

        if (input_detected == 0) {
            sys_yield();
        }
    }

    sys_exit();
    return 0;
}