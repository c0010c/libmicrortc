#include "datachannel.h"

#include <string.h>

int rtc_dc_init(rtc_dc_manager_t *m, rtc_channel_t *storage, uint16_t max_channels) {
  if (!m || !storage || max_channels == 0) {
    return -1;
  }
  memset(storage, 0, sizeof(rtc_channel_t) * max_channels);
  m->channels = storage;
  m->max_channels = max_channels;
  m->count = 0;
  return 0;
}

rtc_channel_t *rtc_dc_find(rtc_dc_manager_t *m, uint16_t id) {
  uint16_t i;
  if (!m) {
    return 0;
  }
  for (i = 0; i < m->count; ++i) {
    if (m->channels[i].id == id) {
      return &m->channels[i];
    }
  }
  return 0;
}

int rtc_dc_add(rtc_dc_manager_t *m, uint16_t id, const char *label, rtc_channel_t **out) {
  size_t n;
  if (!m || !label) {
    return -1;
  }
  if (m->count >= m->max_channels) {
    return -1;
  }
  m->channels[m->count].id = id;
  m->channels[m->count].state = RTC_CHANNEL_OPEN;
  n = strlen(label);
  if (n >= RTC_DC_LABEL_MAX) {
    n = RTC_DC_LABEL_MAX - 1;
  }
  memcpy(m->channels[m->count].label, label, n);
  m->channels[m->count].label[n] = '\0';
  if (out) {
    *out = &m->channels[m->count];
  }
  ++m->count;
  return 0;
}

int rtc_dc_close(rtc_dc_manager_t *m, uint16_t id) {
  rtc_channel_t *ch = rtc_dc_find(m, id);
  if (!ch) {
    return -1;
  }
  ch->state = RTC_CHANNEL_CLOSED;
  return 0;
}
