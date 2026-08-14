#include "DOM.h"
#include "../browser.h"
#include <stdint.h>
#include "../css/css.h"
#include "../js/headers/js_runtime.h"
#include "../utils/syscall/syscalls.h"
#include "../utils/string/str_utils.h"
#include "../navigation/navigation.h"
extern void js_collect_script(const char* src, int len) ;
extern int text_pixel_width(int len, int font_scale);
int layout_text(DOMNode* node, int wrap_w) {
    int scale = node->style.font_scale;
    int line_h = 8 * scale + 2;
    int chars_per_line = wrap_w / (8 * scale);
    if (chars_per_line < 1) chars_per_line = 1;

    node->cached_chars_per_line = chars_per_line; 

    int remaining = node->text_len;
    int lines = 0;
    int max_line_w = 0;
    while (remaining > 0) {
        int take = remaining < chars_per_line ? remaining : chars_per_line;
        int line_w = text_pixel_width(take, scale);
        if (line_w > max_line_w) max_line_w = line_w;
        remaining -= take;
        lines++;
    }
    node->render_w = max_line_w;
    node->render_h = lines * line_h;
    return node->render_h;
}


void paint_text(DOMNode* node, int x, int y, int wrap_w) {
    int scale = node->style.font_scale;
    int line_h = 8 * scale + 2;
   
    int chars_per_line = node->cached_chars_per_line;
    if (chars_per_line < 1) {
        chars_per_line = wrap_w / (8 * scale);
        if (chars_per_line < 1) chars_per_line = 1;
    }

    int remaining = node->text_len;
    const char* p = node->text;
    int cur_y = y;

    while (remaining > 0) {
        int take = remaining < chars_per_line ? remaining : chars_per_line;
        int line_w = text_pixel_width(take, scale);
        int draw_x = x;
        if (node->style.text_align == 1) draw_x = x + (wrap_w - line_w) / 2;
        else if (node->style.text_align == 2) draw_x = x + (wrap_w - line_w);
        if (draw_x < x) draw_x = x;

        draw_text_scaled(draw_x, cur_y, p, take, node->style.text_color, scale);

        p += take;
        remaining -= take;
        cur_y += line_h;
    }
}


