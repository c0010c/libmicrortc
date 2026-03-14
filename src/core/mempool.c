#include "mempool.h"

#include <stdint.h>
#include <string.h>

#define ALIGN8(x) (((x) + 7u) & ~7u)

typedef struct block_header {
  size_t len;
  int free;
  struct block_header *next;
} block_header_t;

static size_t header_size(void) { return ALIGN8(sizeof(block_header_t)); }

int rtc_mempool_init(rtc_mempool_t *p, void *buf, size_t len) {
  if (!p || !buf || len <= header_size()) {
    return -1;
  }
  memset(p, 0, sizeof(*p));
  p->base = (uint8_t *)buf;
  p->len = len;
  p->head = (block_header_t *)buf;
  p->head->len = len - header_size();
  p->head->free = 1;
  p->head->next = 0;
  return 0;
}

static void split_if_possible(block_header_t *b, size_t want) {
  size_t h = header_size();
  if (b->len < want + h + 16u) {
    return;
  }
  uint8_t *new_ptr = ((uint8_t *)b) + h + want;
  block_header_t *n = (block_header_t *)new_ptr;
  n->len = b->len - want - h;
  n->free = 1;
  n->next = b->next;
  b->len = want;
  b->next = n;
}

void *rtc_mempool_alloc(rtc_mempool_t *p, size_t len) {
  if (!p || len == 0) {
    return 0;
  }
  size_t want = ALIGN8(len);
  block_header_t *it = p->head;
  while (it) {
    if (it->free && it->len >= want) {
      split_if_possible(it, want);
      it->free = 0;
      p->used += it->len;
      if (p->used > p->peak) {
        p->peak = p->used;
      }
      return ((uint8_t *)it) + header_size();
    }
    it = it->next;
  }
  return 0;
}

static block_header_t *ptr_to_header(void *ptr) {
  return (block_header_t *)(((uint8_t *)ptr) - header_size());
}

static void coalesce(block_header_t *head) {
  block_header_t *it = head;
  while (it && it->next) {
    if (it->free && it->next->free) {
      it->len += header_size() + it->next->len;
      it->next = it->next->next;
      continue;
    }
    it = it->next;
  }
}

void rtc_mempool_free(rtc_mempool_t *p, void *ptr) {
  if (!p || !ptr) {
    return;
  }
  block_header_t *h = ptr_to_header(ptr);
  if (!h->free) {
    h->free = 1;
    if (p->used >= h->len) {
      p->used -= h->len;
    } else {
      p->used = 0;
    }
    coalesce(p->head);
  }
}

size_t rtc_mempool_used(const rtc_mempool_t *p) { return p ? p->used : 0; }

size_t rtc_mempool_peak(const rtc_mempool_t *p) { return p ? p->peak : 0; }
