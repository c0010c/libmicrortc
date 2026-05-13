#include <micrortc/micrortc.h>

#include "common/mrtc_common.h"
#include "data_channel/data_channel.h"
#include "dtls/dtls_session.h"
#include "ice/ice_agent.h"
#include "media/media_transceiver.h"
#include "rtp/codecs/h264.h"
#include "rtp/codecs/opus.h"
#include "rtcp/retransmitter.h"
#include "rtcp/rtcp_packet.h"
#include "rtcp/rtp_rolling_buffer.h"
#include "rtp/rtp_packet.h"
#include "sdp.h"
#include "srtp/srtp_session.h"
#include "sctp/sctp_session.h"

#include <stdio.h>
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
    MRTC_ICE_HOST_ENDPOINT host_endpoint;
    char *remote_fingerprint;
    char *remote_setup;
    MRTC_DTLS_SESSION dtls_session;
    MRTC_SRTP_SESSION srtp_session;
    MRTC_SCTP_SESSION sctp_session;
    int selected_pair_ready;
    int dtls_started;
    int sctp_started;
    int remote_has_application;
    MRTC_PEER_CONNECTION_STATE state;
    int has_remote_description;
    int has_local_description;
    unsigned short next_stream_id;
    MRTC_DATA_CHANNEL_HANDLE data_channels;
    MRTC_RTP_TRANSCEIVER_HANDLE transceivers;
    MRTC_MEDIA_SEND_HOOK media_send_hook;
    void *media_send_hook_user_data;
    unsigned int next_audio_mid;
    unsigned int next_video_mid;
    uint32_t next_media_ssrc;
};

#define MRTC_MEDIA_RTP_MTU 1200u
#define MRTC_MEDIA_SRTP_TRAILER_CAPACITY 32u
#define MRTC_H264_CLOCK_RATE 90000u
#define MRTC_100NS_PER_SECOND_U64 10000000ull
#define MRTC_MAX_H264_RECEIVE_FRAME_SIZE (4u * 1024u * 1024u)

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

static MRTC_STATUS mrtc_peer_connection_start_secure_transport(MRTC_PEER_CONNECTION_HANDLE peer_connection)
{
    MRTC_DTLS_ROLE role;
    MRTC_DTLS_KEYING_MATERIAL keying_material;
    MRTC_STATUS status;

    if (peer_connection == 0 || !peer_connection->selected_pair_ready) {
        return MRTC_STATUS_INVALID_STATE;
    }
    if (peer_connection->dtls_started) {
        return MRTC_STATUS_OK;
    }
    if (peer_connection->remote_fingerprint == 0 || peer_connection->remote_setup == 0) {
        return MRTC_STATUS_INVALID_STATE;
    }

    role = mrtc_dtls_role_from_remote_setup(peer_connection->remote_setup);
    status = mrtc_dtls_session_init(&peer_connection->dtls_session, role, peer_connection->local_fingerprint);
    if (status != MRTC_STATUS_OK) {
        return status;
    }
    status = mrtc_dtls_session_set_verified_peer_fingerprint(&peer_connection->dtls_session, peer_connection->remote_fingerprint);
    if (status != MRTC_STATUS_OK) {
        return status;
    }
    status = mrtc_dtls_session_verify_remote_fingerprint(&peer_connection->dtls_session, peer_connection->remote_fingerprint);
    if (status != MRTC_STATUS_OK) {
        return status;
    }
    status = mrtc_dtls_session_export_srtp_keying_material(&peer_connection->dtls_session, &keying_material);
    if (status != MRTC_STATUS_OK) {
        return status;
    }
    status = mrtc_srtp_session_init_from_dtls(&peer_connection->srtp_session, &keying_material, role);
    if (status != MRTC_STATUS_OK) {
        return status;
    }
    if (peer_connection->data_channels != 0 || peer_connection->remote_has_application) {
        status = mrtc_sctp_session_init(&peer_connection->sctp_session);
        if (status != MRTC_STATUS_OK) {
            return status;
        }
        status = mrtc_sctp_session_connect(&peer_connection->sctp_session);
        if (status != MRTC_STATUS_OK) {
            return status;
        }
        peer_connection->sctp_started = 1;
        {
            MRTC_DATA_CHANNEL_HANDLE channel = peer_connection->data_channels;
            while (channel != 0) {
                mrtc_data_channel_mark_open(channel);
                channel = channel->next;
            }
        }
        if (peer_connection->data_channels == 0 && peer_connection->callbacks.on_data_channel != 0) {
            MRTC_DATA_CHANNEL_HANDLE remote_channel = mrtc_data_channel_alloc("remote", 0, 0, 0);
            if (remote_channel != 0) {
                remote_channel->stream_id = peer_connection->next_stream_id;
                peer_connection->next_stream_id = (unsigned short) (peer_connection->next_stream_id + 2);
                remote_channel->next = peer_connection->data_channels;
                peer_connection->data_channels = remote_channel;
                mrtc_data_channel_mark_open(remote_channel);
                peer_connection->callbacks.on_data_channel(peer_connection->user_data, remote_channel);
            }
        }
    }
    peer_connection->dtls_started = 1;
    return MRTC_STATUS_OK;
}