void layout_dom_node(DOMNode* node, int start_x, int start_y, int avail_w, int* out_w, int* out_h) {
    if (!node) { *out_w = 0; *out_h = 0; return; }

    node->render_x = start_x;
    node->render_y = start_y;

    if (node->style.is_inline == -1) { // display: none
        node->render_w = 0; node->render_h = 0;
        *out_w = 0; *out_h = 0;
        return;
    }

    if (node->type == NODE_BR) {
        node->render_w = 0; node->render_h = 8;
        *out_w = 0; *out_h = 8;
        return;
    }

    if (node->type == NODE_TEXT) {
        node->parent_avail_w = avail_w > 0 ? avail_w : 760;
        if (node->text_len > 0) {
            layout_text(node, node->parent_avail_w);
        } else {
            node->render_w = 0; node->render_h = 0;
        }
        *out_w = node->render_w;
        *out_h = node->render_h;
        return;
    }

    if (node->type == NODE_IMG) {
        // Získáme rozměry obrázku
        const uint8_t* d = node->img_data;
        int w = 0, h = 0;
        if (d && d[0] == 0x42 && d[1] == 0x4D) {
            w = *(int*)&d[18];
            h = *(int*)&d[22];
        }
        node->render_w = w; node->render_h = h;
        *out_w = w; *out_h = h;
        return;
    }

    if (node->type == NODE_INPUT) {
        int w = str_eq(node->input_type, "submit") || str_eq(node->input_type, "button") ? 100 : 160;
        if (str_eq(node->input_type, "checkbox")) w = 16; // Pevná šířka pro checkbox
        
        int h = str_eq(node->input_type, "checkbox") ? 16 : 22; // Pevná výška pro checkbox
        
        node->render_w = w;
        node->render_h = h;
        *out_w = w + node->style.margin * 2;
        *out_h = h + node->style.margin * 2;
        return;
    }

    // --- Box model ---
    ComputedStyle* st = &node->style;
    int box_x = start_x + st->margin;
    int box_y = start_y + st->margin;
    int box_w = st->has_width ? st->width : avail_w;
    if (box_w <= 0) box_w = avail_w > 0 ? avail_w : 760;

    int li_indent = (node->type == NODE_LI) ? 16 : 0;
    int content_x = box_x + st->padding + li_indent;
    int content_y = box_y + st->padding;
    int content_avail_w = box_w - st->padding * 2 - li_indent;
    if (content_avail_w < 8) content_avail_w = 8;

    int current_x = content_x;
    int current_y = content_y;
    int row_max_h = 0;

    for (int i = 0; i < node->child_count; i++) {
        DOMNode* child = node->children[i];
        if (child->style.is_inline == -1) continue; // display:none

        int child_w = 0, child_h = 0;
        int child_is_block = (child->style.is_inline == 0);

        if (child_is_block) {
            if (child->style.float_side == 1) { 
                // =====================================
                // FLOAT: LEFT
                // =====================================
                int remaining_w = content_x + content_avail_w - current_x;
                layout_dom_node(child, current_x, current_y, remaining_w, &child_w, &child_h);
                
                current_x += child_w; 
                if (child_h + child->style.margin * 2 > row_max_h) {
                    row_max_h = child_h + child->style.margin * 2;
                }
                
            } else if (child->style.float_side == 2) { 
                // =====================================
                // FLOAT: RIGHT (Dvojitý průchod)
                // =====================================
                int remaining_w = content_x + content_avail_w - current_x;
                
                // 1. Průchod nanečisto
                layout_dom_node(child, current_x, current_y, remaining_w, &child_w, &child_h);
                
                // Vypočtení pozice pro zarovnání vpravo
                int right_x = content_x + content_avail_w - child_w;
                if (right_x < current_x) right_x = current_x; 
                
                layout_dom_node(child, right_x, current_y, child_w, &child_w, &child_h);
                
                content_avail_w -= child_w; 
                
                if (child_h + child->style.margin * 2 > row_max_h) {
                    row_max_h = child_h + child->style.margin * 2;
                }
                
            } else { 
                // =====================================
                // BĚŽNÝ BLOK (clear chování)
                // =====================================
                if (current_x > content_x) {
                    current_y += row_max_h;
                    current_x = content_x;
                    row_max_h = 0;
                }
                layout_dom_node(child, current_x, current_y, content_avail_w, &child_w, &child_h);
                current_y += child_h + child->style.margin * 2;
            }
        } else {
            // =====================================
            // INLINE (Text, obrázky, tlačítka)
            // =====================================
            int est_w = (child->type == NODE_TEXT)
                ? text_pixel_width(child->text_len, child->style.font_scale) : 0;
            if (current_x + est_w > content_x + content_avail_w && current_x > content_x) {
                current_y += row_max_h;
                current_x = content_x;
                row_max_h = 0;
            }
            int remaining_w = content_x + content_avail_w - current_x;
            layout_dom_node(child, current_x, current_y, remaining_w, &child_w, &child_h);
            current_x += child_w;
            if (child_h > row_max_h) row_max_h = child_h;
        }
    }
    current_y += row_max_h;

    int natural_h = current_y - content_y;
    int box_h = st->height > 0 ? st->height : (natural_h + st->padding * 2);
    if (box_h < st->padding * 2) box_h = st->padding * 2;

    node->render_w = box_w;
    node->render_h = box_h;
    *out_w = box_w + st->margin * 2;
    *out_h = box_h + st->margin * 2;
}


