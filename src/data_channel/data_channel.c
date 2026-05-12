#include "data_channel.h"

#include "common/mrtc_common.h"

MRTC_DATA_CHANNEL_HANDLE mrtc_data_channel_alloc(const char *label,
                                                 const MRTC_DATA_CHANNEL_INIT *init,
                                                 const MRTC_DATA_CHANNEL_CALLBACKS *callbacks,
                                                 void *user_data)
{
    MRTC_DATA_CHANNEL_HANDLE channel;

    if (label == 0 || label[0] == '\0') {
        return 0;
    }

    channel = (MRTC_DATA_CHANNEL_HANDLE) mrtc_calloc(1, sizeof(*channel));
    if (channel == 0) {
        return 0;
    }

    channel->label = mrtc_strdup(label);
    if (channel->label == 0) {
        mrtc_free(channel);
        return 0;
    }
    channel->ordered = init == 0 ? 1 : init->ordered;
    channel->negotiated = init == 0 ? 0 : init->negotiated;
    channel->stream_id = init == 0 ? 0 : init->id;
    if (callbacks != 0) {
        channel->callbacks = *callbacks;
    }
    channel->user_data = user_data;
    return channel;
}

void mrtc_data_channel_free_internal(MRTC_DATA_CHANNEL_HANDLE channel)
{
    if (channel == 0) {
        return;
    }
    mrtc_free(channel->label);
    mrtc_free(channel);
}

void mrtc_data_channel_mark_open(MRTC_DATA_CHANNEL_HANDLE channel)
{
    if (channel == 0 || channel->closed) {
        return;
    }
    channel->open = 1;
    if (!channel->open_notified && channel->callbacks.on_open != 0) {
        channel->open_notified = 1;
        channel->callbacks.on_open(channel->user_data, channel);
    }
}

MRTC_STATUS mrtc_data_channel_deliver(MRTC_DATA_CHANNEL_HANDLE channel,
                                      MRTC_DATA_CHANNEL_MESSAGE_TYPE message_type,
                                      const unsigned char *data,
                                      size_t data_len)
{
    if (channel == 0 || (data == 0 && data_len > 0)) {
        return MRTC_STATUS_INVALID_ARG;
    }
    if (!channel->open || channel->closed) {
        return MRTC_STATUS_INVALID_STATE;
    }
    if (channel->callbacks.on_message != 0) {
        channel->callbacks.on_message(channel->user_data, channel, message_type, data, data_len);
    }
    return MRTC_STATUS_OK;
}
