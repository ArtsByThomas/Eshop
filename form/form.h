#ifndef FORM_H
#define FORM_H

#include "../DOM/DOM.h"

void submit_form(DOMNode* form);

// Pokud se rozhodneš funkci collect_form_inputs ponechat jako veřejnou, 
// může zůstat zde. Jinak ji z tohoto souboru smaž a v form.c přidej 'static'.
int collect_form_inputs(DOMNode* node, DOMNode* form, DOMNode** out, int max_count, int count);

#endif // FORM_H