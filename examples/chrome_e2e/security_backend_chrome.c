#include "security_backend_chrome.h"

#include <stddef.h>

rtc_status_t rtc_chrome_e2e_configure_security_backend(
    rtc_peer_connection_config_t *config)
{
    if (config == 0) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }

#if defined(RTC_CHROME_E2E_WITH_OPTIONAL_SECURITY)
    (void)config;
    return RTC_STATUS_UNSUPPORTED;
#else
    config->security_backend = 0;
    return RTC_STATUS_UNSUPPORTED;
#endif
}
