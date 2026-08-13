#include "css.h"
#include "../DOM/DOM.h"
#include <cstddef>
#include <stddef.h>
extern int str_starts_with(const char* str, const char* prefix);
extern int str_len(const char* s);
extern int str_copy(char* dst, const char* src, int max_len);
extern int skip_spaces(const char** p);
extern int parse_int(const char** p);
extern int is_alpha_c(char c);
extern int is_digit_c(char c);
extern int hex_digit_val(char c);
extern int str_eq(const char* a, const char* b);

uint32_t parse_css_color(const char** p) {
    skip_spaces(p);
    const char* s = *p;
    if (*s == '#') {
        s++;
        int hexlen = 0;
        while (hex_digit_val(s[hexlen]) >= 0) hexlen++;
        uint32_t r = 0, g = 0, b = 0;
        if (hexlen == 6) {
            r = hex_digit_val(s[0]) * 16 + hex_digit_val(s[1]);
            g = hex_digit_val(s[2]) * 16 + hex_digit_val(s[3]);
            b = hex_digit_val(s[4]) * 16 + hex_digit_val(s[5]);
        } else if (hexlen == 3) {
            r = hex_digit_val(s[0]) * 17;
            g = hex_digit_val(s[1]) * 17;
            b = hex_digit_val(s[2]) * 17;
        }
        *p = s + hexlen;
        return 0xFF000000 | (r << 16) | (g << 8) | b;
    }
    char name[32]; int n = 0;
    while (is_alpha_c(*s) && n < 31) { name[n++] = *s; s++; }
    name[n] = '\0';
    *p = s;
    for (unsigned i = 0; i < NAMED_COLOR_COUNT; i++) {
        if (str_eq(name, named_colors[i].name)) return named_colors[i].value;
    }
    return 0xFF000000; 
}

int parse_css_length(const char** p) {
    skip_spaces(p);
    int val = parse_int(p);
    if (str_starts_with(*p, "px")) *p += 2;
    return val;
}
CssPropId get_css_prop_id(const char *prop_name) {
    for (int i = 0; prop_map[i].name != NULL; i++) {
        if (str_eq(prop_name, prop_map[i].name)) {
            return prop_map[i].id;
        }
    }
    return CSS_PROP_UNKNOWN;
}
void parse_css_declaration(const char** p, ComputedStyle* style) {
    skip_spaces(p);
    char prop[32]; int n = 0;
    while (is_alpha_c(**p) && n < 31) { prop[n++] = **p; (*p)++; }
    prop[n] = '\0';
    skip_spaces(p);
    if (**p == ':') (*p)++;
    skip_spaces(p);
// 1. Zjistíme, o jakou vlastnost jde
CssPropId id = get_css_prop_id(prop);

// 2. Provedeme logiku přes switch
switch (id) {
    case CSS_PROP_COLOR:
        style->has_color = 1;
        style->text_color = parse_css_color(p);
        break;

    case CSS_PROP_BG_COLOR:
        style->has_bg = 1;
        style->bg_color = parse_css_color(p);
        break;

    case CSS_PROP_WIDTH:
        style->has_width = 1;
        style->width = parse_css_length(p);
        break;

    case CSS_PROP_HEIGHT:
        style->has_height = 1;
        style->height = parse_css_length(p);
        break;

    case CSS_PROP_MARGIN:
        style->has_margin = 1;
        style->margin = parse_css_length(p);
        break;

    case CSS_PROP_PADDING:
        style->has_padding = 1;
        style->padding = parse_css_length(p);
        break;

    case CSS_PROP_DISPLAY:
        style->has_display = 1;
        skip_spaces(p);
        if (str_starts_with(*p, "inline")) { 
            style->is_inline = 1; 
            *p += sizeof("inline") - 1; 
        } else if (str_starts_with(*p, "block")) { 
            style->is_inline = 0; 
            *p += sizeof("block") - 1; 
        } else if (str_starts_with(*p, "none")) { 
            style->is_inline = -1; 
            *p += sizeof("none") - 1; 
        }
        break;

    case CSS_PROP_FONT_SIZE:
        style->has_font_size = 1;
        int px = parse_css_length(p);
        int scale = px / 8;
        // Elegantní "clamp" oříznutí hodnoty mezi 1 a 6
        style->font_scale = (scale < 1) ? 1 : ((scale > 6) ? 6 : scale);
        break;

    case CSS_PROP_FLOAT:
        style->has_float = 1;
        skip_spaces(p);
        if (str_starts_with(*p, "left")) { 
            style->float_side = 1; 
            *p += sizeof("left") - 1; 
        } else if (str_starts_with(*p, "right")) { 
            style->float_side = 2; 
            *p += sizeof("right") - 1; 
        } else { 
            style->float_side = 0; 
            while (is_alpha_c(**p)) (*p)++; 
        } 
        break;

    case CSS_PROP_TEXT_ALIGN:
        style->has_text_align = 1;
        skip_spaces(p);
        if (str_starts_with(*p, "center")) { 
            style->text_align = 1; 
            *p += sizeof("center") - 1; 
        } else if (str_starts_with(*p, "right")) { 
            style->text_align = 2; 
            *p += sizeof("right") - 1; 
        } else { 
            style->text_align = 0; 
            while (is_alpha_c(**p)) (*p)++; 
        }
        break;

    case CSS_PROP_BORDER:
        style->has_border = 1;
        style->border_w = parse_css_length(p);
        skip_spaces(p);
        while (is_alpha_c(**p)) (*p)++;  
        skip_spaces(p);
        style->border_color = parse_css_color(p);
        break;

    case CSS_PROP_BORDER_RADIUS: 
        style->has_border_radius = 1;
        style->border_radius = parse_css_length(p);
        break;

    case CSS_PROP_UNKNOWN:
    default:
        // Neznámá vlastnost - přeskočíme zbytek řádku
        while (**p && **p != ';' && **p != '}') (*p)++;
        break;
}

    skip_spaces(p);
    if (**p == ';') (*p)++;
}


