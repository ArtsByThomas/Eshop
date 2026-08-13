#include <stdint.h>

typedef struct {
    int has_width;      int width;
    int has_height;     int height;
    int has_bg;          uint32_t bg_color;
    int has_color;       uint32_t text_color;
    int has_margin;      int margin;
    int has_padding;     int padding;
    int has_display;     int is_inline;       
    int has_font_size;   int font_scale;      
    int has_text_align;  int text_align;       
    int has_border;       int border_w; uint32_t border_color;
    int has_border_radius; int border_radius;
    
    int has_float;       
    int float_side;      
} ComputedStyle;

#define SEL_TAG   0
#define SEL_CLASS 1
#define SEL_ID    2

typedef struct {
    int sel_type;
    char sel_name[64];
    int is_hover; 
    int order;
    ComputedStyle decl;
} CSSRule;

#define MAX_CSS_RULES 128
CSSRule css_rules[MAX_CSS_RULES];
int css_rule_count = 0;

typedef struct { const char* name; uint32_t value; } NamedColor;
 static const NamedColor named_colors[] = {
    {"black",   0xFF000000}, {"white",   0xFFFFFFFF}, {"red",     0xFFFF0000},
    {"green",   0xFF008000}, {"blue",    0xFF0000FF}, {"yellow",  0xFFFFFF00},
    {"gray",    0xFF808080}, {"grey",    0xFF808080}, {"silver",  0xFFC0C0C0},
    {"orange",  0xFFFFA500}, {"purple",  0xFF800080}, {"pink",    0xFFFFC0CB},
    {"brown",   0xFFA52A2A}, {"cyan",    0xFF00FFFF}, {"magenta", 0xFFFF00FF},
    {"navy",    0xFF000080}, {"teal",    0xFF008080}, {"lime",    0xFF00FF00},
    {"maroon",  0xFF800000}, {"olive",   0xFF808000}, {"transparent", 0x00000000},
};
extern const NamedColor named_colors[];
extern const unsigned int NAMED_COLOR_COUNT;
#define NAMED_COLOR_COUNT (sizeof(named_colors)/sizeof(named_colors[0]))
