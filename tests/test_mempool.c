#include "test_common.h"

#include <stdint.h>

#include "mempool.h"

int main(void) {
  uint8_t buf[256];
  rtc_mempool_t p;
  void *a;
  void *b;

  ASSERT_EQ_INT(0, rtc_mempool_init(&p, buf, sizeof(buf)));
  a = rtc_mempool_alloc(&p, 64);
  ASSERT_TRUE(a != 0);
  b = rtc_mempool_alloc(&p, 64);
  ASSERT_TRUE(b != 0);
  ASSERT_TRUE(rtc_mempool_used(&p) >= 128);

  rtc_mempool_free(&p, a);
  rtc_mempool_free(&p, b);

  ASSERT_EQ_INT(0, (int)rtc_mempool_used(&p));
  ASSERT_TRUE(rtc_mempool_peak(&p) >= 128);
  return 0;
}
