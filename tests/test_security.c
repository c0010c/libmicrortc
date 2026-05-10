#include "test_runner.h"

#include "rtc/config.h"
#include "rtc/counters.h"
#include "rtc/security.h"
#include "rtc/trace.h"

#include "observability/counters.h"

static int test_security_backend_contract(void)
{
    rtc_security_backend_event_t event;
    rtc_security_backend_config_t backend;
    rtc_peer_connection_config_t config;

    event.type = RTC_SECURITY_BACKEND_EVENT_OUTGOING_DATAGRAM;
    event.datagram = 0;
    event.datagram_len = 0;
    event.status = RTC_STATUS_OK;
    event.detail_code = 0;

    backend.vtable = 0;
    backend.user_data = 0;
    backend.session_storage_bytes = 32;

    config.security_backend = &backend;

    RTC_TEST_EQ_INT(RTC_SECURITY_BACKEND_EVENT_OUTGOING_DATAGRAM, event.type);
    RTC_TEST_ASSERT(config.security_backend == &backend);

    return 0;
}

static int test_security_counters_and_trace_contract(void)
{
    rtc_peer_connection_counters_t counters;
    rtc_security_backend_event_t event;

    rtc_counters_init(&counters);

    RTC_TEST_EQ_INT(0, counters.dtls.handshake_started);
    RTC_TEST_EQ_INT(0, counters.dtls.handshake_completed);
    RTC_TEST_EQ_INT(0, counters.dtls.handshake_failed);
    RTC_TEST_EQ_INT(0, counters.dtls.early_datagrams_rejected);
    RTC_TEST_EQ_INT(0, counters.dtls.outgoing_datagrams);
    RTC_TEST_EQ_INT(0, counters.dtls.fingerprint_mismatch);
    RTC_TEST_EQ_INT(0, counters.dtls.key_export_failed);
    RTC_TEST_EQ_INT(0, counters.dtls.srtp_init_failed);
    RTC_TEST_EQ_INT(0, counters.srtp.protect_failed);
    RTC_TEST_EQ_INT(0, counters.srtp.unprotect_failed);
    RTC_TEST_EQ_INT(0, counters.srtp.replay_failed);

    event.type = RTC_SECURITY_BACKEND_EVENT_HANDSHAKE_COMPLETE;
    event.datagram = 0;
    event.datagram_len = 0;
    event.status = RTC_STATUS_OK;
    event.detail_code = 0;

    RTC_TEST_ASSERT(RTC_TRACE_DTLS_STATE[0] != '\0');
    RTC_TEST_ASSERT(RTC_TRACE_DTLS_HANDSHAKE[0] != '\0');
    RTC_TEST_ASSERT(RTC_TRACE_SRTP_STATE[0] != '\0');
    RTC_TEST_ASSERT(RTC_TRACE_SRTP_PROTECT[0] != '\0');
    RTC_TEST_EQ_INT(RTC_SECURITY_BACKEND_EVENT_HANDSHAKE_COMPLETE, event.type);

    return 0;
}

int rtc_test_security(void)
{
    int status = test_security_backend_contract();
    if (status != 0) {
        return status;
    }
    return test_security_counters_and_trace_contract();
}