void paint_dom_node(DOMNode* node) {
    if (!node) return;
    if (node->style.is_inline == -1) return; // display:none

    if (node->type == NODE_TEXT) {
        if (node->text_len > 0) {
            paint_text(node, node->render_x, node->render_y, node->parent_avail_w);
        }
        return;
    }

    if (node->type == NODE_IMG) {
        int w, h;
        draw_bmp(node->render_x, node->render_y, node->img_data, &w, &h);
        return;
    }

    if (node->type == NODE_BR) return;

    if (node->type == NODE_INPUT) {
        // --- 1.  CHECKBOX ---
        if (str_eq(node->input_type, "checkbox")) {
            //  čtvereček 16x16
            draw_rect_user(node->render_x, node->render_y, 16, 16, 0xFFFFFFFF);
            draw_border_user(node->render_x, node->render_y, 16, 16, 1, 0xFF000000);
            
            if (node->is_checked) {
                draw_rect_user(node->render_x + 3, node->render_y + 3, 10, 10, 0xFF000000);
            }
            return; 
        }

        // --- 2.  POLE text, heslo a tlačítka ---
        int is_button_like = str_eq(node->input_type, "submit") || str_eq(node->input_type, "button");
        
        // Změna barvy pozadí
        uint32_t bg = is_button_like ? 0xFFE0E0E0 : (focused_input == node ? 0xFFEEFFFF : 0xFFFFFFFF);
        draw_rect_user(node->render_x, node->render_y, node->render_w, node->render_h, bg);
        
        // Zvýraznění rámečku, pokud je focused
        uint32_t border_col = (focused_input == node) ? 0xFF0000FF : 0xFF888888;
        draw_border_user(node->render_x, node->render_y, node->render_w, node->render_h, 1, border_col);

        const char* label = node->input_value[0] ? node->input_value
                           : (str_eq(node->input_type, "submit") ? "Submit"
                           : (str_eq(node->input_type, "password") ? "" : ""));
        
        int cursor_x_offset = 0; 

        if (label[0] || focused_input == node) {
            if (str_eq(node->input_type, "password")) {
                char masked[128]; int mi = 0;
                for (; node->input_value[mi] && mi < 127; mi++) masked[mi] = '*';
                masked[mi] = '\0';
                draw_text_user(node->render_x + 6, node->render_y + 6, masked, mi, 0xFF000000);
                cursor_x_offset = mi * 8;
            } else {
                draw_text_user(node->render_x + 6, node->render_y + 6, label, str_len(label), 0xFF000000);
                
                // Spočítáme skutečnou šířku textu 
                for (int i = 0; i < str_len(label); i += utf8_char_len(label + i)) {
                    cursor_x_offset += 8;
                }
            }
            
            // Nakreslíme blikající kurzor 
            if (focused_input == node && (sys_get_time_ms() % 1000 < 500)) {
                draw_char_user(node->render_x + 6 + cursor_x_offset, node->render_y + 6, '|', 0xFF000000);
            }
        }
        return;
    }

    // --- 3. ELEMENTY (boxy, odstavce, divy...) ---
    ComputedStyle* st = &node->style;
    int box_x = node->render_x + st->margin;
    int box_y = node->render_y + st->margin;
    int box_w = node->render_w;
    int box_h = node->render_h;

    // Vykreslení pozadí (s podporou pro border-radius)
    if (st->has_bg && st->bg_color != 0x00000000) {
        if (st->has_border_radius && st->border_radius > 0) {
            draw_rounded_rect_user(box_x, box_y, box_w, box_h, st->border_radius, st->bg_color);
        } else {
            draw_rect_user(box_x, box_y, box_w, box_h, st->bg_color);
        }
    }
    
    // Vykreslení rámečku
    if (st->border_w > 0) {
        draw_border_user(box_x, box_y, box_w, box_h, st->border_w, st->border_color);
    }
    
    // Vykreslení odrážky seznamu
    if (node->type == NODE_LI) {
        draw_rect_user(box_x + st->padding + 4, box_y + st->padding + 3, 4, 4, st->text_color);
    }

    // 4. Rekurzivní vykreslení potomků
    for (int i = 0; i < node->child_count; i++) {
        paint_dom_node(node->children[i]);
    }
}

void render_dom_node(DOMNode* node, int start_x, int start_y, int* out_w, int* out_h) {
    layout_dom_node(node, start_x, start_y, browser_width, out_w, out_h);
    paint_dom_node(node);
}

// ============================================================================
// 8. HTML PARSER 
// ============================================================================

int extract_attr(const char* tag_start, const char* tag_end, const char* attr_name,
                  char* out, int out_size) {
    out[0] = '\0';
    int name_len = str_len(attr_name);
    const char* p = tag_start;
    while (p < tag_end) {
        const char* found = strstr(p, attr_name);
        if (!found || found >= tag_end) return 0;
        char before = (found == tag_start) ? ' ' : *(found - 1);
        if ((before == ' ' || before == '\t' || before == '\n') && found[name_len] == '=' ) {
            const char* val = found + name_len + 1;
            char quote = *val;
            if (quote == '"' || quote == '\'') {
                val++;
                int i = 0;
                while (val[i] != quote && val[i] != '\0' && val < tag_end && i < out_size - 1) {
                    out[i] = val[i]; i++;
                }
                out[i] = '\0';
                return 1;
            }
        }
        p = found + name_len;
    }
    return 0;
}

