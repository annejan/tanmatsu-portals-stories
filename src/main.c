// SPDX-License-Identifier: MIT
// Portals Stories: a launcher plugin that puts the story packs from
// scene.rs/stories on the card, where Portals reads them
// (/sd/portals/dlc/<pack>/), and keeps them up to date.
//
// The site lists the packs in STORIES_URL/index.txt, one a line, tab-separated:
//
//   <id> <sha256> <bytes> <files> <name> <about>
//
// and each pack's files in packs/<id>/files.txt, `<bytes> <path>` a line,
// the files themselves next to it: packs/<id>/<path>. A pack put on the card
// from here gets a STAMP file in its folder: the sha256 it came as, then the
// files it has, so an update knows what changed and what to remove.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "asp/display.h"
#include "asp/http.h"
#include "asp/input.h"
#include "pax_fonts.h"
#include "pax_gfx.h"
#include "pax_text.h"
#include "tanmatsu_plugin.h"

#ifndef STORIES_URL
#define STORIES_URL "https://scene.rs/stories/"
#endif
#ifndef DLC_DIR
#define DLC_DIR "/sd/portals/dlc"
#endif
#define STAMP      ".stories"
#define TAG        "portals-stories"
#define PACKS_MAX  16
#define FILES_MAX  96
#define INDEX_MAX  (16 * 1024)
#define LIST_MAX   (8 * 1024)
#define FILE_MAX   (1024 * 1024)

typedef enum {
    ST_NEW,       // not on the card
    ST_CARD,      // on the card, not put there from here: replace it?
    ST_UPDATE,    // put there from here, changed since
    ST_CURRENT,   // put there from here, as it is now
} status_t;

typedef struct {
    char     id[32];
    char     sha[65];
    char     name[48];
    char     about[160];
    int      bytes;
    int      files;
    status_t status;
} pack_t;

static pack_t    s_packs[PACKS_MAX];
static int       s_n;
static int       s_sel;
static char      s_line[96];  // what is going on, or what went wrong
static pax_buf_t s_buf;

// Drawing

#define C_BG     0xff101418
#define C_TEXT   0xffe8e8e8
#define C_DIM    0xff8a949e
#define C_ORANGE 0xffff9a1f
#define C_BLUE   0xff3fa9ff
#define C_SEL    0xff26303a

static char const* status_text(pack_t const* p) {
    switch (p->status) {
        case ST_NEW: return "new";
        case ST_CARD: return "on the card";
        case ST_UPDATE: return "update";
        case ST_CURRENT: return "up to date";
    }
    return "";
}

static void text(pax_col_t col, float size, float x, float y, char const* s) {
    pax_draw_text(&s_buf, col, pax_font_sky_mono, size, x, y, s);
}

// `s` wrapped to `cols` characters a line, at most `rows` lines.
static void wrapped(pax_col_t col, float size, float x, float y, int cols, int rows, char const* s) {
    char line[128];
    for (int r = 0; r < rows && *s; r++) {
        int n = (int)strlen(s);
        if (n > cols) {
            n = cols;
            while (n > 0 && s[n] != ' ') n--;
            if (n == 0) n = cols;
        }
        if (n >= (int)sizeof(line)) n = sizeof(line) - 1;
        memcpy(line, s, n);
        line[n] = '\0';
        text(col, size, x, y + r * (size + 4), line);
        s += n;
        while (*s == ' ') s++;
    }
}

