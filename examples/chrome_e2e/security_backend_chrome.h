#ifndef RTC_CHROME_E2E_SECURITY_BACKEND_CHROME_H
#define RTC_CHROME_E2E_SECURITY_BACKEND_CHROME_H

#include "rtc/config.h"
#include "rtc/status.h"

#ifdef __cplusplus
extern "C" {
#endif

rtc_status_t rtc_chrome_e2e_configure_security_backend(
    rtc_peer_connection_config_t *config);

#ifdef __cplusplus
}
#endif

#endif
