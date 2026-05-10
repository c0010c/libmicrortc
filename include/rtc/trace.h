#ifndef RTC_TRACE_H
#define RTC_TRACE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RTC_TRACE_PC_CREATE "pc.create"
#define RTC_TRACE_PC_DESTROY "pc.destroy"
#define RTC_TRACE_CAPACITY_FAILURE "capacity.failure"
#define RTC_TRACE_AFFINITY_VIOLATION "affinity.violation"
#define RTC_TRACE_UNSUPPORTED_API "api.unsupported"
#define RTC_TRACE_SDP_PARSE "sdp.parse"
#define RTC_TRACE_SDP_WRITE "sdp.write"
#define RTC_TRACE_JSEP_TRANSITION "jsep.transition"
#define RTC_TRACE_JSEP_REJECT "jsep.reject"
#define RTC_TRACE_ICE_CANDIDATE_STORED "ice.candidate.stored"
#define RTC_TRACE_ICE_STATE "ice.state"
#define RTC_TRACE_ICE_CANDIDATE_LOCAL "ice.candidate.local"
#define RTC_TRACE_ICE_CANDIDATE_REMOTE "ice.candidate.remote"
#define RTC_TRACE_ICE_PAIR_CREATED "ice.pair.created"
#define RTC_TRACE_ICE_SELECTED_PAIR "ice.selected_pair"
#define RTC_TRACE_STUN_TRANSACTION "stun.transaction"
#define RTC_TRACE_NET_DEMUX "net.demux"
#define RTC_TRACE_DTLS_STATE "dtls.state"
#define RTC_TRACE_DTLS_HANDSHAKE "dtls.handshake"
#define RTC_TRACE_SRTP_STATE "srtp.state"
#define RTC_TRACE_SRTP_PROTECT "srtp.protect"
#define RTC_TRACE_MEDIA_FRAME "media.frame"
#define RTC_TRACE_RTP_PACKET "rtp.packet"
#define RTC_TRACE_RTCP_PACKET "rtcp.packet"
#define RTC_TRACE_MEDIA_FEEDBACK "media.feedback"

#define RTC_TRACE_FIELD_SUBSYSTEM "subsystem"
#define RTC_TRACE_FIELD_OPERATION "operation"
#define RTC_TRACE_FIELD_STATUS "status"
#define RTC_TRACE_FIELD_RESOURCE "resource"
#define RTC_TRACE_FIELD_REQUIRED "required"
#define RTC_TRACE_FIELD_USED "used"
#define RTC_TRACE_FIELD_STATE "state"
#define RTC_TRACE_FIELD_TARGET_STATE "target_state"
#define RTC_TRACE_FIELD_DESCRIPTION_TYPE "description_type"
#define RTC_TRACE_FIELD_REASON "reason"
#define RTC_TRACE_FIELD_CANDIDATE_TYPE "candidate_type"
#define RTC_TRACE_FIELD_CANDIDATE_ID "candidate_id"
#define RTC_TRACE_FIELD_PAIR_ID "pair_id"
#define RTC_TRACE_FIELD_ROLE "role"
#define RTC_TRACE_FIELD_LOCAL_ADDRESS "local_address"
#define RTC_TRACE_FIELD_REMOTE_ADDRESS "remote_address"
#define RTC_TRACE_FIELD_PORT "port"
#define RTC_TRACE_FIELD_TRANSACTION_ID "transaction_id"
#define RTC_TRACE_FIELD_PROTOCOL "protocol"
#define RTC_TRACE_FIELD_MEDIA_KIND "media_kind"
#define RTC_TRACE_FIELD_SSRC "ssrc"
#define RTC_TRACE_FIELD_SEQUENCE "sequence"
#define RTC_TRACE_FIELD_TIMESTAMP "timestamp"

typedef struct rtc_trace_field_t {
    const char *key;
    const char *value;
    uint64_t number;
} rtc_trace_field_t;

#ifdef __cplusplus
}
#endif

#endif
