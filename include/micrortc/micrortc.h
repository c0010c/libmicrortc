#ifndef MRTC_MICRORTC_H
#define MRTC_MICRORTC_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum MRTC_STATUS {
    MRTC_STATUS_OK = 0,
    MRTC_STATUS_INVALID_ARG = 1,
    MRTC_STATUS_NOT_IMPLEMENTED = 2,
    MRTC_STATUS_INVALID_STATE = 3,
    MRTC_STATUS_PARSE_ERROR = 4
} MRTC_STATUS;

const char *mrtc_version_string(void);
MRTC_STATUS mrtc_initialize(void);
void mrtc_shutdown(void);

#include <micrortc/peer_connection.h>

#ifdef __cplusplus
}
#endif

#endif /* MRTC_MICRORTC_H */