void parse_css_block(const char* content) {
    const char* p = content;
    while (*p && css_rule_count < MAX_CSS_RULES) {
        skip_spaces(&p);
        if (!*p) break;
        if (str_starts_with(p, "</style")) break;

        int sel_type;
        char sel_name[64]; int n = 0;
        if (*p == '.') { sel_type = SEL_CLASS; p++; }
        else if (*p == '#') { sel_type = SEL_ID; p++; }
        else { sel_type = SEL_TAG; }

        while ((is_alpha_c(*p) || is_digit_c(*p) || *p == ':' || *p == '-') && n < 63) { 
            sel_name[n++] = *p; 
            p++; 
        }
        sel_name[n] = '\0';
        
        if (n == 0) {
            while (*p && *p != '{') p++;
            if (!*p) break;
        }

        skip_spaces(&p);
        if (*p != '{') {
            while (*p && *p != '{' && !str_starts_with(p, "</style")) p++;
            if (*p != '{') continue;
        }
        p++; 

        CSSRule* rule = &css_rules[css_rule_count];
        rule->sel_type = sel_type;
        rule->order = css_rule_count;
        
        
        rule->is_hover = 0;
        int sel_len = str_len(sel_name);
        if (sel_len > 6 && str_eq(&sel_name[sel_len - 6], ":hover")) {
            rule->is_hover = 1;
            sel_name[sel_len - 6] = '\0'; 
        }

        str_copy(rule->sel_name, sel_name, sizeof(rule->sel_name));
        
        ComputedStyle blank; 
        for (int i = 0; i < (int)sizeof(ComputedStyle); i++) ((char*)&blank)[i] = 0;
        rule->decl = blank;

        skip_spaces(&p);
        while (*p && *p != '}') {
            parse_css_declaration(&p, &rule->decl);
            skip_spaces(&p);
        }
        if (*p == '}') p++;

        css_rule_count++;
    }
}