// Naparsuje jen PRVNÍ třídu z atributu class="a b c" (vícenásobné třídy na
// jednom elementu nejsou v tomto enginu podporované 
void extract_first_class(char* class_attr) {
    int i = 0;
    while (class_attr[i] && class_attr[i] != ' ') i++;
    class_attr[i] = '\0';
}

// Vloží do parent->children nový text node s obsahem [text, text+len).
void append_text_node(DOMNode* parent, const char* text, int len) {
    if (!parent || len <= 0) return;
    int is_just_spaces = 1;
    for (int i = 0; i < len; i++) { if ((unsigned char)text[i] > 32) is_just_spaces = 0; }
    if (is_just_spaces) return;

    int copy_len = len;
    if (copy_len > 127) copy_len = 127;

    DOMNode* text_node = alloc_node();
    if (!text_node) return;
    text_node->type = NODE_TEXT;
    text_node->text_len = copy_len;
    for (int i = 0; i < copy_len; i++) text_node->text[i] = text[i];
    text_node->text[copy_len] = '\0';

    if (parent->child_count < MAX_CHILDREN) {
        parent->children[parent->child_count++] = text_node;
    }
}

// Vyplní tag_name/class_name/id_name 
void fill_node_attrs(DOMNode* node, const char* tag_name, const char* tag_start, const char* tag_end) {
    str_copy(node->tag_name, tag_name, sizeof(node->tag_name));
    
    char class_attr[64];
    if (extract_attr(tag_start, tag_end, "class", class_attr, sizeof(class_attr))) {
        str_copy(node->class_name, class_attr, sizeof(node->class_name));
    }
    
    char id_attr[64];
    if (extract_attr(tag_start, tag_end, "id", id_attr, sizeof(id_attr))) {
        str_copy(node->id_name, id_attr, sizeof(node->id_name)); 
    }

    // ==========================================
    // 1. Zpracování INPUT polí (včetně checkboxu)
    // ==========================================
    if (str_eq(node->tag_name, "input")) {
        // Tohle tu MUSÍ ZŮSTAT z dřívějška, jinak nebudou fungovat textová pole:
        extract_attr(tag_start, tag_end, "type", node->input_type, sizeof(node->input_type));
        extract_attr(tag_start, tag_end, "name", node->input_name, sizeof(node->input_name));
        extract_attr(tag_start, tag_end, "value", node->input_value, sizeof(node->input_value));
        
        char dummy[16];
        if (extract_attr(tag_start, tag_end, "checked", dummy, sizeof(dummy))) {
            node->is_checked = 1;
        } else {
            const char* p = tag_start;
            while (p < tag_end) {
                if (str_eq_n(p, "checked", 7)) { node->is_checked = 1; break; }
                p++;
            }
        }
    }
    
    if (str_eq(node->tag_name, "button")) {
        extract_attr(tag_start, tag_end, "type", node->input_type, sizeof(node->input_type));
    }
    if (str_eq(node->tag_name, "a")) {
        extract_attr(tag_start, tag_end, "href", node->href, sizeof(node->href));
    }
}