static void draw(void) {
    float const w = pax_buf_get_width(&s_buf);
    float const h = pax_buf_get_height(&s_buf);
    pax_background(&s_buf, C_BG);
    text(C_ORANGE, 27, 24, 18, "Portals Stories");
    text(C_DIM, 18, 24, 52, "from scene.rs/stories, onto the card for Portals");

    float y = 96;
    for (int i = 0; i < s_n; i++) {
        pack_t const* p = &s_packs[i];
        if (i == s_sel) pax_simple_rect(&s_buf, C_SEL, 16, y - 6, w - 32, 34);
        char right[48];
        snprintf(right, sizeof(right), "%s  %d KB", status_text(p), (p->bytes + 1023) / 1024);
        text(i == s_sel ? C_TEXT : C_DIM, 18, 28, y, p->name);
        pax_right_text(&s_buf, p->status == ST_CURRENT ? C_DIM : C_BLUE, pax_font_sky_mono, 18, w - 28, y, right);
        y += 40;
    }
    float const cw = pax_text_size(pax_font_sky_mono, 18, "0").x;  // a monospace font
    if (s_n > 0 && cw > 0) wrapped(C_DIM, 18, 28, y + 8, (int)((w - 56) / cw), 3, s_packs[s_sel].about);

    text(C_ORANGE, 18, 24, h - 64, s_line);
    static char logged[sizeof(s_line)];
    if (strcmp(logged, s_line) != 0 && s_line[0]) asp_log_info(TAG, "%s", s_line);
    snprintf(logged, sizeof(logged), "%s", s_line);
    text(C_DIM, 18, 24, h - 34, "Enter: get it   A: get all new and updates   Esc: back");
    asp_disp_write_pax(&s_buf);
}

static void say(char const* s) {
    snprintf(s_line, sizeof(s_line), "%s", s);
    draw();
}

// Fetching

// GET `url` into a new buffer of at most `max` bytes: NULL if it failed (the
// reason in s_line) or was longer.
static char* fetch(char const* url, size_t max) {
    char* buf = malloc(max + 2);
    if (buf == NULL) {
        say("Out of memory.");
        return NULL;
    }
    int status = 0;
    if (asp_http_get(url, buf, max + 2, &status) != ASP_OK) {
        say("Could not reach scene.rs.");
        free(buf);
        return NULL;
    }
    if (status != 200 || strlen(buf) > max) {
        snprintf(s_line, sizeof(s_line), "scene.rs answered %d.", status);
        draw();
        free(buf);
        return NULL;
    }
    return buf;
}

// The next line of `*s`, cut off there: NULL past the end.
static char* next_line(char** s) {
    if (**s == '\0') return NULL;
    char* line = *s;
    char* end  = strchr(line, '\n');
    if (end) {
        *end = '\0';
        *s   = end + 1;
    } else {
        *s += strlen(line);
    }
    size_t n = strlen(line);
    if (n > 0 && line[n - 1] == '\r') line[n - 1] = '\0';
    return line;
}

// The next tab-separated field of `*s` into `out`.
static void field(char** s, char* out, size_t n) {
    char* tab = strchr(*s, '\t');
    size_t len = tab ? (size_t)(tab - *s) : strlen(*s);
    snprintf(out, n, "%.*s", (int)len, *s);
    *s += len + (tab ? 1 : 0);
}

// A path the site may name: letters, digits, '-', '_', '.', '/'; no "..",
// not absolute.
static bool safe_path(char const* p) {
    if (*p == '\0' || *p == '/' || strstr(p, "..")) return false;
    for (; *p; p++)
        if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '-' ||
              *p == '_' || *p == '.' || *p == '/'))
            return false;
    return true;
}

// The card

static bool exists(char const* path) {
    struct stat st;
    return stat(path, &st) == 0;
}

// The stamp's sha256 of pack `id` into `sha` ("" for none).
static void stamp_sha(char const* id, char* sha, size_t n) {
    char path[96];
    snprintf(path, sizeof(path), DLC_DIR "/%s/" STAMP, id);
    sha[0]  = '\0';
    FILE* f = fopen(path, "r");
    if (f == NULL) return;
    char line[80] = "";
    if (fread(line, 1, 64, f) == 64) snprintf(sha, n, "%.64s", line);
    fclose(f);
}

static void card_status(pack_t* p) {
    char path[96], sha[65];
    snprintf(path, sizeof(path), DLC_DIR "/%s/pack.txt", p->id);
    stamp_sha(p->id, sha, sizeof(sha));
    if (sha[0]) {
        p->status = strcmp(sha, p->sha) == 0 ? ST_CURRENT : ST_UPDATE;
    } else {
        p->status = exists(path) ? ST_CARD : ST_NEW;
    }
}

// Every folder on the way to `path` (a file's).
static void make_dirs(char const* path) {
    char dir[160];
    snprintf(dir, sizeof(dir), "%s", path);
    for (char* s = dir + 1; *s; s++)
        if (*s == '/') {
            *s = '\0';
            mkdir(dir, 0755);
            *s = '/';
        }
}

