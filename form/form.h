#ifndef FORM_H
#define FORM_H

#include "../DOM/DOM.h"

void submit_form(DOMNode* form);


int collect_form_inputs(DOMNode* node, DOMNode* form, DOMNode** out, int max_count, int count);

#endif // FORM_H