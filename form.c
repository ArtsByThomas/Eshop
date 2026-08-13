#include "DOM.h"
#include "browser.h"
#include <stdint.h>


extern int str_eq(const char* a, const char* b), parse_url(const char* url, char* out_domain, char* out_path, int* out_is_https, int* out_port, int* out_is_ip), sys_https_post(uint32_t ip, const char *domain, uint16_t port, const char *path,
    const char *body, int body_len, char *buf, int max_len),sys_http_post(uint32_t ip, uint16_t port, const char *path,
        const char *body, int body_len, char *buf, int max_len);
extern void navigate_to(const char* url),url_encode(const char* src, char* out, int out_size), resolve_relative_url(const char* href, char* out, int out_size), debug_print(const char* text), process_navigation_response(const char* full_url, const char* path, int bytes) ;
extern char* current_domain;
extern int current_is_https, current_port;
extern uint32_t current_ip,current_server_ip, parse_ipv4(const char* s), sys_dns_resolve(const char *domain);
int collect_form_inputs(DOMNode* node, DOMNode* form, DOMNode** out, int max_count, int count) {
    if (!node || count >= max_count) return count;
    if (node != form && node->type == NODE_FORM) return count; // nezacházet do vnořeného formuláře
    if (node->type == NODE_INPUT) {
        out[count++] = node;
    }
    for (int i = 0; i < node->child_count && count < max_count; i++) {
        count = collect_form_inputs(node->children[i], form, out, max_count, count);
    }
    return count;
}

void submit_form(DOMNode* form) {
    if (!form || form->type != NODE_FORM) return;

    DOMNode* inputs[64];
    int input_count = collect_form_inputs(form, form, inputs, 64, 0);

    
    char form_data[1024];
    int pos = 0;
    for (int i = 0; i < input_count; i++) {
        DOMNode* inp = inputs[i];
        // "submit"/"button"/"reset" typy se NEPOSÍLAJÍ jako form data 
        if (str_eq(inp->input_type, "submit") || str_eq(inp->input_type, "button") ||
            str_eq(inp->input_type, "reset")) continue;
        if (!inp->input_name[0]) continue; // bez name atributu se input neposílá 

        if (pos > 0 && pos < (int)sizeof(form_data) - 1) form_data[pos++] = '&';

        char enc_name[128];
        url_encode(inp->input_name, enc_name, sizeof(enc_name));
        for (int j = 0; enc_name[j] && pos < (int)sizeof(form_data) - 1; j++) form_data[pos++] = enc_name[j];

        if (pos < (int)sizeof(form_data) - 1) form_data[pos++] = '=';

        char enc_value[384];
        url_encode(inp->input_value, enc_value, sizeof(enc_value));
        for (int j = 0; enc_value[j] && pos < (int)sizeof(form_data) - 1; j++) form_data[pos++] = enc_value[j];
    }
    form_data[pos] = '\0';

    // form_action rozbalíme stejně jako <a href> 
    char full_action_url[256];
    resolve_relative_url(form->form_action, full_action_url, sizeof(full_action_url));

    if (form->form_method == 0) {
        // --- GET: query string se připojí k URL, žádné tělo requestu ---
        char get_url[512];
        int gp = 0;
        for (int i = 0; full_action_url[i] && gp < (int)sizeof(get_url) - 1; i++) get_url[gp++] = full_action_url[i];
        if (pos > 0 && gp < (int)sizeof(get_url) - 1) get_url[gp++] = '?';
        for (int i = 0; form_data[i] && gp < (int)sizeof(get_url) - 1; i++) get_url[gp++] = form_data[i];
        get_url[gp] = '\0';

        // GET formulář je koncepčně stejná operace jako klik na <a href>
        // s URL obsahující query string 
        navigate_to(get_url);
        return;
    }

    // --- POST: form_data se pošle jako tělo requestu, URL beze změny ---
    char domain[128], path[128];
    int is_https = 1, explicit_port = 0, is_ip = 0;
    int is_absolute = parse_url(full_action_url, domain, path, &is_https, &explicit_port, &is_ip);

    uint32_t target_ip = current_server_ip;
    if (is_absolute) {
        str_copy(current_domain, domain, sizeof(current_domain));
        current_is_https = is_https;
        current_port = explicit_port;
        if (is_ip) {
            target_ip = parse_ipv4(domain);
        } else {
            uint32_t resolved = sys_dns_resolve(domain);
            if (resolved != 0) target_ip = resolved;
        }
        current_server_ip = target_ip;
    }

    int port = explicit_port ? explicit_port : (is_https ? 443 : 80);

    debug_print(is_https ? "--- Zahajuji HTTPS POST (formular) ---\n" : "--- Zahajuji HTTP POST (formular) ---\n");
    debug_print("Cesta (path): ");
    debug_print(path);
    debug_print("\nForm data: ");
    debug_print(form_data);
    debug_print("\n");

    for (int i = 0; i < HTML_BUF_SIZE; i++) html_buffer[i] = 0;

    int bytes = is_https
        ? sys_https_post(target_ip, domain, (uint16_t)port, path, form_data, pos, html_buffer, HTML_BUF_SIZE)
        : sys_http_post(target_ip, (uint16_t)port, path, form_data, pos, html_buffer, HTML_BUF_SIZE);

    process_navigation_response(full_action_url, path, bytes);
}