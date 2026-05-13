#ifndef MRTC_DATA_CHANNEL_INTERNAL_H
#define MRTC_DATA_CHANNEL_INTERNAL_H

#include <micrortc/peer_connection.h>

struct MRTC_DATA_CHANNEL {
    MRTC_PEER_CONNECTION_HANDLE owner;
    char *label;
    int ordered;
    int negotiated;
    unsigned short stream_id;
    int open;
    int open_notified;
    int closed;
    MRTC_DATA_CHANNEL_CALLBACKS callbacks;
    void *user_data;
    struct MRTC_DATA_CHANNEL *next;
};

MRTC_DATA_CHANNEL_HANDLE mrtc_data_channel_alloc(const char *label,
                                                 const MRTC_DATA_CHANNEL_INIT *init,
                                                 const MRTC_DATA_CHANNEL_CALLBACKS *callbacks,
                                                 void *user_data);
void mrtc_data_channel_free_internal(MRTC_DATA_CHANNEL_HANDLE channel);
void mrtc_data_channel_mark_open(MRTC_DATA_CHANNEL_HANDLE channel);
MRTC_STATUS mrtc_data_channel_deliver(MRTC_DATA_CHANNEL_HANDLE channel,
                                      MRTC_DATA_CHANNEL_MESSAGE_TYPE message_type,
                                      const unsigned char *data,
                                      size_t data_len);

#endif /* MRTC_DATA_CHANNEL_INTERNAL_H */
