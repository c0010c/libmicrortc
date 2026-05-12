#include <micrortc/micrortc.h>

#include "common/mrtc_common.h"
#include "data_channel/data_channel.h"
#include "dtls/dtls_session.h"
#include "ice/ice_agent.h"
#include "sdp.h"

#include <stdlib.h>
#include <string.h>

struct MRTC_PEER_CONNECTION {
    MRTC_PEER_CONNECTION_CONFIG config;
    char *bundle_policy;
    MRTC_ICE_SERVER *ice_servers;
    MRTC_PEER_CONNECTION_CALLBACKS callbacks;
    void *user_data;
    char *remote_description_type;
    char *remote_description_sdp;
    char *local_description_type;
    char *local_description_sdp;
    char *last_remote_candidate;
    char local_ice_ufrag[17];
    char local_ice_pwd[33];
    char local_fingerprint[96];
    char *remote_fingerprint;
    char *remote_setup;
    MRTC_PEER_CONNECTION_STATE state;
    int has_remote_description;
    int has_local_description;
    unsigned short next_stream_id;
    MRTC_DATA_CHANNEL_HANDLE data_channels;
};

static int mrtc_string_equals(const char *left, const char *right)
{
    return left != 0 && right != 0 && strcmp(left, right) == 0;
}

static MRTC_STATUS mrtc_duplicate_string(const char *value, char **copy)
{
    size_t len;
    char *result;

    if (value == 0 || copy == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }

    len = strlen(value);
    result = (char *) malloc(len + 1);
    if (result == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }

    memcpy(result, value, len + 1);
    *copy = result;
    return MRTC_STATUS_OK;
}

static MRTC_STATUS mrtc_duplicate_optional_string(const char *value, const char **copy)
{
    char *mutable_copy = 0;

    if (copy == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }

    *copy = 0;
    if (value == 0) {
        return MRTC_STATUS_OK;
    }

    if (mrtc_duplicate_string(value, &mutable_copy) != MRTC_STATUS_OK) {
        return MRTC_STATUS_INVALID_ARG;
    }

    *copy = mutable_copy;
    return MRTC_STATUS_OK;
}

static MRTC_STATUS mrtc_replace_string(char **target, const char *value)
{
    char *copy = 0;
    MRTC_STATUS status = mrtc_duplicate_string(value, &copy);

    if (status != MRTC_STATUS_OK) {
        return status;
    }

    free(*target);
    *target = copy;
    return MRTC_STATUS_OK;
}

