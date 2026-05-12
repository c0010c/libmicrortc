#include <micrortc/micrortc.h>

#include "sdp.h"

#include <stdlib.h>
#include <string.h>

typedef enum MRTC_PEER_CONNECTION_STATE {
    MRTC_PEER_CONNECTION_STATE_NEW = 0,
    MRTC_PEER_CONNECTION_STATE_REMOTE_SET = 1,
    MRTC_PEER_CONNECTION_STATE_LOCAL_SET = 2
} MRTC_PEER_CONNECTION_STATE;

struct MRTC_PEER_CONNECTION {
    MRTC_PEER_CONNECTION_CONFIG config;
    MRTC_PEER_CONNECTION_CALLBACKS callbacks;
    void *user_data;
    char *remote_description_type;
    char *remote_description_sdp;
    char *local_description_type;
    char *local_description_sdp;
    char *last_remote_candidate;
    MRTC_PEER_CONNECTION_STATE state;
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

    created->config = *config;
    if (callbacks != 0) {
        created->callbacks = *callbacks;
    }
    created->user_data = user_data;
    created->state = MRTC_PEER_CONNECTION_STATE_NEW;

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
    mrtc_sdp_free(parsed);

    status = mrtc_replace_string(&peer_connection->remote_description_type, type);
    if (status != MRTC_STATUS_OK) {
        return status;
    }

    status = mrtc_replace_string(&peer_connection->remote_description_sdp, sdp);
    if (status != MRTC_STATUS_OK) {
        return status;
    }

    peer_connection->state = MRTC_PEER_CONNECTION_STATE_REMOTE_SET;
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

    if (peer_connection->state < MRTC_PEER_CONNECTION_STATE_REMOTE_SET || peer_connection->remote_description_sdp == 0) {
        return MRTC_STATUS_INVALID_STATE;
    }

    return mrtc_sdp_create_answer(peer_connection->remote_description_sdp, buffer, buffer_len, required_len);
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

    peer_connection->state = MRTC_PEER_CONNECTION_STATE_LOCAL_SET;
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
    if (peer_connection == 0 || candidate == 0 || candidate[0] == '\0') {
        return MRTC_STATUS_INVALID_ARG;
    }

    return mrtc_replace_string(&peer_connection->last_remote_candidate, candidate);
}
