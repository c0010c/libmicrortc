#ifndef MRTC_DATA_CHANNEL_INTERNAL_H
#define MRTC_DATA_CHANNEL_INTERNAL_H

#include <micrortc/peer_connection.h>

struct MRTC_DATA_CHANNEL {
    char *label;
    int ordered;
    int negotiated;
    unsigned short stream_id;
    int open;
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

#endif /* MRTC_DATA_CHANNEL_INTERNAL_H */