static uint32_t mrtc_video_rtp_timestamp_from_frame(const MRTC_FRAME *frame)
{
    if (frame == 0) {
        return 0;
    }
    return (uint32_t) ((frame->presentation_ts * (uint64_t) MRTC_H264_CLOCK_RATE) / MRTC_100NS_PER_SECOND_U64);
}

static uint64_t mrtc_video_frame_timestamp_from_rtp(uint32_t rtp_timestamp)
{
    return ((uint64_t) rtp_timestamp * MRTC_100NS_PER_SECOND_U64) / (uint64_t) MRTC_H264_CLOCK_RATE;
}

static uint64_t mrtc_opus_frame_timestamp_from_rtp(uint32_t rtp_timestamp)
{
    return ((uint64_t) rtp_timestamp * MRTC_100NS_PER_SECOND_U64) / (uint64_t) MRTC_OPUS_CLOCK_RATE;
}

static int mrtc_transceiver_can_receive(MRTC_RTP_TRANSCEIVER_HANDLE transceiver)
{
    return transceiver != 0 &&
           transceiver->direction != MRTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY &&
           transceiver->direction != MRTC_RTP_TRANSCEIVER_DIRECTION_INACTIVE;
}

static MRTC_RTP_TRANSCEIVER_HANDLE mrtc_peer_connection_find_media_receiver(MRTC_PEER_CONNECTION_HANDLE peer_connection,
                                                                            const MRTC_RTP_PACKET *packet)
{
    MRTC_RTP_TRANSCEIVER_HANDLE current;
    MRTC_RTP_TRANSCEIVER_HANDLE payload_match = 0;

    if (peer_connection == 0 || packet == 0) {
        return 0;
    }

    current = peer_connection->transceivers;
    while (current != 0) {
        if (!mrtc_transceiver_can_receive(current)) {
            current = current->next;
            continue;
        }
        if (current->remote_ssrc != 0u && current->remote_ssrc == packet->ssrc) {
            return current->payload_type == packet->payload_type ? current : 0;
        }
        if (current->remote_ssrc == 0u && current->payload_type == packet->payload_type && payload_match == 0) {
            payload_match = current;
        }
        current = current->next;
    }

    if (payload_match != 0) {
        payload_match->remote_ssrc = packet->ssrc;
    }
    return payload_match;
}

static MRTC_RTP_TRANSCEIVER_HANDLE mrtc_peer_connection_find_local_transceiver_by_ssrc(MRTC_PEER_CONNECTION_HANDLE peer_connection,
                                                                                       uint32_t ssrc)
{
    MRTC_RTP_TRANSCEIVER_HANDLE current;

    if (peer_connection == 0 || ssrc == 0u) {
        return 0;
    }
    current = peer_connection->transceivers;
    while (current != 0) {
        if (current->local_ssrc == ssrc) {
            return current;
        }
        current = current->next;
    }
    return 0;
}

static MRTC_RTP_TRANSCEIVER_HANDLE mrtc_peer_connection_find_remote_transceiver_by_ssrc(MRTC_PEER_CONNECTION_HANDLE peer_connection,
                                                                                        uint32_t ssrc)
{
    MRTC_RTP_TRANSCEIVER_HANDLE current;

    if (peer_connection == 0 || ssrc == 0u) {
        return 0;
    }
    current = peer_connection->transceivers;
    while (current != 0) {
        if (current->remote_ssrc == ssrc) {
            return current;
        }
        current = current->next;
    }
    return 0;
}

