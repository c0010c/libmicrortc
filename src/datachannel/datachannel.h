#ifndef RTC_DATACHANNEL_H
#define RTC_DATACHANNEL_H

#include <stddef.h>
#include <stdint.h>

#include "rtc/rtc_types.h"

#define RTC_DC_LABEL_MAX 64

typedef struct {
  uint16_t id;
  rtc_channel_state_t state;
  char label[RTC_DC_LABEL_MAX];
} rtc_channel_t;

typedef struct {
  rtc_channel_t *channels;
  uint16_t max_channels;
  uint16_t count;
} rtc_dc_manager_t;

int rtc_dc_init(rtc_dc_manager_t *m, rtc_channel_t *storage, uint16_t max_channels);
rtc_channel_t *rtc_dc_find(rtc_dc_manager_t *m, uint16_t id);
int rtc_dc_add(rtc_dc_manager_t *m, uint16_t id, const char *label, rtc_channel_t **out);
int rtc_dc_close(rtc_dc_manager_t *m, uint16_t id);

#endif
