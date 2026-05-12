#ifndef MRTC_PEER_CONNECTION_H
#define MRTC_PEER_CONNECTION_H

#include <stddef.h>

#ifndef MRTC_STATUS_DEFINED
#define MRTC_STATUS_DEFINED
typedef enum MRTC_STATUS {
    MRTC_STATUS_OK = 0,
    MRTC_STATUS_INVALID_ARG = 1,
    MRTC_STATUS_NOT_IMPLEMENTED = 2,
    MRTC_STATUS_INVALID_STATE = 3,
    MRTC_STATUS_PARSE_ERROR = 4
} MRTC_STATUS;
#endif /* MRTC_STATUS_DEFINED */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MRTC_PEER_CONNECTION *MRTC_PEER_CONNECTION_HANDLE;

typedef struct MRTC_PEER_CONNECTION_CONFIG {
    const char *bundle_policy;
} MRTC_PEER_CONNECTION_CONFIG;

typedef struct MRTC_PEER_CONNECTION_CALLBACKS {
    void (*on_ice_candidate)(void *user_data, const char *candidate);
} MRTC_PEER_CONNECTION_CALLBACKS;

MRTC_STATUS mrtc_peer_connection_create(const MRTC_PEER_CONNECTION_CONFIG *config,
                                        const MRTC_PEER_CONNECTION_CALLBACKS *callbacks,
                                        void *user_data,
                                        MRTC_PEER_CONNECTION_HANDLE *peer_connection);

void mrtc_peer_connection_free(MRTC_PEER_CONNECTION_HANDLE peer_connection);

MRTC_STATUS mrtc_peer_connection_set_remote_description(MRTC_PEER_CONNECTION_HANDLE peer_connection,
                                                        const char *type,
                                                        const char *sdp);

MRTC_STATUS mrtc_peer_connection_create_answer(MRTC_PEER_CONNECTION_HANDLE peer_connection,
                                               char *buffer,
                                               size_t buffer_len,
                                               size_t *required_len);

MRTC_STATUS mrtc_peer_connection_set_local_description(MRTC_PEER_CONNECTION_HANDLE peer_connection,
                                                       const char *type,
                                                       const char *sdp);

MRTC_STATUS mrtc_peer_connection_create_offer(MRTC_PEER_CONNECTION_HANDLE peer_connection,
                                              char *buffer,
                                              size_t buffer_len,
                                              size_t *required_len);

MRTC_STATUS mrtc_peer_connection_add_ice_candidate(MRTC_PEER_CONNECTION_HANDLE peer_connection,
                                                   const char *candidate);

#ifdef __cplusplus
}
#endif

#endif /* MRTC_PEER_CONNECTION_H */