static MRTC_STATUS mrtc_transceiver_append_receive_frame(MRTC_RTP_TRANSCEIVER_HANDLE transceiver,
                                                        const uint8_t *data,
                                                        size_t data_size)
{
    size_t needed;
    uint8_t *grown;

    if (transceiver == 0 || data == 0 || data_size == 0u) {
        return MRTC_STATUS_INVALID_ARG;
    }
    if (data_size > MRTC_MAX_H264_RECEIVE_FRAME_SIZE ||
        transceiver->receive_frame_size > MRTC_MAX_H264_RECEIVE_FRAME_SIZE - data_size) {
        transceiver->receive_frame_size = 0;
        return MRTC_STATUS_INVALID_STATE;
    }

    needed = transceiver->receive_frame_size + data_size;
    if (needed > transceiver->receive_frame_capacity) {
        size_t capacity = transceiver->receive_frame_capacity == 0u ? 1024u : transceiver->receive_frame_capacity;
        while (capacity < needed) {
            if (capacity > MRTC_MAX_H264_RECEIVE_FRAME_SIZE / 2u) {
                capacity = MRTC_MAX_H264_RECEIVE_FRAME_SIZE;
                break;
            }
            capacity *= 2u;
        }
        if (capacity < needed) {
            transceiver->receive_frame_size = 0;
            return MRTC_STATUS_INVALID_STATE;
        }
        grown = (uint8_t *) realloc(transceiver->receive_frame_buffer, capacity);
        if (grown == 0) {
            return MRTC_STATUS_INVALID_ARG;
        }
        transceiver->receive_frame_buffer = grown;
        transceiver->receive_frame_capacity = capacity;
    }
    memcpy(transceiver->receive_frame_buffer + transceiver->receive_frame_size, data, data_size);
    transceiver->receive_frame_size += data_size;
    return MRTC_STATUS_OK;
}

static uint32_t mrtc_h264_frame_flags_from_annexb(const uint8_t *data, size_t size)
{
    size_t i;

    if (data == 0 || size < MRTC_H264_START_CODE_SIZE + 1u) {
        return MRTC_FRAME_FLAG_NONE;
    }
    for (i = 0; i + MRTC_H264_START_CODE_SIZE < size; ++i) {
        if (data[i] == 0x00 && data[i + 1u] == 0x00 && data[i + 2u] == 0x00 && data[i + 3u] == 0x01) {
            uint8_t nalu_type = data[i + MRTC_H264_START_CODE_SIZE] & MRTC_H264_NAL_TYPE_MASK;
            if (nalu_type == 5u) {
                return MRTC_FRAME_FLAG_KEY_FRAME;
            }
        }
    }
    return MRTC_FRAME_FLAG_NONE;
}