DOMNode* build_dom_tree(const char* html) {
    nodes_allocated = 0;
    css_rule_count = 0; 
    js_script_block_count = 0;
    img_pool_used = 0;
    DOMNode* root = alloc_node();
    root->type = NODE_DIV;
    str_copy(root->tag_name, "div", sizeof(root->tag_name));
    root->style.has_width = 1;  root->style.width = browser_width;
    root->style.has_height = 1; root->style.height = browser_height;
    root->style.has_bg = 1;     root->style.bg_color = 0xFFEEEEEE;
    root->style.has_margin = 1; root->style.margin = 0;
    root->style.has_padding = 1; root->style.padding = 10;
    root->style.has_display = 1; root->style.is_inline = 0;

    DOMNode* stack[128];
    int stack_ptr = 0;
    stack[stack_ptr++] = root;

    const char* cursor = html;

    while (*cursor) {
        const char* next_tag = strstr(cursor, "<");
     
        if (!next_tag) {
            append_text_node(stack[stack_ptr - 1], cursor, str_len(cursor));
            break;
        }

        if (next_tag > cursor) {
            append_text_node(stack[stack_ptr - 1], cursor, next_tag - cursor);
        }

        const char* tag_end = strstr(next_tag, ">");
        if (!tag_end) break; 

        // --- <style> ... </style>  ---
        if (str_starts_with(next_tag, "<style")) {
            const char* content_start = tag_end + 1;
            const char* close = strstr(content_start, "</style");
            if (close) {
                // zkopírujeme obsah do pomocného bufferu a naparsujeme jako CSS
                static char css_tmp[8192];
                int len = close - content_start;
                if (len > 8191) len = 8191;
                for (int i = 0; i < len; i++) css_tmp[i] = content_start[i];
                css_tmp[len] = '\0';
                parse_css_block(css_tmp);
                cursor = strstr(close, ">");
                cursor = cursor ? cursor + 1 : close;
            } else {
                cursor = tag_end + 1;
            }
            continue;
        }
        if (str_starts_with(next_tag, "<img")) {
            DOMNode* new_img = alloc_node();
            new_img->type = NODE_IMG;
            str_copy(new_img->tag_name, "img", sizeof(new_img->tag_name));
            fill_node_attrs(new_img, "img", next_tag, tag_end);

            char src_attr[128];
            if (extract_attr(next_tag, tag_end, "src", src_attr, sizeof(src_attr))) {
                // Rozbalíme relativní URL na plnou 
                char full_src[256];
                resolve_relative_url(src_attr, full_src, sizeof(full_src));

                char img_domain[128], img_path[128];
                int img_is_https = 1, img_port = 0, img_is_ip = 0;
                parse_url(full_src, img_domain, img_path, &img_is_https, &img_port, &img_is_ip);

                uint32_t ip = str_eq(img_domain, current_domain)
                    ? current_server_ip
                    : (img_is_ip ? parse_ipv4(img_domain) : sys_dns_resolve(img_domain));
                
                int port = img_port ? img_port : (img_is_https ? 443 : 80);

                if (img_pool_used < IMG_POOL_SIZE) {
                    int max_dl = IMG_POOL_SIZE - img_pool_used;
                    int bytes = img_is_https
                        ? sys_https_get(ip, img_domain, (uint16_t)port, img_path, (char*)&img_pool[img_pool_used], max_dl)
                        : sys_http_get(ip, (uint16_t)port, img_path, (char*)&img_pool[img_pool_used], max_dl);

                    if (bytes > 0 && img_pool[img_pool_used] == 0x42 && img_pool[img_pool_used + 1] == 0x4D) {
                        new_img->img_data = &img_pool[img_pool_used];
                        img_pool_used += bytes; 
                    } else {
                        new_img->img_data = test_image_bmp; 
                    }
                } else {
                    new_img->img_data = test_image_bmp; 
                }
            } else {
                new_img->img_data = test_image_bmp;
            }

            DOMNode* parent = stack[stack_ptr - 1];
            if (parent->child_count < MAX_CHILDREN) parent->children[parent->child_count++] = new_img;
            cursor = tag_end + 1;
            continue;
        }
        // --- <script> ... </script>  nebo  <script src="..."></script> ---
        if (str_starts_with(next_tag, "<script")) {
            char src_attr[128];
            int has_src = extract_attr(next_tag, tag_end, "src", src_attr, sizeof(src_attr));

            const char* content_start = tag_end + 1;
            const char* close = strstr(content_start, "</script");
            const char* after = close ? strstr(close, ">") : 0;

            if (has_src) {
                char s_domain[128], s_path[128];
                static char js_dl_buf[SCRIPT_BLOCK_SIZE];

             
                char full_src[256];
                resolve_relative_url(src_attr, full_src, sizeof(full_src));

                int s_is_https = 1, s_port = 0, s_is_ip = 0;
                parse_url(full_src, s_domain, s_path, &s_is_https, &s_port, &s_is_ip);
           
                uint32_t ip = str_eq(s_domain, current_domain)
                    ? current_server_ip
                    : (s_is_ip ? parse_ipv4(s_domain) : sys_dns_resolve(s_domain));
                const char* target_domain = s_domain;

                int port = s_port ? s_port : (s_is_https ? 443 : 80);

                for (int i = 0; i < SCRIPT_BLOCK_SIZE; i++) js_dl_buf[i] = 0;

                int bytes = s_is_https
                    ? sys_https_get(ip, target_domain, (uint16_t)port, s_path, js_dl_buf, SCRIPT_BLOCK_SIZE)
                    : sys_http_get(ip, (uint16_t)port, s_path, js_dl_buf, SCRIPT_BLOCK_SIZE);

                if (bytes > 0) js_collect_script(js_dl_buf, bytes);
            } else if (close) {
                js_collect_script(content_start, close - content_start);
            }

            cursor = after ? after + 1 : (close ? close + 1 : tag_end + 1);
            continue;
        }

    

        // --- uzavírací tagy běžných block/inline elementů — pop ze stacku ---
        if (str_starts_with(next_tag, "</div") || str_starts_with(next_tag, "</p")  ||
            str_starts_with(next_tag, "</h1")  || str_starts_with(next_tag, "</h2") ||
            str_starts_with(next_tag, "</h3")  || str_starts_with(next_tag, "</span") ||
            str_starts_with(next_tag, "</ul")  || str_starts_with(next_tag, "</li") ||
            str_starts_with(next_tag, "</a")   || str_starts_with(next_tag, "</body") ||
            str_starts_with(next_tag, "</form") || str_starts_with(next_tag, "</button")) {
            if (stack_ptr > 1) stack_ptr--;
            cursor = tag_end + 1;
            continue;
        }

        // --- <body> ---
        if (str_starts_with(next_tag, "<body")) {
            cursor = tag_end + 1;
            continue;
        }

        // --- samostatné/nepárové tagy ---
        if (str_starts_with(next_tag, "<br")) {
            DOMNode* br = alloc_node();
            if (br) {
                br->type = NODE_BR;
                str_copy(br->tag_name, "br", sizeof(br->tag_name));
                DOMNode* parent = stack[stack_ptr - 1];
                if (parent->child_count < MAX_CHILDREN) parent->children[parent->child_count++] = br;
            }
            cursor = tag_end + 1;
            continue;
        }

        if (str_starts_with(next_tag, "<img")) {
            DOMNode* new_img = alloc_node();
            new_img->type = NODE_IMG;
            str_copy(new_img->tag_name, "img", sizeof(new_img->tag_name));
            new_img->img_data = test_image_bmp; // TODO: stahování z atributu 'src'
            fill_node_attrs(new_img, "img", next_tag, tag_end);

            DOMNode* parent = stack[stack_ptr - 1];
            if (parent->child_count < MAX_CHILDREN) parent->children[parent->child_count++] = new_img;
            cursor = tag_end + 1;
            continue;
        }

        // --- párové tagy ---
        const char* tag_match = 0;
        NodeType node_type = NODE_DIV;
        const char* tag_name_str = "div";

        if (str_starts_with(next_tag, "<div"))  { tag_match = next_tag; node_type = NODE_DIV;  tag_name_str = "div";  }
        else if (str_starts_with(next_tag, "<p")  && !is_alpha_c(next_tag[2])) { tag_match = next_tag; node_type = NODE_P;    tag_name_str = "p";    }
        else if (str_starts_with(next_tag, "<h1")) { tag_match = next_tag; node_type = NODE_H1;   tag_name_str = "h1";   }
        else if (str_starts_with(next_tag, "<h2")) { tag_match = next_tag; node_type = NODE_H2;   tag_name_str = "h2";   }
        else if (str_starts_with(next_tag, "<h3")) { tag_match = next_tag; node_type = NODE_H3;   tag_name_str = "h3";   }
        else if (str_starts_with(next_tag, "<span")) { tag_match = next_tag; node_type = NODE_SPAN; tag_name_str = "span"; }
        else if (str_starts_with(next_tag, "<ul"))  { tag_match = next_tag; node_type = NODE_UL;   tag_name_str = "ul";   }
        else if (str_starts_with(next_tag, "<li"))  { tag_match = next_tag; node_type = NODE_LI;   tag_name_str = "li";   }

        if (tag_match) {
            DOMNode* new_node = alloc_node();
            if (new_node) {
                new_node->type = node_type;
                fill_node_attrs(new_node, tag_name_str, next_tag, tag_end);

                DOMNode* parent = stack[stack_ptr - 1];
                if (parent->child_count < MAX_CHILDREN) parent->children[parent->child_count++] = new_node;
                if (stack_ptr < 128) stack[stack_ptr++] = new_node;
            }
            cursor = tag_end + 1;
            continue;
        }

        if (str_starts_with(next_tag, "<a ") || str_starts_with(next_tag, "<a>")) {
            DOMNode* new_link = alloc_node();
            if (new_link) {
                new_link->type = NODE_DIV;
                new_link->is_link = 1;
                fill_node_attrs(new_link, "a", next_tag, tag_end);
                // odkazy jsou ve výchozím stavu inline a modré 
                new_link->style.has_display = 1; new_link->style.is_inline = 1;
                new_link->style.has_color = 1;   new_link->style.text_color = 0xFF0000FF;
                new_link->style.has_bg = 1;       new_link->style.bg_color = 0x00000000;

                char href_attr[128];
                if (extract_attr(next_tag, tag_end, "href", href_attr, sizeof(href_attr))) {
                    str_copy(new_link->href, href_attr, sizeof(new_link->href));
                }

                DOMNode* parent = stack[stack_ptr - 1];
                if (parent && parent->child_count < MAX_CHILDREN) parent->children[parent->child_count++] = new_link;
                if (stack_ptr < 128) stack[stack_ptr++] = new_link;
            }
            cursor = tag_end + 1;
            continue;
        }

        // --- <form action="..." method="get|post"> — párový tag, otevírá
       
        if (str_starts_with(next_tag, "<form")) {
            DOMNode* new_form = alloc_node();
            if (new_form) {
                new_form->type = NODE_FORM;
                fill_node_attrs(new_form, "form", next_tag, tag_end);

                char action_attr[128];
                if (extract_attr(next_tag, tag_end, "action", action_attr, sizeof(action_attr))) {
                    str_copy(new_form->form_action, action_attr, sizeof(new_form->form_action));
                }
     
                char method_attr[16];
                new_form->form_method = 0; // GET
                if (extract_attr(next_tag, tag_end, "method", method_attr, sizeof(method_attr))) {
                    if (str_eq(method_attr, "post") || str_eq(method_attr, "POST")) new_form->form_method = 1;
                }

                DOMNode* parent = stack[stack_ptr - 1];
                if (parent && parent->child_count < MAX_CHILDREN) parent->children[parent->child_count++] = new_form;
                if (stack_ptr < 128) stack[stack_ptr++] = new_form;
            }
            cursor = tag_end + 1;
            continue;
        }

        // --- <input type="..." name="..." value="..."> 
        if (str_starts_with(next_tag, "<input")) {
            DOMNode* new_input = alloc_node();
            if (new_input) {
                new_input->type = NODE_INPUT;
                fill_node_attrs(new_input, "input", next_tag, tag_end);

                char name_attr[64];
                if (extract_attr(next_tag, tag_end, "name", name_attr, sizeof(name_attr))) {
                    str_copy(new_input->input_name, name_attr, sizeof(new_input->input_name));
                }
                char value_attr[128];
                if (extract_attr(next_tag, tag_end, "value", value_attr, sizeof(value_attr))) {
                    str_copy(new_input->input_value, value_attr, sizeof(new_input->input_value));
                }
                char type_attr[16];
                if (extract_attr(next_tag, tag_end, "type", type_attr, sizeof(type_attr))) {
                    str_copy(new_input->input_type, type_attr, sizeof(new_input->input_type));
                } else {
                    str_copy(new_input->input_type, "text", sizeof(new_input->input_type)); // HTML spec default
                }
                if (str_eq(new_input->input_type, "submit")) new_input->is_submit_button = 1;

                DOMNode* parent = stack[stack_ptr - 1];
                if (parent && parent->child_count < MAX_CHILDREN) parent->children[parent->child_count++] = new_input;
            }
            cursor = tag_end + 1;
            continue;
        }

        // --- <button type="submit">Text</button> ---
        if (str_starts_with(next_tag, "<button")) {
            DOMNode* new_button = alloc_node();
            if (new_button) {
                new_button->type = NODE_BUTTON;
                fill_node_attrs(new_button, "button", next_tag, tag_end);

                char type_attr[16];
                int has_type = extract_attr(next_tag, tag_end, "type", type_attr, sizeof(type_attr));
                if (!has_type || str_eq(type_attr, "submit")) new_button->is_submit_button = 1;

                DOMNode* parent = stack[stack_ptr - 1];
                if (parent && parent->child_count < MAX_CHILDREN) parent->children[parent->child_count++] = new_button;
                if (stack_ptr < 128) stack[stack_ptr++] = new_button;
            }
            cursor = tag_end + 1;
            continue;
        }

        // neznámý/nepodporovaný tag (např. <meta>, <title>, <!DOCTYPE>...) - ignorujeme
        cursor = tag_end + 1;
    }

    compute_style_tree(root, 0);
    return root;
}