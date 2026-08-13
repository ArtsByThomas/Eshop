#include "DOM.h"
#include "../css/css.h"
extern int str_len(const char *s);
extern int str_eq(const char *a, const char *b);
extern int str_eq_n(const char *a, const char *b, int n);
DOMNode* alloc_node() {
    if (nodes_allocated >= MAX_NODES) {
        return 0;
    }
    DOMNode* n = &node_pool[nodes_allocated++];
    n->child_count = 0;
    n->text_len = 0;
    n->tag_name[0] = '\0';
    n->class_name[0] = '\0';
    n->id_name[0] = '\0';
    n->is_link = 0;
    n->href[0] = '\0';
    n->img_data = 0;
    n->parent_avail_w = 0;

    n->form_action[0] = '\0';
    n->form_method = 0;
    n->input_name[0] = '\0';
    n->input_value[0] = '\0';
    n->input_type[0] = '\0';
    n->is_submit_button = 0;
    for (int i = 0; i < (int)sizeof(ComputedStyle); i++) ((char*)&n->style)[i] = 0;
    return n;
}


void apply_default_style(DOMNode* n) {
    ComputedStyle* s = &n->style;
    switch (n->type) {
        case NODE_DIV:
            if (!s->has_width)   { s->has_width = 1;   s->width = 760; }
            if (!s->has_bg)      { s->has_bg = 1;       s->bg_color = 0x00000000; }
            if (!s->has_color)   { s->has_color = 1;    s->text_color = 0xFF000000; }
            if (!s->has_margin)  { s->has_margin = 1;   s->margin = 4; }
            if (!s->has_padding) { s->has_padding = 1;  s->padding = 4; }
            if (!s->has_display) { s->has_display = 1;  s->is_inline = 0; }
            break;
        case NODE_P:
            if (!s->has_color)   { s->has_color = 1;    s->text_color = 0xFF000000; }
            if (!s->has_margin)  { s->has_margin = 1;   s->margin = 6; }
            if (!s->has_padding) { s->has_padding = 1;  s->padding = 0; }
            if (!s->has_display) { s->has_display = 1;  s->is_inline = 0; }
            break;
        case NODE_H1:
            if (!s->has_color)     { s->has_color = 1;     s->text_color = 0xFF000000; }
            if (!s->has_font_size) { s->has_font_size = 1;  s->font_scale = 3; }
            if (!s->has_margin)    { s->has_margin = 1;     s->margin = 8; }
            if (!s->has_display)   { s->has_display = 1;    s->is_inline = 0; }
            break;
        case NODE_H2:
            if (!s->has_color)     { s->has_color = 1;     s->text_color = 0xFF000000; }
            if (!s->has_font_size) { s->has_font_size = 1;  s->font_scale = 2; }
            if (!s->has_margin)    { s->has_margin = 1;     s->margin = 7; }
            if (!s->has_display)   { s->has_display = 1;    s->is_inline = 0; }
            break;
        case NODE_H3:
            if (!s->has_color)     { s->has_color = 1;     s->text_color = 0xFF000000; }
            if (!s->has_font_size) { s->has_font_size = 1;  s->font_scale = 1; }
            if (!s->has_margin)    { s->has_margin = 1;     s->margin = 6; }
            if (!s->has_display)   { s->has_display = 1;    s->is_inline = 0; }
            break;
        case NODE_SPAN:
            if (!s->has_color)   { s->has_color = 1;    s->text_color = 0xFF000000; }
            if (!s->has_display) { s->has_display = 1;  s->is_inline = 1; }
            break;
        case NODE_LI:
            if (!s->has_color)   { s->has_color = 1;    s->text_color = 0xFF000000; }
            if (!s->has_margin)  { s->has_margin = 1;   s->margin = 2; }
            if (!s->has_padding) { s->has_padding = 1;  s->padding = 0; }
            if (!s->has_display) { s->has_display = 1;  s->is_inline = 0; }
            break;
        case NODE_UL:
            if (!s->has_margin)  { s->has_margin = 1;   s->margin = 4; }
            if (!s->has_padding) { s->has_padding = 1;  s->padding = 0; }
            if (!s->has_display) { s->has_display = 1;  s->is_inline = 0; }
            break;
        default:
            if (!s->has_color)   { s->has_color = 1;    s->text_color = 0xFF000000; }
            if (!s->has_display) { s->has_display = 1;  s->is_inline = 0; }
            break;
    }
    if (!s->has_font_size) { s->has_font_size = 1; s->font_scale = 1; }
    if (!s->has_text_align) { s->has_text_align = 1; s->text_align = 0; }
    if (!s->has_border) { s->has_border = 1; s->border_w = 0; s->border_color = 0; }
    if (!s->has_border_radius) { s->has_border_radius = 1; s->border_radius = 0; }
    if (!s->has_height) { s->has_height = 1; s->height = 0; }
    if (!s->has_float) { s->has_float = 1; s->float_side = 0; }
}

