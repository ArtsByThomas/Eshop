#ifndef DOM_H
#define DOM_H

#include <stdint.h>
#include "../css/css.h" // Pro strukturu ComputedStyle

#define MAX_NODES 512
#define MAX_CHILDREN 64

typedef enum {
    NODE_DIV,
    NODE_TEXT,
    NODE_IMG,
    NODE_P,
    NODE_H1,
    NODE_H2,
    NODE_H3,
    NODE_SPAN,
    NODE_BR,
    NODE_UL,
    NODE_LI,
    NODE_FORM,
    NODE_INPUT,
    NODE_BUTTON
} NodeType;

typedef struct DOMNode {
    NodeType type;
    char tag_name[16];     // "div", "p", "h1", ... 
    char class_name[64];   // obsah atributu class="..." (jen první třída, viz pozn. níže)
    char id_name[64];      // obsah atributu id="..."

    ComputedStyle style;    

    struct DOMNode* children[MAX_CHILDREN];
    int child_count;
    char text[128];
    int text_len;
    const uint8_t* img_data;

    int is_link;
    char href[128];

    // --- Formulářová pole ---
    // <form action="..." method="get|post">
    char form_action[128];
    int form_method; // 0 = GET , 1 = POST
    // <input type="..." name="..." value="...">
    char input_name[64];
    char input_value[128];
    char input_type[16]; // "text", "password", "submit", "hidden", "email", ...

    int is_submit_button;
    int is_checked; // 1 = zaškrtnuto, 0 = prázdné
    // Layout bounding box 
    int render_x;
    int render_y;
    int render_w;
    int render_h;
    int parent_avail_w;    // dostupná šířka pro zalomení 
    int cached_chars_per_line; // uloženo v layout_text, použito v paint_text 
} DOMNode;

extern DOMNode node_pool[MAX_NODES];
extern int nodes_allocated;
extern DOMNode* root_node;
extern DOMNode* hovered_node;
extern DOMNode* focused_input;

// Hlavní funkce
DOMNode* alloc_node(void);
DOMNode* get_clicked_link(DOMNode* node, int doc_x, int doc_y);
void compute_style_tree(DOMNode* n, ComputedStyle* parent_style);

#endif // DOM_H