static MRTC_STATUS mrtc_transceiver_deliver_frame(MRTC_RTP_TRANSCEIVER_HANDLE transceiver,
                                                  const uint8_t *data,
                                                  size_t data_size,
                                                  uint64_t presentation_ts,
                                                  uint32_t flags)
{
    MRTC_FRAME frame;

    if (transceiver == 0 || data == 0 || data_size == 0u) {
        return MRTC_STATUS_INVALID_ARG;
    }

    memset(&frame, 0, sizeof(frame));
    frame.data = data;
    frame.size = data_size;
    frame.presentation_ts = presentation_ts;
    frame.decoding_ts = presentation_ts;
    frame.index = transceiver->frames_received;
    frame.flags = flags;

    if (transceiver->callbacks.on_frame != 0) {
        transceiver->callbacks.on_frame(transceiver->user_data, transceiver, &frame);
    }
    transceiver->frames_received++;
    return MRTC_STATUS_OK;
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
    created->next_media_ssrc = 1000001u;
    mrtc_ice_host_endpoint_init(&created->host_endpoint);
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
    mrtc_sctp_session_deinit(&peer_connection->sctp_session);
    mrtc_srtp_session_deinit(&peer_connection->srtp_session);
    mrtc_dtls_session_deinit(&peer_connection->dtls_session);
    mrtc_ice_host_endpoint_close(&peer_connection->host_endpoint);
    while (peer_connection->data_channels != 0) {
        MRTC_DATA_CHANNEL_HANDLE next = peer_connection->data_channels->next;
        mrtc_data_channel_free_internal(peer_connection->data_channels);
        peer_connection->data_channels = next;
    }
    while (peer_connection->transceivers != 0) {
        MRTC_RTP_TRANSCEIVER_HANDLE next = peer_connection->transceivers->next;
        peer_connection->transceivers->owner = 0;
        mrtc_media_transceiver_free_internal(peer_connection->transceivers);
        peer_connection->transceivers = next;
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
    peer_connection->remote_has_application = mrtc_sdp_has_application(parsed);
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

    return mrtc_sdp_create_answer_with_media(peer_connection->remote_description_sdp,
                                             peer_connection->data_channels != 0,
                                             peer_connection->transceivers,
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
        if (mrtc_ice_host_endpoint_bind(&peer_connection->host_endpoint) == MRTC_STATUS_OK &&
            mrtc_ice_format_host_endpoint_candidate(&peer_connection->host_endpoint,
                                                    candidate,
                                                    sizeof(candidate),
                                                    &candidate_len) == MRTC_STATUS_OK) {
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
    if (peer_connection == 0 || required_len == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }

    if (peer_connection->transceivers == 0) {
        *required_len = 0;
        return MRTC_STATUS_NOT_IMPLEMENTED;
    }

    return mrtc_sdp_create_offer_with_media(peer_connection->data_channels != 0,
                                            peer_connection->transceivers,
                                            peer_connection->local_ice_ufrag,
                                            peer_connection->local_ice_pwd,
                                            peer_connection->local_fingerprint,
                                            buffer,
                                            buffer_len,
                                            required_len);
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
        peer_connection->selected_pair_ready = 1;
        if (mrtc_peer_connection_start_secure_transport(peer_connection) == MRTC_STATUS_OK) {
            mrtc_peer_connection_set_state(peer_connection, MRTC_PEER_CONNECTION_STATE_CONNECTED);
        }
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
    if (peer_connection->sctp_started) {
        mrtc_data_channel_mark_open(created);
    }
    *channel = created;
    return MRTC_STATUS_OK;
}

MRTC_STATUS mrtc_peer_connection_add_transceiver(MRTC_PEER_CONNECTION_HANDLE peer_connection,
                                                 const MRTC_TRANSCEIVER_INIT *init,
                                                 void *user_data,
                                                 MRTC_RTP_TRANSCEIVER_HANDLE *transceiver)
{
    char mid[16];
    unsigned char payload_type;
    MRTC_RTP_TRANSCEIVER_HANDLE created;

    if (peer_connection == 0 || init == 0 || transceiver == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }
    if (!mrtc_media_transceiver_kind_codec_valid(init->kind, init->codec) ||
        !mrtc_media_transceiver_direction_valid(init->direction)) {
        return MRTC_STATUS_INVALID_ARG;
    }

    if (init->kind == MRTC_MEDIA_KIND_AUDIO) {
        (void) snprintf(mid, sizeof(mid), "audio%u", peer_connection->next_audio_mid++);
        payload_type = 111;
    } else {
        (void) snprintf(mid, sizeof(mid), "video%u", peer_connection->next_video_mid++);
        payload_type = 96;
    }

    created = mrtc_media_transceiver_alloc(peer_connection,
                                           init,
                                           user_data,
                                           mid,
                                           peer_connection->next_media_ssrc++,
                                           payload_type);
    if (created == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }

    if (peer_connection->transceivers == 0) {
        peer_connection->transceivers = created;
    } else {
        MRTC_RTP_TRANSCEIVER_HANDLE tail = peer_connection->transceivers;
        while (tail->next != 0) {
            tail = tail->next;
        }
        tail->next = created;
    }

    *transceiver = created;
    return MRTC_STATUS_OK;
}

MRTC_STATUS mrtc_transceiver_set_callbacks(MRTC_RTP_TRANSCEIVER_HANDLE transceiver,
                                           const MRTC_TRANSCEIVER_CALLBACKS *callbacks,
                                           void *user_data)
{
    if (transceiver == 0 || callbacks == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }

    transceiver->callbacks = *callbacks;
    transceiver->user_data = user_data;
    return MRTC_STATUS_OK;
}

MRTC_STATUS mrtc_transceiver_on_frame(MRTC_RTP_TRANSCEIVER_HANDLE transceiver,
                                      void (*on_frame)(void *user_data,
                                                       MRTC_RTP_TRANSCEIVER_HANDLE transceiver,
                                                       const MRTC_FRAME *frame),
                                      void *user_data)
{
    if (transceiver == 0 || on_frame == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }

    transceiver->callbacks.on_frame = on_frame;
    transceiver->user_data = user_data;
    return MRTC_STATUS_OK;
}

MRTC_STATUS mrtc_transceiver_on_picture_loss(MRTC_RTP_TRANSCEIVER_HANDLE transceiver,
                                             void (*on_picture_loss)(void *user_data,
                                                                     MRTC_RTP_TRANSCEIVER_HANDLE transceiver),
                                             void *user_data)
{
    if (transceiver == 0 || on_picture_loss == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }

    transceiver->callbacks.on_picture_loss = on_picture_loss;
    transceiver->user_data = user_data;
    return MRTC_STATUS_OK;
}

MRTC_STATUS mrtc_transceiver_write_frame(MRTC_RTP_TRANSCEIVER_HANDLE transceiver,
                                         const MRTC_FRAME *frame)
{
    MRTC_PEER_CONNECTION_HANDLE owner;
    uint8_t *payloads = 0;
    size_t payloads_size = 0;
    size_t *payload_lengths = 0;
    size_t payload_count = 0;
    size_t payload_offset = 0;
    uint32_t rtp_timestamp;
    uint16_t sequence_number;
    size_t i;
    MRTC_STATUS status;

    if (transceiver == 0 || frame == 0 || (frame->data == 0 && frame->size > 0) || frame->size == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }
    if (transceiver->direction == MRTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY ||
        transceiver->direction == MRTC_RTP_TRANSCEIVER_DIRECTION_INACTIVE) {
        return MRTC_STATUS_INVALID_STATE;
    }
    if (transceiver->owner == 0 || !transceiver->owner->srtp_session.ready) {
        return MRTC_STATUS_INVALID_STATE;
    }
    owner = transceiver->owner;
    if (!owner->has_remote_description || !owner->has_local_description || !owner->selected_pair_ready ||
        owner->media_send_hook == 0) {
        return MRTC_STATUS_INVALID_STATE;
    }

    if (transceiver->codec == MRTC_CODEC_H264_PROFILE_42E01F_PACKETIZATION_MODE_1) {
        status = mrtc_h264_packetize_annexb(frame->data,
                                            frame->size,
                                            MRTC_MEDIA_RTP_MTU,
                                            0,
                                            &payloads_size,
                                            0,
                                            &payload_count);
        if (status != MRTC_STATUS_OK) {
            return status;
        }
        payloads = (uint8_t *) malloc(payloads_size);
        payload_lengths = (size_t *) calloc(payload_count, sizeof(*payload_lengths));
        if (payloads == 0 || payload_lengths == 0) {
            free(payloads);
            free(payload_lengths);
            return MRTC_STATUS_INVALID_ARG;
        }
        status = mrtc_h264_packetize_annexb(frame->data,
                                            frame->size,
                                            MRTC_MEDIA_RTP_MTU,
                                            payloads,
                                            &payloads_size,
                                            payload_lengths,
                                            &payload_count);
        rtp_timestamp = mrtc_video_rtp_timestamp_from_frame(frame);
    } else if (transceiver->codec == MRTC_CODEC_OPUS) {
        payload_count = 1u;
        status = mrtc_opus_payload_frame(frame, 0, &payloads_size);
        if (status != MRTC_STATUS_OK) {
            return status;
        }
        payloads = (uint8_t *) malloc(payloads_size);
        payload_lengths = (size_t *) calloc(1u, sizeof(*payload_lengths));
        if (payloads == 0 || payload_lengths == 0) {
            free(payloads);
            free(payload_lengths);
            return MRTC_STATUS_INVALID_ARG;
        }
        payload_lengths[0] = payloads_size;
        status = mrtc_opus_payload_frame(frame, payloads, &payloads_size);
        rtp_timestamp = mrtc_opus_rtp_timestamp_from_frame(frame);
    } else {
        return MRTC_STATUS_INVALID_ARG;
    }
    if (status != MRTC_STATUS_OK) {
        free(payloads);
        free(payload_lengths);
        return status;
    }

    sequence_number = transceiver->sequence_number;
    for (i = 0; i < payload_count; ++i) {
        MRTC_RTP_PACKET rtp_packet;
        size_t raw_capacity = MRTC_RTP_FIXED_HEADER_SIZE + payload_lengths[i] + MRTC_MEDIA_SRTP_TRAILER_CAPACITY;
        uint8_t *raw = (uint8_t *) malloc(raw_capacity);
        size_t raw_size = 0;
        size_t protected_size;

        if (raw == 0) {
            free(payloads);
            free(payload_lengths);
            return MRTC_STATUS_INVALID_ARG;
        }
        status = mrtc_rtp_packet_build(&rtp_packet,
                                       (uint8_t) (i + 1u == payload_count),
                                       transceiver->payload_type,
                                       sequence_number,
                                       rtp_timestamp,
                                       transceiver->local_ssrc,
                                       payloads + payload_offset,
                                       payload_lengths[i]);
        if (status == MRTC_STATUS_OK) {
            status = mrtc_rtp_packet_serialize(&rtp_packet, raw, raw_capacity, &raw_size);
        }
        protected_size = raw_size;
        if (status == MRTC_STATUS_OK) {
            status = mrtc_srtp_protect_rtp(&owner->srtp_session, raw, raw_capacity, &protected_size);
        }
        if (status == MRTC_STATUS_OK) {
            status = mrtc_peer_connection_send_protected_media_packet(owner, transceiver, raw, protected_size);
        }
        if (status != MRTC_STATUS_OK) {
            free(raw);
            free(payloads);
            free(payload_lengths);
            return status;
        }

        transceiver->rtp_packets_sent++;
        transceiver->rtp_octets_sent += payload_lengths[i];
        transceiver->last_rtp_timestamp = rtp_timestamp;

        /*
         * Keep the exact protected packet bytes that were sent. NACK retransmit
         * can resend these bytes directly through the media send hook without
         * re-running SRTP protect for an old RTP sequence number.
         */
        status = mrtc_rtp_rolling_buffer_add(transceiver->rtp_rolling_buffer,
                                             sequence_number,
                                             raw,
                                             protected_size);
        if (status != MRTC_STATUS_OK) {
            free(raw);
            free(payloads);
            free(payload_lengths);
            return status;
        }

        free(raw);
        sequence_number = mrtc_rtp_sequence_next(sequence_number);
        transceiver->sequence_number = sequence_number;
        payload_offset += payload_lengths[i];
    }

    transceiver->frames_sent++;
    free(payloads);
    free(payload_lengths);
    return MRTC_STATUS_OK;
}

static MRTC_STATUS mrtc_peer_connection_handle_pli(MRTC_PEER_CONNECTION_HANDLE peer_connection,
                                                   const uint8_t *rtcp_packet,
                                                   size_t rtcp_packet_size)
{
    uint32_t sender_ssrc = 0;
    uint32_t media_ssrc = 0;
    MRTC_RTP_TRANSCEIVER_HANDLE transceiver;
    MRTC_STATUS status;

    (void) sender_ssrc;
    status = mrtc_rtcp_parse_pli(rtcp_packet, rtcp_packet_size, &sender_ssrc, &media_ssrc);
    if (status != MRTC_STATUS_OK) {
        return status;
    }

    transceiver = mrtc_peer_connection_find_local_transceiver_by_ssrc(peer_connection, media_ssrc);
    if (transceiver == 0 || transceiver->kind != MRTC_MEDIA_KIND_VIDEO) {
        return MRTC_STATUS_INVALID_STATE;
    }
    transceiver->picture_loss_count++;
    if (transceiver->callbacks.on_picture_loss != 0) {
        transceiver->callbacks.on_picture_loss(transceiver->user_data, transceiver);
    }
    return MRTC_STATUS_OK;
}

static MRTC_STATUS mrtc_peer_connection_handle_sender_report(MRTC_PEER_CONNECTION_HANDLE peer_connection,
                                                             const uint8_t *rtcp_packet,
                                                             size_t rtcp_packet_size)
{
    MRTC_RTCP_SENDER_REPORT report;
    MRTC_RTP_TRANSCEIVER_HANDLE transceiver;
    MRTC_STATUS status;

    status = mrtc_rtcp_parse_sender_report(rtcp_packet, rtcp_packet_size, &report);
    if (status != MRTC_STATUS_OK) {
        return status;
    }
    transceiver = mrtc_peer_connection_find_remote_transceiver_by_ssrc(peer_connection, report.sender_ssrc);
    if (transceiver == 0) {
        return MRTC_STATUS_INVALID_STATE;
    }
    transceiver->sender_reports_received++;
    return MRTC_STATUS_OK;
}

static MRTC_STATUS mrtc_peer_connection_handle_receiver_report(MRTC_PEER_CONNECTION_HANDLE peer_connection,
                                                               const uint8_t *rtcp_packet,
                                                               size_t rtcp_packet_size)
{
    MRTC_RTCP_RECEIVER_REPORT report;
    MRTC_RTP_TRANSCEIVER_HANDLE transceiver;
    MRTC_STATUS status;

    status = mrtc_rtcp_parse_receiver_report(rtcp_packet, rtcp_packet_size, &report);
    if (status != MRTC_STATUS_OK) {
        return status;
    }
    transceiver = mrtc_peer_connection_find_local_transceiver_by_ssrc(peer_connection, report.report_ssrc);
    if (transceiver == 0) {
        return MRTC_STATUS_INVALID_STATE;
    }
    transceiver->receiver_reports_received++;
    transceiver->last_receiver_fraction_lost = report.fraction_lost;
    transceiver->last_receiver_cumulative_lost = report.cumulative_lost;
    transceiver->last_receiver_highest_sequence_number = report.highest_sequence_number;
    transceiver->last_receiver_jitter = report.jitter;
    return MRTC_STATUS_OK;
}

MRTC_STATUS mrtc_peer_connection_set_media_send_hook(MRTC_PEER_CONNECTION_HANDLE peer_connection,
                                                     MRTC_MEDIA_SEND_HOOK hook,
                                                     void *user_data)
{
    if (peer_connection == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }
    peer_connection->media_send_hook = hook;
    peer_connection->media_send_hook_user_data = user_data;
    return MRTC_STATUS_OK;
}

int mrtc_peer_connection_media_is_srtp_passthrough(MRTC_PEER_CONNECTION_HANDLE peer_connection)
{
    return peer_connection != 0 && mrtc_srtp_session_is_passthrough(&peer_connection->srtp_session);
}

MRTC_STATUS mrtc_peer_connection_send_protected_media_packet(MRTC_PEER_CONNECTION_HANDLE peer_connection,
                                                            MRTC_RTP_TRANSCEIVER_HANDLE transceiver,
                                                            const uint8_t *packet,
                                                            size_t packet_size)
{
    if (peer_connection == 0 || transceiver == 0 || packet == 0 || packet_size == 0u) {
        return MRTC_STATUS_INVALID_ARG;
    }
    if (peer_connection->media_send_hook == 0) {
        return MRTC_STATUS_INVALID_STATE;
    }
    return peer_connection->media_send_hook(peer_connection->media_send_hook_user_data,
                                            peer_connection,
                                            transceiver,
                                            packet,
                                            packet_size);
}

MRTC_STATUS mrtc_peer_connection_receive_protected_media_packet(MRTC_PEER_CONNECTION_HANDLE peer_connection,
                                                               const uint8_t *packet,
                                                               size_t packet_size)
{
    uint8_t *mutable_packet;
    size_t unprotected_size;
    MRTC_RTP_PACKET rtp_packet;
    MRTC_RTP_TRANSCEIVER_HANDLE transceiver;
    MRTC_STATUS status;

    if (peer_connection == 0 || packet == 0 || packet_size == 0u) {
        return MRTC_STATUS_INVALID_ARG;
    }
    if (!peer_connection->srtp_session.ready) {
        return MRTC_STATUS_INVALID_STATE;
    }

    mutable_packet = (uint8_t *) malloc(packet_size);
    if (mutable_packet == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }
    memcpy(mutable_packet, packet, packet_size);
    unprotected_size = packet_size;

    status = mrtc_srtp_unprotect_rtp(&peer_connection->srtp_session, mutable_packet, &unprotected_size);
    if (status == MRTC_STATUS_OK) {
        status = mrtc_rtp_packet_parse(mutable_packet, unprotected_size, &rtp_packet);
    }
    if (status != MRTC_STATUS_OK) {
        free(mutable_packet);
        return status;
    }

    transceiver = mrtc_peer_connection_find_media_receiver(peer_connection, &rtp_packet);
    if (transceiver == 0 ||
        transceiver->direction == MRTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY ||
        transceiver->direction == MRTC_RTP_TRANSCEIVER_DIRECTION_INACTIVE) {
        free(mutable_packet);
        return MRTC_STATUS_INVALID_STATE;
    }

    if (transceiver->codec == MRTC_CODEC_OPUS) {
        uint8_t *frame_data;
        size_t frame_size = 0;

        status = mrtc_opus_depayload(rtp_packet.payload, rtp_packet.payload_size, 0, &frame_size);
        if (status != MRTC_STATUS_OK) {
            free(mutable_packet);
            return status;
        }
        frame_data = (uint8_t *) malloc(frame_size);
        if (frame_data == 0) {
            free(mutable_packet);
            return MRTC_STATUS_INVALID_ARG;
        }
        status = mrtc_opus_depayload(rtp_packet.payload, rtp_packet.payload_size, frame_data, &frame_size);
        if (status == MRTC_STATUS_OK) {
            status = mrtc_transceiver_deliver_frame(transceiver,
                                                    frame_data,
                                                    frame_size,
                                                    mrtc_opus_frame_timestamp_from_rtp(rtp_packet.timestamp),
                                                    MRTC_FRAME_FLAG_NONE);
        }
        free(frame_data);
    } else if (transceiver->codec == MRTC_CODEC_H264_PROFILE_42E01F_PACKETIZATION_MODE_1) {
        uint8_t *annexb;
        size_t annexb_size = 0;
        int is_start = 0;

        status = mrtc_h264_depacketize_payload(rtp_packet.payload, rtp_packet.payload_size, 0, &annexb_size, &is_start);
        if (status != MRTC_STATUS_OK) {
            free(mutable_packet);
            return status;
        }
        annexb = (uint8_t *) malloc(annexb_size);
        if (annexb == 0) {
            free(mutable_packet);
            return MRTC_STATUS_INVALID_ARG;
        }
        status = mrtc_h264_depacketize_payload(rtp_packet.payload, rtp_packet.payload_size, annexb, &annexb_size, &is_start);
        if (status == MRTC_STATUS_OK) {
            if (is_start && (transceiver->receive_frame_size == 0u ||
                             transceiver->receive_frame_timestamp != rtp_packet.timestamp)) {
                transceiver->receive_frame_size = 0;
                transceiver->receive_frame_timestamp = rtp_packet.timestamp;
            } else if (transceiver->receive_frame_size == 0u ||
                       transceiver->receive_frame_timestamp != rtp_packet.timestamp) {
                status = MRTC_STATUS_PARSE_ERROR;
            }
        }
        if (status == MRTC_STATUS_OK) {
            status = mrtc_transceiver_append_receive_frame(transceiver, annexb, annexb_size);
        }
        if (status == MRTC_STATUS_OK && rtp_packet.marker) {
            status = mrtc_transceiver_deliver_frame(transceiver,
                                                    transceiver->receive_frame_buffer,
                                                    transceiver->receive_frame_size,
                                                    mrtc_video_frame_timestamp_from_rtp(rtp_packet.timestamp),
                                                    mrtc_h264_frame_flags_from_annexb(transceiver->receive_frame_buffer,
                                                                                      transceiver->receive_frame_size));
            transceiver->receive_frame_size = 0;
        }
        free(annexb);
    } else {
        status = MRTC_STATUS_INVALID_ARG;
    }

    free(mutable_packet);
    return status;
}

MRTC_STATUS mrtc_peer_connection_receive_protected_rtcp_packet(MRTC_PEER_CONNECTION_HANDLE peer_connection,
                                                              const uint8_t *packet,
                                                              size_t packet_size,
                                                              MRTC_RTCP_RETRANSMIT_RESULT *retransmit_result)
{
    uint8_t *mutable_packet;
    size_t unprotected_size;
    MRTC_RTCP_HEADER header;
    MRTC_STATUS status;

    if (peer_connection == 0 || packet == 0 || packet_size == 0u) {
        return MRTC_STATUS_INVALID_ARG;
    }
    if (!peer_connection->srtp_session.ready) {
        return MRTC_STATUS_INVALID_STATE;
    }
    if (retransmit_result != 0) {
        memset(retransmit_result, 0, sizeof(*retransmit_result));
    }

    mutable_packet = (uint8_t *) malloc(packet_size);
    if (mutable_packet == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }
    memcpy(mutable_packet, packet, packet_size);
    unprotected_size = packet_size;

    status = mrtc_srtp_unprotect_rtcp(&peer_connection->srtp_session, mutable_packet, &unprotected_size);
    if (status == MRTC_STATUS_OK) {
        status = mrtc_rtcp_parse_header(mutable_packet, unprotected_size, &header);
    }
    if (status == MRTC_STATUS_OK &&
        header.packet_type == MRTC_RTCP_TYPE_RTPFB &&
        header.count == MRTC_RTCP_FMT_NACK) {
        status = mrtc_rtcp_retransmit_nack(peer_connection, mutable_packet, header.packet_size, retransmit_result);
    } else if (status == MRTC_STATUS_OK &&
               header.packet_type == MRTC_RTCP_TYPE_PSFB &&
               header.count == MRTC_RTCP_FMT_PLI) {
        status = mrtc_peer_connection_handle_pli(peer_connection, mutable_packet, header.packet_size);
    } else if (status == MRTC_STATUS_OK && header.packet_type == MRTC_RTCP_TYPE_SR) {
        status = mrtc_peer_connection_handle_sender_report(peer_connection, mutable_packet, header.packet_size);
    } else if (status == MRTC_STATUS_OK && header.packet_type == MRTC_RTCP_TYPE_RR) {
        status = mrtc_peer_connection_handle_receiver_report(peer_connection, mutable_packet, header.packet_size);
    }

    free(mutable_packet);
    return status;
}

MRTC_RTP_TRANSCEIVER_HANDLE mrtc_peer_connection_get_transceivers(MRTC_PEER_CONNECTION_HANDLE peer_connection)
{
    return peer_connection == 0 ? 0 : peer_connection->transceivers;
}

void mrtc_transceiver_free(MRTC_RTP_TRANSCEIVER_HANDLE transceiver)
{
    MRTC_PEER_CONNECTION_HANDLE owner;

    if (transceiver == 0) {
        return;
    }

    owner = transceiver->owner;
    if (owner != 0) {
        MRTC_RTP_TRANSCEIVER_HANDLE *cursor = &owner->transceivers;
        while (*cursor != 0) {
            if (*cursor == transceiver) {
                *cursor = transceiver->next;
                transceiver->owner = 0;
                transceiver->next = 0;
                mrtc_media_transceiver_free_internal(transceiver);
                return;
            }
            cursor = &(*cursor)->next;
        }
    }

    mrtc_media_transceiver_free_internal(transceiver);
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
    if (channel == 0 || (data == 0 && data_len > 0)) {
        return MRTC_STATUS_INVALID_ARG;
    }
    if (!channel->open || channel->closed) {
        return MRTC_STATUS_INVALID_STATE;
    }
    if (message_type == MRTC_DATA_CHANNEL_MESSAGE_TYPE_TEXT && data_len == 4 && memcmp(data, "ping", 4) == 0) {
        static const unsigned char pong[] = {'p', 'o', 'n', 'g'};
        return mrtc_data_channel_deliver(channel, message_type, pong, sizeof(pong));
    }
    if (message_type == MRTC_DATA_CHANNEL_MESSAGE_TYPE_BINARY) {
        return mrtc_data_channel_deliver(channel, message_type, data, data_len);
    }
    return MRTC_STATUS_OK;
}

const char *mrtc_data_channel_label(MRTC_DATA_CHANNEL_HANDLE channel)
{
    return channel == 0 ? 0 : channel->label;
}

unsigned short mrtc_data_channel_id(MRTC_DATA_CHANNEL_HANDLE channel)
{
    return channel == 0 ? 0 : channel->stream_id;
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
