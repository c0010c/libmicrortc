#ifndef RTC_RTCP_H_
#define RTC_RTCP_H_

#include <stdint.h>

#include "rtc/rtc.h"

typedef enum rtc_rtcp_event_type {
  RTC_RTCP_EVENT_UNKNOWN = 0,
  RTC_RTCP_EVENT_RR = 1,
  RTC_RTCP_EVENT_PLI = 2,
  RTC_RTCP_EVENT_NACK = 3
} rtc_rtcp_event_type_t;

typedef struct rtc_rtcp_event {
  rtc_rtcp_event_type_t type;
  uint8_t pt;
  uint8_t fmt;
  uint8_t nack_truncated;
  uint8_t reserved;
  uint16_t nack_seq_count;
  uint32_t sender_ssrc;
  uint32_t media_ssrc;
  uint16_t nack_seq[RTC_CFG_RTX_CACHE];
} rtc_rtcp_event_t;

typedef struct rtc_rtcp_parser {
  const uint8_t *packet;
  uint16_t packet_len;
  uint16_t offset;
  uint16_t last_offset;
  uint8_t last_pt;
  uint8_t last_fmt;
} rtc_rtcp_parser_t;

void rtc_rtcp_parser_init(rtc_rtcp_parser_t *parser, const uint8_t *packet,
                          uint16_t packet_len);
rtc_result_t rtc_rtcp_parser_next(rtc_rtcp_parser_t *parser,
                                  rtc_rtcp_event_t *out_event);

#endif  // RTC_RTCP_H_
