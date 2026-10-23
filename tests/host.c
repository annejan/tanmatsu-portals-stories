// Host test: the plugin's fetching and card writing, with the badge's APIs
// shimmed. HTTP reads files under SITE (a stories_site.py output), the card
// is a folder (DLC_DIR), and the keys come from argv.
//
//   make test

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "asp/display.h"
#include "asp/http.h"
#include "asp/input.h"
#include "pax_gfx.h"
#include "pax_text.h"
#include "tanmatsu_plugin.h"

static char const* s_keys;  // what is pressed, in order: d(own) u(p) e(nter) a x(esc)

pax_2vec2f pax_draw_text_adv(pax_buf_t* buf, pax_col_t c, pax_font_t const* f, float size, float x, float y,
                             char const* t, size_t len, pax_align_t h, pax_align_t v, ptrdiff_t cur) {
    return (pax_2vec2f){0};
}
void pax_background(pax_buf_t* b, pax_col_t c) {}
void pax_simple_rect(pax_buf_t* b, pax_col_t c, float x, float y, float w, float h) {}
int  pax_buf_get_width(pax_buf_t const* b) { return 800; }
int  pax_buf_get_height(pax_buf_t const* b) { return 480; }
void pax_buf_destroy(pax_buf_t* b) {}
pax_font_t const pax_font_sky_mono_raw;
asp_err_t asp_disp_init_pax_buf(pax_buf_t* b) { return ASP_OK; }
void asp_log_error(char const* tag, char const* fmt, ...) {}
void asp_log_info(char const* tag, char const* fmt, ...) {}

extern char const* host_line(void);
asp_err_t asp_disp_write_pax(pax_buf_t* b) {
    static char last[128];
    if (strcmp(last, host_line()) != 0 && host_line()[0]) printf("  | %s\n", host_line());
    snprintf(last, sizeof(last), "%s", host_line());
    return ASP_OK;
}

asp_err_t asp_net_is_connected(bool* out) {
    *out = true;
    return ASP_OK;
}

asp_err_t asp_http_get(char const* url, char* response, size_t max_len, int* status) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", SITE, url + strlen(STORIES_URL));
    FILE* f = fopen(path, "rb");
    if (f == NULL) {
        *status = 404;
        response[0] = '\0';
        return ASP_OK;
    }
    size_t n = fread(response, 1, max_len - 1, f);
    response[n] = '\0';
    fclose(f);
    *status = 200;
    return ASP_OK;
}

bool asp_input_poll(asp_input_event_t* e, int64_t ms) {
    memset(e, 0, sizeof(*e));
    char const k = *s_keys ? *s_keys++ : 'x';
    if (k == 'a') {
        e->type               = ASP_INPUT_EVENT_TYPE_KEYBOARD;
        e->args_keyboard.ascii = 'a';
        return true;
    }
    e->type                    = ASP_INPUT_EVENT_TYPE_NAVIGATION;
    e->args_navigation.state   = true;
    e->args_navigation.key     = k == 'd'   ? ASP_INPUT_NAVIGATION_KEY_DOWN
                                 : k == 'u' ? ASP_INPUT_NAVIGATION_KEY_UP
                                 : k == 'e' ? ASP_INPUT_NAVIGATION_KEY_RETURN
                                            : ASP_INPUT_NAVIGATION_KEY_ESC;
    return true;
}

#include "../src/main.c"

char const* host_line(void) {
    return s_line;
}

int main(int argc, char** argv) {
    s_keys = argc > 1 ? argv[1] : "";
    bool again = entry.menu_select(NULL);
    for (int i = 0; i < s_n; i++) printf("%s: %s\n", s_packs[i].id, status_text(&s_packs[i]));
    return again ? 1 : 0;
}
