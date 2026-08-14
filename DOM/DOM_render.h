#ifndef DOM_RENDER_H
#define DOM_RENDER_H

#include "DOM.h" /* Pro definici struktury DOMNode */

// --- Hlavní API pro parsování HTML ---
DOMNode* build_dom_tree(const char* html);

// --- Hlavní API pro layout a vykreslování ---
void render_dom_node(DOMNode* node, int start_x, int start_y, int* out_w, int* out_h);
void layout_dom_node(DOMNode* node, int start_x, int start_y, int avail_w, int* out_w, int* out_h);
void paint_dom_node(DOMNode* node);

// --- Pomocné funkce pro text a atributy ---
// (Poznámka: Pokud je voláš pouze z DOM_render.c, můžeš je odsud smazat 
// a v .c souboru je označit jako static)
int layout_text(DOMNode* node, int wrap_w);
void paint_text(DOMNode* node, int x, int y, int wrap_w);
int extract_attr(const char* tag_start, const char* tag_end, const char* attr_name, char* out, int out_size);
void extract_first_class(char* class_attr);
void append_text_node(DOMNode* parent, const char* text, int len);
void fill_node_attrs(DOMNode* node, const char* tag_name, const char* tag_start, const char* tag_end);

#endif // DOM_RENDER_H