static bool write_file(char const* path, char const* data) {
    make_dirs(path);
    FILE* f = fopen(path, "w");
    if (f == NULL) return false;
    size_t const n  = strlen(data);
    bool const   ok = fwrite(data, 1, n, f) == n;
    return fclose(f) == 0 && ok;
}

// Files the last stamp lists that `now` (the new list) does not: removed.
static void remove_stale(char const* id, char paths[][96], int n) {
    char path[160];
    snprintf(path, sizeof(path), DLC_DIR "/%s/" STAMP, id);
    FILE* f = fopen(path, "r");
    if (f == NULL) return;
    char line[112];
    fgets(line, sizeof(line), f);  // the sha256
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (!safe_path(line)) continue;
        bool kept = false;
        for (int i = 0; i < n && !kept; i++) kept = strcmp(paths[i], line) == 0;
        if (!kept) {
            snprintf(path, sizeof(path), DLC_DIR "/%s/%s", id, line);
            remove(path);
        }
    }
    fclose(f);
}

// Pack `p` from the site onto the card: every file fetched first, so a
// failed fetch leaves the card as it was; then written, the files it no
// longer has removed, and stamped.
static bool get_pack(pack_t* p) {
    static char paths[FILES_MAX][96];
    static char* data[FILES_MAX];
    char         url[256];

    snprintf(s_line, sizeof(s_line), "%s: the list of files...", p->name);
    draw();
    snprintf(url, sizeof(url), STORIES_URL "packs/%s/files.txt", p->id);
    char* list = fetch(url, LIST_MAX);
    if (list == NULL) return false;

    int   n    = 0;
    bool  ok   = true;
    char* rest = list;
    char* line;
    while (ok && (line = next_line(&rest)) != NULL) {
        int  size = 0;
        char path[96];
        if (line[0] == '\0' || line[0] == '#') continue;
        if (sscanf(line, "%d\t%95s", &size, path) != 2 || !safe_path(path) || size < 0 || size > FILE_MAX ||
            n == FILES_MAX) {
            say("The list of files looks wrong.");
            ok = false;
            break;
        }
        snprintf(s_line, sizeof(s_line), "%s: %d of %d", p->name, n + 1, p->files);
        draw();
        snprintf(url, sizeof(url), STORIES_URL "packs/%s/%s", p->id, path);
        data[n] = fetch(url, (size_t)size);
        if (data[n] == NULL) {
            ok = false;
        } else if ((int)strlen(data[n]) != size) {
            say("A file came in the wrong size.");
            free(data[n]);
            ok = false;
        } else {
            snprintf(paths[n], sizeof(paths[n]), "%s", path);
            n++;
        }
    }
    free(list);

    if (ok) {
        say("Writing to the card...");
        char path[160];
        for (int i = 0; i < n && ok; i++) {
            snprintf(path, sizeof(path), DLC_DIR "/%s/%.95s", p->id, paths[i]);
            ok = write_file(path, data[i]);
        }
        if (!ok) say("Could not write to the card. Is there one in?");
    }
    if (ok) {
        remove_stale(p->id, paths, n);
        char path[96];
        snprintf(path, sizeof(path), DLC_DIR "/%s/" STAMP, p->id);
        FILE* f = fopen(path, "w");
        if (f) {
            fprintf(f, "%s\n", p->sha);
            for (int i = 0; i < n; i++) fprintf(f, "%s\n", paths[i]);
            fclose(f);
        }
        p->status = ST_CURRENT;
        snprintf(s_line, sizeof(s_line), "%s is on the card. Portals: Stories.", p->name);
    }
    for (int i = 0; i < n; i++) free(data[i]);
    draw();
    return ok;
}

