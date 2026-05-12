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
typedef struct MRTC_DATA_CHANNEL *MRTC_DATA_CHANNEL_HANDLE;

typedef struct MRTC_ICE_SERVER {
    const char *urls;
    const char *username;
    const char *password;
} MRTC_ICE_SERVER;

typedef enum MRTC_PEER_CONNECTION_STATE {
    MRTC_PEER_CONNECTION_STATE_NEW = 0,
    MRTC_PEER_CONNECTION_STATE_CONNECTING = 1,
    MRTC_PEER_CONNECTION_STATE_CONNECTED = 2,
    MRTC_PEER_CONNECTION_STATE_DISCONNECTED = 3,
    MRTC_PEER_CONNECTION_STATE_FAILED = 4,
    MRTC_PEER_CONNECTION_STATE_CLOSED = 5
} MRTC_PEER_CONNECTION_STATE;

typedef enum MRTC_DATA_CHANNEL_MESSAGE_TYPE {
    MRTC_DATA_CHANNEL_MESSAGE_TYPE_TEXT = 0,
    MRTC_DATA_CHANNEL_MESSAGE_TYPE_BINARY = 1
} MRTC_DATA_CHANNEL_MESSAGE_TYPE;

typedef struct MRTC_DATA_CHANNEL_INIT {
    int ordered;
    int negotiated;
    unsigned short id;
} MRTC_DATA_CHANNEL_INIT;

typedef struct MRTC_DATA_CHANNEL_CALLBACKS {
    void (*on_open)(void *user_data, MRTC_DATA_CHANNEL_HANDLE channel);
    void (*on_message)(void *user_data,
                       MRTC_DATA_CHANNEL_HANDLE channel,
                       MRTC_DATA_CHANNEL_MESSAGE_TYPE message_type,
                       const unsigned char *data,
                       size_t data_len);
    void (*on_close)(void *user_data, MRTC_DATA_CHANNEL_HANDLE channel);
} MRTC_DATA_CHANNEL_CALLBACKS;

typedef struct MRTC_PEER_CONNECTION_CONFIG {
    const char *bundle_policy;
    const MRTC_ICE_SERVER *ice_servers;
    size_t ice_server_count;
} MRTC_PEER_CONNECTION_CONFIG;

typedef struct MRTC_PEER_CONNECTION_CALLBACKS {
    void (*on_ice_candidate)(void *user_data, const char *candidate);
    void (*on_connection_state_change)(void *user_data, MRTC_PEER_CONNECTION_STATE state);
    void (*on_data_channel)(void *user_data, MRTC_DATA_CHANNEL_HANDLE channel);
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

MRTC_STATUS mrtc_peer_connection_create_data_channel(MRTC_PEER_CONNECTION_HANDLE peer_connection,
                                                     const char *label,
                                                     const MRTC_DATA_CHANNEL_INIT *init,
                                                     const MRTC_DATA_CHANNEL_CALLBACKS *callbacks,
                                                     void *user_data,
                                                     MRTC_DATA_CHANNEL_HANDLE *channel);

MRTC_STATUS mrtc_data_channel_set_callbacks(MRTC_DATA_CHANNEL_HANDLE channel,
                                            const MRTC_DATA_CHANNEL_CALLBACKS *callbacks,
                                            void *user_data);

MRTC_STATUS mrtc_data_channel_send(MRTC_DATA_CHANNEL_HANDLE channel,
                                   MRTC_DATA_CHANNEL_MESSAGE_TYPE message_type,
                                   const unsigned char *data,
                                   size_t data_len);

MRTC_STATUS mrtc_data_channel_close(MRTC_DATA_CHANNEL_HANDLE channel);

#ifdef __cplusplus
}
#endif

#endif /* MRTC_PEER_CONNECTION_H */
