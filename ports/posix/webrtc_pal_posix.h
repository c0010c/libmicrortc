#ifndef WEBRTC_PAL_POSIX_H
#define WEBRTC_PAL_POSIX_H

#include "webrtc/webrtc_pal.h"
#include "webrtc/webrtc_status.h"

#ifdef __cplusplus
extern "C" {
#endif

webrtc_status_t webrtc_pal_posix_vtable(webrtc_pal_vtable_t* out_pal);

#ifdef __cplusplus
}
#endif

#endif /* WEBRTC_PAL_POSIX_H */