static bool load_index(void) {
    say("Asking scene.rs what there is...");
    char* index = fetch(STORIES_URL "index.txt", INDEX_MAX);
    if (index == NULL) return false;
    s_n        = 0;
    char* rest = index;
    char* line;
    while ((line = next_line(&rest)) != NULL && s_n < PACKS_MAX) {
        if (line[0] == '\0' || line[0] == '#') continue;
        pack_t* p = &s_packs[s_n];
        char    num[16];
        field(&line, p->id, sizeof(p->id));
        field(&line, p->sha, sizeof(p->sha));
        field(&line, num, sizeof(num));
        p->bytes = 0;
        sscanf(num, "%d", &p->bytes);
        field(&line, num, sizeof(num));
        p->files = 0;
        sscanf(num, "%d", &p->files);
        field(&line, p->name, sizeof(p->name));
        field(&line, p->about, sizeof(p->about));
        if (!safe_path(p->id) || strchr(p->id, '/') || strlen(p->sha) != 64) continue;
        card_status(p);
        asp_log_info(TAG, "%s: %s", p->id, status_text(p));
        s_n++;
    }
    free(index);
    snprintf(s_line, sizeof(s_line), "%s", s_n ? "" : "scene.rs has no stories for us.");
    return s_n > 0;
}

// The plugin

static plugin_info_t const info = {
    .name        = "Portals Stories",
    .slug        = "com.annejan.portals-stories",
    .version     = "0.1.0",
    .author      = "Anne Jan Brouwer",
    .description = "Story packs for Portals from scene.rs",
    .api_version = TANMATSU_PLUGIN_API_VERSION,
    .type        = PLUGIN_TYPE_MENU,
    .flags       = 0,
};

static plugin_info_t const* get_info(void) {
    return &info;
}

static int init(plugin_context_t* ctx) {
    return 0;
}

static void cleanup(plugin_context_t* ctx) {
}

// The list from the site, if there is a network.
static void refresh(void) {
    bool connected = false;
    asp_net_is_connected(&connected);
    if (!connected) {
        say("No WiFi. Connect in the launcher's WiFi settings, then Enter.");
        return;
    }
    load_index();
    draw();
}

static bool run(plugin_context_t* ctx) {
    if (asp_disp_init_pax_buf(&s_buf) != ASP_OK) {
        asp_log_error(TAG, "no display buffer");
        return false;
    }
    s_sel = 0;
    s_n   = 0;

    refresh();

    for (;;) {
        asp_input_event_t e;
        if (!asp_input_poll(&e, 1000)) continue;
        if (e.type != ASP_INPUT_EVENT_TYPE_NAVIGATION && e.type != ASP_INPUT_EVENT_TYPE_KEYBOARD) continue;
        if (e.type == ASP_INPUT_EVENT_TYPE_KEYBOARD) {
            if ((e.args_keyboard.ascii == 'a' || e.args_keyboard.ascii == 'A') && s_n > 0) {
                int got = 0;
                for (int i = 0; i < s_n; i++)
                    if ((s_packs[i].status == ST_NEW || s_packs[i].status == ST_UPDATE) && get_pack(&s_packs[i]))
                        got++;
                snprintf(s_line, sizeof(s_line), got ? "%d on the card. Portals: Stories." : "Nothing new.", got);
                draw();
            }
            continue;
        }
        if (!e.args_navigation.state) continue;
        switch (e.args_navigation.key) {
            case ASP_INPUT_NAVIGATION_KEY_ESC:
            case ASP_INPUT_NAVIGATION_KEY_F1:
            case ASP_INPUT_NAVIGATION_KEY_GAMEPAD_B:
                pax_buf_destroy(&s_buf);
                return false;
            case ASP_INPUT_NAVIGATION_KEY_UP:
                if (s_sel > 0) s_sel--;
                s_line[0] = '\0';
                draw();
                break;
            case ASP_INPUT_NAVIGATION_KEY_DOWN:
                if (s_sel < s_n - 1) s_sel++;
                s_line[0] = '\0';
                draw();
                break;
            case ASP_INPUT_NAVIGATION_KEY_RETURN:
            case ASP_INPUT_NAVIGATION_KEY_GAMEPAD_A:
                if (s_n > 0) {
                    get_pack(&s_packs[s_sel]);
                } else {
                    refresh();  // nothing listed yet: try again
                }
                break;
            default:
                break;
        }
    }
}

static plugin_entry_t const entry = {
    .get_info    = get_info,
    .init        = init,
    .cleanup     = cleanup,
    .menu_select = run,
};

TANMATSU_PLUGIN_REGISTER(entry);
