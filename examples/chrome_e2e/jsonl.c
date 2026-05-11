#include "jsonl.h"

#include <string.h>

static void json_string(FILE *file, const char *value)
{
    const unsigned char *p;

    fputc('"', file);
    if (value != 0) {
        p = (const unsigned char *)value;
        while (*p != 0) {
            switch (*p) {
            case '"':
                fputs("\\\"", file);
                break;
            case '\\':
                fputs("\\\\", file);
                break;
            case '\b':
                fputs("\\b", file);
                break;
            case '\f':
                fputs("\\f", file);
                break;
            case '\n':
                fputs("\\n", file);
                break;
            case '\r':
                fputs("\\r", file);
                break;
            case '\t':
                fputs("\\t", file);
                break;
            default:
                if (*p < 0x20u) {
                    fprintf(file, "\\u%04x", (unsigned int)*p);
                } else {
                    fputc((int)*p, file);
                }
                break;
            }
            ++p;
        }
    }
    fputc('"', file);
}

int rtc_e2e_jsonl_open(rtc_e2e_jsonl_t *jsonl, const char *path)
{
    if (jsonl == 0 || path == 0) {
        return -1;
    }
    jsonl->file = fopen(path, "w");
    return jsonl->file == 0 ? -1 : 0;
}

void rtc_e2e_jsonl_close(rtc_e2e_jsonl_t *jsonl)
{
    if (jsonl != 0 && jsonl->file != 0) {
        fclose(jsonl->file);
        jsonl->file = 0;
    }
}

void rtc_e2e_jsonl_event(rtc_e2e_jsonl_t *jsonl, const char *type,
                         const char *layer, const char *status,
                         const char *detail)
{
    FILE *file;

    if (jsonl == 0 || jsonl->file == 0) {
        return;
    }

    file = jsonl->file;
    fputs("{\"type\":", file);
    json_string(file, type == 0 ? "status" : type);
    fputs(",\"layer\":", file);
    json_string(file, layer == 0 ? "none" : layer);
    fputs(",\"status\":", file);
    json_string(file, status == 0 ? "ok" : status);
    fputs(",\"event\":", file);
    json_string(file, detail == 0 ? "" : detail);
    fputs("}\n", file);
    fflush(file);
}

void rtc_e2e_jsonl_summary(rtc_e2e_jsonl_t *jsonl, int pass,
                           const char *layer, const char *reason)
{
    FILE *file;

    if (jsonl == 0 || jsonl->file == 0) {
        return;
    }

    file = jsonl->file;
    fputs("{\"type\":\"summary\",\"pass\":", file);
    fputs(pass ? "true" : "false", file);
    fputs(",\"layer\":", file);
    json_string(file, layer == 0 ? "none" : layer);
    fputs(",\"reason\":", file);
    json_string(file, reason == 0 ? "" : reason);
    fputs("}\n", file);
    fflush(file);
}