static MRTC_STATUS mrtc_fill_token(char *buffer, size_t buffer_len)
{
    static const char alphabet[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    uint8_t random_bytes[32];
    size_t i;

    if (buffer == 0 || buffer_len < 2 || buffer_len - 1 > sizeof(random_bytes)) {
        return MRTC_STATUS_INVALID_ARG;
    }

    if (mrtc_random_bytes(random_bytes, buffer_len - 1) != MRTC_STATUS_OK) {
        return MRTC_STATUS_INVALID_STATE;
    }

    for (i = 0; i + 1 < buffer_len; ++i) {
        buffer[i] = alphabet[random_bytes[i] % (sizeof(alphabet) - 1)];
    }
    buffer[buffer_len - 1] = '\0';
    return MRTC_STATUS_OK;
}

static void mrtc_peer_connection_set_state(MRTC_PEER_CONNECTION_HANDLE peer_connection, MRTC_PEER_CONNECTION_STATE state)
{
    if (peer_connection == 0 || peer_connection->state == state) {
        return;
    }

    peer_connection->state = state;
    if (peer_connection->callbacks.on_connection_state_change != 0) {
        peer_connection->callbacks.on_connection_state_change(peer_connection->user_data, state);
    }
}

static void mrtc_free_ice_servers(MRTC_PEER_CONNECTION_HANDLE peer_connection)
{
    size_t i;

    if (peer_connection == 0 || peer_connection->ice_servers == 0) {
        return;
    }

    for (i = 0; i < peer_connection->config.ice_server_count; ++i) {
        free((void *) peer_connection->ice_servers[i].urls);
        free((void *) peer_connection->ice_servers[i].username);
        free((void *) peer_connection->ice_servers[i].password);
    }
    free(peer_connection->ice_servers);
    peer_connection->ice_servers = 0;
    peer_connection->config.ice_servers = 0;
    peer_connection->config.ice_server_count = 0;
}

static MRTC_STATUS mrtc_copy_config(MRTC_PEER_CONNECTION_HANDLE peer_connection, const MRTC_PEER_CONNECTION_CONFIG *config)
{
    size_t i;

    peer_connection->config = *config;
    peer_connection->config.bundle_policy = 0;
    peer_connection->config.ice_servers = 0;
    peer_connection->config.ice_server_count = config->ice_server_count;

    if (mrtc_duplicate_optional_string(config->bundle_policy, &peer_connection->config.bundle_policy) != MRTC_STATUS_OK) {
        return MRTC_STATUS_INVALID_ARG;
    }
    peer_connection->bundle_policy = (char *) peer_connection->config.bundle_policy;

    if (config->ice_server_count == 0) {
        return MRTC_STATUS_OK;
    }
    if (config->ice_servers == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }

    peer_connection->ice_servers = (MRTC_ICE_SERVER *) calloc(config->ice_server_count, sizeof(*peer_connection->ice_servers));
    if (peer_connection->ice_servers == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }

    for (i = 0; i < config->ice_server_count; ++i) {
        if (config->ice_servers[i].urls == 0 || config->ice_servers[i].urls[0] == '\0') {
            return MRTC_STATUS_INVALID_ARG;
        }
        if (mrtc_duplicate_optional_string(config->ice_servers[i].urls, &peer_connection->ice_servers[i].urls) != MRTC_STATUS_OK ||
            mrtc_duplicate_optional_string(config->ice_servers[i].username, &peer_connection->ice_servers[i].username) != MRTC_STATUS_OK ||
            mrtc_duplicate_optional_string(config->ice_servers[i].password, &peer_connection->ice_servers[i].password) != MRTC_STATUS_OK) {
            return MRTC_STATUS_INVALID_ARG;
        }
    }

    peer_connection->config.ice_servers = peer_connection->ice_servers;
    return MRTC_STATUS_OK;
}

MRTC_STATUS mrtc_peer_connection_create(const MRTC_PEER_CONNECTION_CONFIG *config,
                                        const MRTC_PEER_CONNECTION_CALLBACKS *callbacks,
                                        void *user_data,
                                        MRTC_PEER_CONNECTION_HANDLE *peer_connection)
{
    MRTC_PEER_CONNECTION_HANDLE created;

    if (config == 0 || peer_connection == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }

    created = (MRTC_PEER_CONNECTION_HANDLE) calloc(1, sizeof(*created));
    if (created == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }

    if (mrtc_copy_config(created, config) != MRTC_STATUS_OK) {
        mrtc_free_ice_servers(created);
        free(created->bundle_policy);
        free(created);
        return MRTC_STATUS_INVALID_ARG;
    }
    if (callbacks != 0) {
        created->callbacks = *callbacks;
    }
    created->user_data = user_data;
    created->state = MRTC_PEER_CONNECTION_STATE_NEW;
    created->next_stream_id = 0;
    {
        size_t fingerprint_len = 0;
        if (mrtc_fill_token(created->local_ice_ufrag, sizeof(created->local_ice_ufrag)) != MRTC_STATUS_OK ||
            mrtc_fill_token(created->local_ice_pwd, sizeof(created->local_ice_pwd)) != MRTC_STATUS_OK ||
            mrtc_dtls_generate_fingerprint(created->local_fingerprint, sizeof(created->local_fingerprint), &fingerprint_len) != MRTC_STATUS_OK) {
            mrtc_free_ice_servers(created);
            free(created->bundle_policy);
            free(created);
            return MRTC_STATUS_INVALID_STATE;
        }
    }

    *peer_connection = created;
    return MRTC_STATUS_OK;
}

void mrtc_peer_connection_free(MRTC_PEER_CONNECTION_HANDLE peer_connection)
{
    if (peer_connection == 0) {
        return;
    }

    free(peer_connection->remote_description_type);
    free(peer_connection->remote_description_sdp);
    free(peer_connection->local_description_type);
    free(peer_connection->local_description_sdp);
    free(peer_connection->last_remote_candidate);
    free(peer_connection->remote_fingerprint);
    free(peer_connection->remote_setup);
    while (peer_connection->data_channels != 0) {
        MRTC_DATA_CHANNEL_HANDLE next = peer_connection->data_channels->next;
        mrtc_data_channel_free_internal(peer_connection->data_channels);
        peer_connection->data_channels = next;
    }
    mrtc_free_ice_servers(peer_connection);
    free(peer_connection->bundle_policy);
    free(peer_connection);
}

MRTC_STATUS mrtc_peer_connection_set_remote_description(MRTC_PEER_CONNECTION_HANDLE peer_connection,
                                                        const char *type,
                                                        const char *sdp)
{
    MRTC_STATUS status;
    MRTC_SDP *parsed = 0;

    if (peer_connection == 0 || type == 0 || sdp == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }

    if (!mrtc_string_equals(type, "offer")) {
        return MRTC_STATUS_PARSE_ERROR;
    }

    status = mrtc_sdp_parse(sdp, &parsed);
    if (status != MRTC_STATUS_OK) {
        return MRTC_STATUS_PARSE_ERROR;
    }
    if (mrtc_sdp_get_fingerprint(parsed) != 0) {
        status = mrtc_replace_string(&peer_connection->remote_fingerprint, mrtc_sdp_get_fingerprint(parsed));
        if (status != MRTC_STATUS_OK) {
            mrtc_sdp_free(parsed);
            return status;
        }
    }
    if (mrtc_sdp_get_setup(parsed) != 0) {
        status = mrtc_replace_string(&peer_connection->remote_setup, mrtc_sdp_get_setup(parsed));
        if (status != MRTC_STATUS_OK) {
            mrtc_sdp_free(parsed);
            return status;
        }
    }
    mrtc_sdp_free(parsed);

    status = mrtc_replace_string(&peer_connection->remote_description_type, type);
    if (status != MRTC_STATUS_OK) {
        return status;
    }

    status = mrtc_replace_string(&peer_connection->remote_description_sdp, sdp);
    if (status != MRTC_STATUS_OK) {
        return status;
    }

    peer_connection->has_remote_description = 1;
    mrtc_peer_connection_set_state(peer_connection, MRTC_PEER_CONNECTION_STATE_CONNECTING);
    return MRTC_STATUS_OK;
}

MRTC_STATUS mrtc_peer_connection_create_answer(MRTC_PEER_CONNECTION_HANDLE peer_connection,
                                               char *buffer,
                                               size_t buffer_len,
                                               size_t *required_len)
{
    if (peer_connection == 0 || required_len == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }

    if (!peer_connection->has_remote_description || peer_connection->remote_description_sdp == 0) {
        return MRTC_STATUS_INVALID_STATE;
    }

    return mrtc_sdp_create_answer_ex(peer_connection->remote_description_sdp,
                                     peer_connection->data_channels != 0,
                                     peer_connection->local_ice_ufrag,
                                     peer_connection->local_ice_pwd,
                                     peer_connection->local_fingerprint,
                                     buffer,
                                     buffer_len,
                                     required_len);
}

MRTC_STATUS mrtc_peer_connection_set_local_description(MRTC_PEER_CONNECTION_HANDLE peer_connection,
                                                       const char *type,
                                                       const char *sdp)
{
    MRTC_STATUS status;
    MRTC_SDP *parsed = 0;

    if (peer_connection == 0 || type == 0 || sdp == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }

    if (!mrtc_string_equals(type, "answer")) {
        return MRTC_STATUS_PARSE_ERROR;
    }

    status = mrtc_sdp_parse(sdp, &parsed);
    if (status != MRTC_STATUS_OK) {
        return MRTC_STATUS_PARSE_ERROR;
    }
    mrtc_sdp_free(parsed);

    status = mrtc_replace_string(&peer_connection->local_description_type, type);
    if (status != MRTC_STATUS_OK) {
        return status;
    }

    status = mrtc_replace_string(&peer_connection->local_description_sdp, sdp);
    if (status != MRTC_STATUS_OK) {
        return status;
    }

    peer_connection->has_local_description = 1;
    if (peer_connection->callbacks.on_ice_candidate != 0) {
        char candidate[128];
        size_t candidate_len = 0;
        if (mrtc_ice_format_host_candidate(candidate, sizeof(candidate), &candidate_len) == MRTC_STATUS_OK) {
            peer_connection->callbacks.on_ice_candidate(peer_connection->user_data, candidate);
        }
    }
    return MRTC_STATUS_OK;
}

MRTC_STATUS mrtc_peer_connection_create_offer(MRTC_PEER_CONNECTION_HANDLE peer_connection,
                                              char *buffer,
                                              size_t buffer_len,
                                              size_t *required_len)
{
    (void) buffer;
    (void) buffer_len;

    if (peer_connection == 0 || required_len == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }

    *required_len = 0;
    return MRTC_STATUS_NOT_IMPLEMENTED;
}

MRTC_STATUS mrtc_peer_connection_add_ice_candidate(MRTC_PEER_CONNECTION_HANDLE peer_connection,
                                                   const char *candidate)
{
    MRTC_ICE_CANDIDATE parsed;

    if (peer_connection == 0 || candidate == 0 || candidate[0] == '\0') {
        return MRTC_STATUS_INVALID_ARG;
    }

    if (mrtc_ice_parse_candidate(candidate, &parsed) != MRTC_STATUS_OK) {
        return MRTC_STATUS_PARSE_ERROR;
    }

    if (mrtc_replace_string(&peer_connection->last_remote_candidate, candidate) != MRTC_STATUS_OK) {
        return MRTC_STATUS_INVALID_ARG;
    }

    if (peer_connection->has_remote_description && peer_connection->has_local_description) {
        mrtc_peer_connection_set_state(peer_connection, MRTC_PEER_CONNECTION_STATE_CONNECTED);
    }

    return MRTC_STATUS_OK;
}

MRTC_STATUS mrtc_peer_connection_create_data_channel(MRTC_PEER_CONNECTION_HANDLE peer_connection,
                                                     const char *label,
                                                     const MRTC_DATA_CHANNEL_INIT *init,
                                                     const MRTC_DATA_CHANNEL_CALLBACKS *callbacks,
                                                     void *user_data,
                                                     MRTC_DATA_CHANNEL_HANDLE *channel)
{
    MRTC_DATA_CHANNEL_HANDLE created;

    if (peer_connection == 0 || label == 0 || channel == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }
    if (init != 0 && init->negotiated) {
        return MRTC_STATUS_INVALID_STATE;
    }

    created = mrtc_data_channel_alloc(label, init, callbacks, user_data);
    if (created == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }

    created->stream_id = peer_connection->next_stream_id;
    peer_connection->next_stream_id = (unsigned short) (peer_connection->next_stream_id + 2);
    created->next = peer_connection->data_channels;
    peer_connection->data_channels = created;
    *channel = created;
    return MRTC_STATUS_OK;
}

MRTC_STATUS mrtc_data_channel_set_callbacks(MRTC_DATA_CHANNEL_HANDLE channel,
                                            const MRTC_DATA_CHANNEL_CALLBACKS *callbacks,
                                            void *user_data)
{
    if (channel == 0 || callbacks == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }

    channel->callbacks = *callbacks;
    channel->user_data = user_data;
    return MRTC_STATUS_OK;
}

MRTC_STATUS mrtc_data_channel_send(MRTC_DATA_CHANNEL_HANDLE channel,
                                   MRTC_DATA_CHANNEL_MESSAGE_TYPE message_type,
                                   const unsigned char *data,
                                   size_t data_len)
{
    (void) message_type;

    if (channel == 0 || (data == 0 && data_len > 0)) {
        return MRTC_STATUS_INVALID_ARG;
    }
    if (!channel->open || channel->closed) {
        return MRTC_STATUS_INVALID_STATE;
    }
    return MRTC_STATUS_OK;
}

MRTC_STATUS mrtc_data_channel_close(MRTC_DATA_CHANNEL_HANDLE channel)
{
    if (channel == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }
    if (channel->closed) {
        return MRTC_STATUS_INVALID_STATE;
    }
    channel->closed = 1;
    channel->open = 0;
    if (channel->callbacks.on_close != 0) {
        channel->callbacks.on_close(channel->user_data, channel);
    }
    return MRTC_STATUS_OK;
}
