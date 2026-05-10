#include "test_runner.h"

#include "rtc/config.h"
#include "rtc/security.h"

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

int rtc_test_security(void)
{
    return test_security_backend_contract();
}
