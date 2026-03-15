#ifndef WEBRTC_WEBRTC_STATUS_H
#define WEBRTC_WEBRTC_STATUS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int32_t webrtc_status_t;

#define WEBRTC_STATUS_OK ((webrtc_status_t) 0)
#define WEBRTC_STATUS_INVALID_ARG ((webrtc_status_t) -1)
#define WEBRTC_STATUS_NO_MEMORY ((webrtc_status_t) -2)
#define WEBRTC_STATUS_INVALID_STATE ((webrtc_status_t) -3)
#define WEBRTC_STATUS_NOT_IMPLEMENTED ((webrtc_status_t) -4)

#ifdef __cplusplus
}
#endif

#endif /* WEBRTC_WEBRTC_STATUS_H */