void merge_style(ComputedStyle* dst, const ComputedStyle* src) {
    if (src->has_width)      { dst->has_width = 1;      dst->width = src->width; }
    if (src->has_height)     { dst->has_height = 1;     dst->height = src->height; }
    if (src->has_bg)         { dst->has_bg = 1;          dst->bg_color = src->bg_color; }
    if (src->has_color)      { dst->has_color = 1;       dst->text_color = src->text_color; }
    if (src->has_margin)     { dst->has_margin = 1;      dst->margin = src->margin; }
    if (src->has_padding)    { dst->has_padding = 1;     dst->padding = src->padding; }
    if (src->has_display)    { dst->has_display = 1;     dst->is_inline = src->is_inline; }
    if (src->has_font_size)  { dst->has_font_size = 1;   dst->font_scale = src->font_scale; }
    if (src->has_text_align) { dst->has_text_align = 1;  dst->text_align = src->text_align; }
    if (src->has_border)     { dst->has_border = 1;       dst->border_w = src->border_w; dst->border_color = src->border_color; }
    if (src->has_float) { dst->has_float = 1; dst->float_side = src->float_side; }
    if (src->has_border_radius) { dst->has_border_radius = 1; dst->border_radius = src->border_radius; } 
}

int rule_specificity(const CSSRule* r) {
    if (r->sel_type == SEL_ID) return 3;
    if (r->sel_type == SEL_CLASS) return 2;
    return 1;
}
int has_class(const char* class_attr, const char* target) {
    if (!class_attr || !target || !target[0]) return 0;
    int tlen = str_len(target);
    const char* p = class_attr;
    while (*p) {
        while (*p == ' ') p++; // přeskoč mezery
        if (!*p) break;
        const char* start = p;
        while (*p && *p != ' ') p++;
        int len = p - start;
        if (len == tlen && str_eq_n(start, target, len)) return 1;
    }
    return 0;
}
int rule_matches(CSSRule* r, DOMNode* n) {
    if (!n) return 0;
    
    if (r->is_hover && n != hovered_node) return 0; 

    if (r->sel_type == SEL_ID) return str_eq(n->id_name, r->sel_name);
    if (r->sel_type == SEL_CLASS) return has_class(n->class_name, r->sel_name);
    if (r->sel_type == SEL_TAG) return str_eq(n->tag_name, r->sel_name);
    return 0;
}


void compute_style(DOMNode* n, ComputedStyle* parent_style) {
    for (int i = 0; i < (int)sizeof(ComputedStyle); i++) ((char*)&n->style)[i] = 0;

    int idx[MAX_CSS_RULES]; int cnt = 0;
    for (int i = 0; i < css_rule_count; i++) {
        if (rule_matches(&css_rules[i], n)) idx[cnt++] = i;
    }
    for (int a = 0; a < cnt; a++) {
        for (int b = a + 1; b < cnt; b++) {
            if (rule_specificity(&css_rules[idx[b]]) < rule_specificity(&css_rules[idx[a]]) || 
               (rule_specificity(&css_rules[idx[b]]) == rule_specificity(&css_rules[idx[a]]) && css_rules[idx[b]].order < css_rules[idx[a]].order)) {
                int tmp = idx[a]; idx[a] = idx[b]; idx[b] = tmp; 
            }
        }
    }
    for (int i = 0; i < cnt; i++) {
        merge_style(&n->style, &css_rules[idx[i]].decl);
    }

    // ==========================================
    // CSS DĚDIČNOST (Inheritance)
    // ==========================================
    if (parent_style) {
        if (!n->style.has_color && parent_style->has_color) {
            n->style.has_color = 1;
            n->style.text_color = parent_style->text_color;
        }
        if (!n->style.has_font_size && parent_style->has_font_size) {
            n->style.has_font_size = 1;
            n->style.font_scale = parent_style->font_scale;
        }
        if (!n->style.has_text_align && parent_style->has_text_align) {
            n->style.has_text_align = 1;
            n->style.text_align = parent_style->text_align;
        }
    }

    apply_default_style(n);
}

void compute_style_tree(DOMNode* n, ComputedStyle* parent_style) {
    if (!n) return;
    compute_style(n, parent_style);
    for (int i = 0; i < n->child_count; i++) {
        compute_style_tree(n->children[i], &n->style);
    }
}

DOMNode* get_clicked_link(DOMNode* node, int doc_x, int doc_y) {
    if (!node) return 0;

    if (node->is_link) {
        if (doc_x >= node->render_x && doc_x < (node->render_x + node->render_w) &&
            doc_y >= node->render_y && doc_y < (node->render_y + node->render_h)) {
            return node;
        }
    }

    for (int i = 0; i < node->child_count; i++) {
        DOMNode* found = get_clicked_link(node->children[i], doc_x, doc_y);
        if (found) return found;
    }

    return 0;
}
