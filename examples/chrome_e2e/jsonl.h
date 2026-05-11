#ifndef RTC_CHROME_E2E_JSONL_H
#define RTC_CHROME_E2E_JSONL_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rtc_e2e_jsonl_t {
    FILE *file;
} rtc_e2e_jsonl_t;

int rtc_e2e_jsonl_open(rtc_e2e_jsonl_t *jsonl, const char *path);
void rtc_e2e_jsonl_close(rtc_e2e_jsonl_t *jsonl);
void rtc_e2e_jsonl_event(rtc_e2e_jsonl_t *jsonl, const char *type,
                         const char *layer, const char *status,
                         const char *detail);
void rtc_e2e_jsonl_summary(rtc_e2e_jsonl_t *jsonl, int pass,
                           const char *layer, const char *reason);

#ifdef __cplusplus
}
#endif

#endif
