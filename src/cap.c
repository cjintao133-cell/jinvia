/* SPDX-License-Identifier: Apache-2.0 */
#include "jinvia/cap.h"
#include <string.h>
#include <ctype.h>

static char *ltrim(char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    return s;
}
static void rtrim(char *s) {
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) s[--n] = '\0';
}
static void copy_trunc(char *dst, size_t cap, const char *src) {
    size_t i = 0;
    for (; src[i] && i + 1 < cap; i++) dst[i] = src[i];
    dst[i] = '\0';
}

int sr_cap_parse(const char *text, SrCap *out) {
    if (!text || !out) return -1;
    memset(out, 0, sizeof(*out));

    char buf[1024];
    const char *p = text;
    while (*p) {
        size_t li = 0;
        while (*p && *p != '\n' && li + 1 < sizeof(buf)) buf[li++] = *p++;
        buf[li] = '\0';
        if (*p == '\n') p++;

        char *line = ltrim(buf);
        rtrim(line);
        if (*line == '\0' || *line == '#') continue;

        char *colon = strchr(line, ':');
        if (!colon) continue;
        *colon = '\0';
        char *key = line;
        rtrim(key);
        char *val = ltrim(colon + 1);
        rtrim(val);

        if (strcmp(key, "name") == 0) {
            copy_trunc(out->name, SR_NAME_MAX, val);
        } else if (strcmp(key, "does") == 0) {
            copy_trunc(out->does, SR_DOES_MAX, val);
        } else if (strcmp(key, "entry") == 0) {
            copy_trunc(out->entry, SR_ENTRY_MAX, val);
        } else if (strcmp(key, "needs") == 0) {
            char *s = val;
            while (*s && out->needs_count < SR_NEEDS_MAX) {
                while (*s == ',' || isspace((unsigned char)*s)) s++;
                if (!*s) break;
                char *start = s;
                while (*s && *s != ',') s++;
                if (*s == ',') { *s = '\0'; s++; }
                char *item = ltrim(start);
                rtrim(item);
                if (*item) {
                    copy_trunc(out->needs[out->needs_count], SR_NEED_LEN, item);
                    out->needs_count++;
                }
            }
        }
        /* 未知 key 忽略——向前兼容 */
    }
    if (out->name[0] == '\0') return -1;
    return 0;
}

int sr_cap_needs_satisfied(const SrCap *cap, const char *const *grants, int ngrants) {
    if (!cap || !grants || ngrants < 0) return 0;
    for (int i = 0; i < cap->needs_count; i++) {
        int ok = 0;
        for (int j = 0; j < ngrants; j++) {
            if (grants[j] && strcmp(cap->needs[i], grants[j]) == 0) { ok = 1; break; }
        }
        if (!ok) return 0;   /* 有未授权项 -> 默认拒绝 */
    }
    return 1;
}
