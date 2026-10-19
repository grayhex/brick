#include "mini_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *p;
    char *err;
    size_t err_cap;
    int failed;
} parser_t;

static const jv_t JV_NULL_NODE = {JV_NULL, 0.0, NULL, 0, NULL, NULL};

static void fail(parser_t *ps, const char *msg)
{
    if (!ps->failed && ps->err_cap > 0) {
        snprintf(ps->err, ps->err_cap, "JSON error: %s near \"%.20s\"", msg, ps->p);
    }
    ps->failed = 1;
}

static void skip_ws(parser_t *ps)
{
    while (*ps->p == ' ' || *ps->p == '\n' || *ps->p == '\r' || *ps->p == '\t') {
        ps->p++;
    }
}

static void parse_value(parser_t *ps, jv_t *out);

static char *parse_string_raw(parser_t *ps, size_t *len_out)
{
    if (*ps->p != '"') {
        fail(ps, "expected string");
        return NULL;
    }
    ps->p++;
    size_t cap = 16;
    size_t len = 0;
    char *buf = (char *)malloc(cap);
    while (*ps->p != '"') {
        if (*ps->p == '\0') {
            fail(ps, "unterminated string");
            free(buf);
            return NULL;
        }
        char c = *ps->p++;
        if (c == '\\') {
            char e = *ps->p++;
            switch (e) {
            case 'n':
                c = '\n';
                break;
            case 't':
                c = '\t';
                break;
            case 'r':
                c = '\r';
                break;
            case 'b':
                c = '\b';
                break;
            case 'f':
                c = '\f';
                break;
            case 'u': {
                unsigned v = 0;
                for (int i = 0; i < 4; i++) {
                    char h = *ps->p++;
                    v = v * 16 + (unsigned)(h <= '9' ? h - '0' : (h | 0x20) - 'a' + 10);
                }
                c = v < 128 ? (char)v : '?';
                break;
            }
            default:
                c = e;
                break; /* \" \\ \/ */
            }
        }
        if (len + 1 >= cap) {
            cap *= 2;
            buf = (char *)realloc(buf, cap);
        }
        buf[len++] = c;
    }
    ps->p++;
    buf[len] = '\0';
    if (len_out != NULL) {
        *len_out = len;
    }
    return buf;
}

static void parse_value(parser_t *ps, jv_t *out)
{
    memset(out, 0, sizeof *out);
    skip_ws(ps);
    char c = *ps->p;
    if (c == '{') {
        ps->p++;
        out->type = JV_OBJ;
        size_t cap = 0;
        skip_ws(ps);
        if (*ps->p == '}') {
            ps->p++;
            return;
        }
        for (;;) {
            skip_ws(ps);
            size_t klen = 0;
            char *key = parse_string_raw(ps, &klen);
            if (key == NULL) {
                return;
            }
            skip_ws(ps);
            if (*ps->p++ != ':') {
                fail(ps, "expected ':'");
                free(key);
                return;
            }
            if (out->len == cap) {
                cap = cap ? cap * 2 : 8;
                out->kids = (jv_t *)realloc(out->kids, cap * sizeof(jv_t));
                out->keys = (char **)realloc(out->keys, cap * sizeof(char *));
            }
            out->keys[out->len] = key;
            parse_value(ps, &out->kids[out->len]);
            out->len++;
            if (ps->failed) {
                return;
            }
            skip_ws(ps);
            if (*ps->p == ',') {
                ps->p++;
                continue;
            }
            if (*ps->p == '}') {
                ps->p++;
                return;
            }
            fail(ps, "expected ',' or '}'");
            return;
        }
    } else if (c == '[') {
        ps->p++;
        out->type = JV_ARR;
        size_t cap = 0;
        skip_ws(ps);
        if (*ps->p == ']') {
            ps->p++;
            return;
        }
        for (;;) {
            if (out->len == cap) {
                cap = cap ? cap * 2 : 8;
                out->kids = (jv_t *)realloc(out->kids, cap * sizeof(jv_t));
            }
            parse_value(ps, &out->kids[out->len]);
            out->len++;
            if (ps->failed) {
                return;
            }
            skip_ws(ps);
            if (*ps->p == ',') {
                ps->p++;
                continue;
            }
            if (*ps->p == ']') {
                ps->p++;
                return;
            }
            fail(ps, "expected ',' or ']'");
            return;
        }
    } else if (c == '"') {
        out->type = JV_STR;
        out->str = parse_string_raw(ps, &out->len);
    } else if (strncmp(ps->p, "true", 4) == 0) {
        out->type = JV_BOOL;
        out->num = 1;
        ps->p += 4;
    } else if (strncmp(ps->p, "false", 5) == 0) {
        out->type = JV_BOOL;
        ps->p += 5;
    } else if (strncmp(ps->p, "null", 4) == 0) {
        out->type = JV_NULL;
        ps->p += 4;
    } else if (c == '-' || (c >= '0' && c <= '9')) {
        char *end = NULL;
        out->type = JV_NUM;
        out->num = strtod(ps->p, &end);
        if (end == ps->p) {
            fail(ps, "bad number");
        }
        ps->p = end;
    } else {
        fail(ps, "unexpected character");
    }
}

jv_t *jv_parse(const char *text, char *err, size_t err_cap)
{
    parser_t ps = {text, err, err_cap, 0};
    jv_t *root = (jv_t *)malloc(sizeof *root);
    parse_value(&ps, root);
    if (!ps.failed) {
        skip_ws(&ps);
        if (*ps.p != '\0') {
            fail(&ps, "trailing characters");
        }
    }
    if (ps.failed) {
        jv_free(root);
        return NULL;
    }
    return root;
}

static void free_members(jv_t *v)
{
    for (size_t i = 0; i < v->len && (v->type == JV_ARR || v->type == JV_OBJ); i++) {
        free_members(&v->kids[i]);
        if (v->type == JV_OBJ) {
            free(v->keys[i]);
        }
    }
    free(v->kids);
    free(v->keys);
    if (v->type == JV_STR) {
        free(v->str);
    }
}

void jv_free(jv_t *root)
{
    if (root == NULL) {
        return;
    }
    free_members(root);
    free(root);
}

const jv_t *jv_get(const jv_t *obj, const char *key)
{
    if (obj != NULL && obj->type == JV_OBJ) {
        for (size_t i = 0; i < obj->len; i++) {
            if (strcmp(obj->keys[i], key) == 0) {
                return &obj->kids[i];
            }
        }
    }
    return &JV_NULL_NODE;
}

const jv_t *jv_at(const jv_t *arr, size_t index)
{
    if (arr != NULL && arr->type == JV_ARR && index < arr->len) {
        return &arr->kids[index];
    }
    return &JV_NULL_NODE;
}

double jv_num(const jv_t *v)
{
    return (v != NULL && (v->type == JV_NUM || v->type == JV_BOOL)) ? v->num : 0.0;
}

const char *jv_str(const jv_t *v)
{
    return (v != NULL && v->type == JV_STR && v->str != NULL) ? v->str : "";
}

char *jv_read_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = (char *)malloc((size_t)n + 1);
    size_t got = fread(buf, 1, (size_t)n, f);
    fclose(f);
    buf[got] = '\0';
    return buf;
}
