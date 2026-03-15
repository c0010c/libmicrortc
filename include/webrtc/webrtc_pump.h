#ifndef WEBRTC_WEBRTC_PUMP_H
#define WEBRTC_WEBRTC_PUMP_H

#include <stdint.h>

#include "webrtc/webrtc_status.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct webrtc_instance webrtc_instance_t;

/*
 * Advances internal protocol/event processing by at most "budget" units.
 * This function is designed for externally driven loops and does not spawn
 * background threads.
 */
webrtc_status_t webrtc_pump_step(webrtc_instance_t* instance, uint64_t now_ms, uint32_t budget);

#ifdef __cplusplus
}
#endif

#endif /* WEBRTC_WEBRTC_PUMP_H */
