#ifndef RTC_MEMPOOL_H
#define RTC_MEMPOOL_H

#include <stddef.h>
#include <stdint.h>

typedef struct rtc_mempool rtc_mempool_t;
typedef struct block_header block_header_t;

struct rtc_mempool {
  uint8_t *base;
  size_t len;
  block_header_t *head;
  size_t used;
  size_t peak;
};

int rtc_mempool_init(rtc_mempool_t *p, void *buf, size_t len);
void *rtc_mempool_alloc(rtc_mempool_t *p, size_t len);
void rtc_mempool_free(rtc_mempool_t *p, void *ptr);
size_t rtc_mempool_used(const rtc_mempool_t *p);
size_t rtc_mempool_peak(const rtc_mempool_t *p);

#endif
