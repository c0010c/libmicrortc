#include "../../src/dtls/dtls_session.h"
#include "../../src/srtp/srtp_session.h"

#include <stdio.h>
#include <string.h>

typedef struct AppDataState {
    unsigned char data[64];
    size_t data_len;
} AppDataState;

static int all_zero(const unsigned char *buffer, size_t len)
{
    size_t i;

    for (i = 0; i < len; ++i) {
        if (buffer[i] != 0) {
            return 0;
        }
    }
    return 1;
}

static void on_app_data(void *user_data, const uint8_t *data, size_t data_len)
{
    AppDataState *state = (AppDataState *) user_data;

    if (data_len <= sizeof(state->data)) {
        memcpy(state->data, data, data_len);
        state->data_len = data_len;
    }
}

static int pump_dtls(MRTC_DTLS_SESSION *left, MRTC_DTLS_SESSION *right)
{
    unsigned char packet[4096];
    size_t packet_len = 0;
    int progress = 0;

    do {
        progress = 0;
        do {
            packet_len = 0;
            if (mrtc_dtls_session_drain_outbound_packet(left, packet, sizeof(packet), &packet_len) != MRTC_STATUS_OK) {
                return 0;
            }
            if (packet_len > 0u) {
                if (mrtc_dtls_session_handle_inbound_packet(right, packet, packet_len) != MRTC_STATUS_OK) {
                    return 0;
                }
                progress = 1;
            }
        } while (packet_len > 0u);

        do {
            packet_len = 0;
            if (mrtc_dtls_session_drain_outbound_packet(right, packet, sizeof(packet), &packet_len) != MRTC_STATUS_OK) {
                return 0;
            }
            if (packet_len > 0u) {
                if (mrtc_dtls_session_handle_inbound_packet(left, packet, packet_len) != MRTC_STATUS_OK) {
                    return 0;
                }
                progress = 1;
            }
        } while (packet_len > 0u);
    } while (progress &&
             (!mrtc_dtls_session_is_connected(left) || !mrtc_dtls_session_is_connected(right)));

    return mrtc_dtls_session_is_connected(left) && mrtc_dtls_session_is_connected(right);
}

static int handshake_pair(MRTC_DTLS_SESSION *client, MRTC_DTLS_SESSION *server, const char *fingerprint)
{
    if (mrtc_dtls_session_init(client, MRTC_DTLS_ROLE_CLIENT, fingerprint) != MRTC_STATUS_OK ||
        mrtc_dtls_session_init(server, MRTC_DTLS_ROLE_SERVER, fingerprint) != MRTC_STATUS_OK ||
        mrtc_dtls_session_set_remote_fingerprint(client, fingerprint) != MRTC_STATUS_OK ||
        mrtc_dtls_session_set_remote_fingerprint(server, fingerprint) != MRTC_STATUS_OK) {
        return 0;
    }
    if (mrtc_dtls_session_export_srtp_keying_material(client, &(MRTC_DTLS_KEYING_MATERIAL){0}) != MRTC_STATUS_INVALID_STATE) {
        return 0;
    }
    if (mrtc_dtls_session_start(server) != MRTC_STATUS_OK ||
        mrtc_dtls_session_start(client) != MRTC_STATUS_OK) {
        return 0;
    }
    return pump_dtls(client, server);
}

static int mismatch_fails(const char *fingerprint)
{
    const char *bad_fingerprint = "00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00";
    MRTC_DTLS_SESSION client;
    MRTC_DTLS_SESSION server;
    unsigned char packet[4096];
    size_t packet_len = 0;
    int i;

    if (mrtc_dtls_session_init(&client, MRTC_DTLS_ROLE_CLIENT, fingerprint) != MRTC_STATUS_OK ||
        mrtc_dtls_session_init(&server, MRTC_DTLS_ROLE_SERVER, fingerprint) != MRTC_STATUS_OK ||
        mrtc_dtls_session_set_remote_fingerprint(&client, bad_fingerprint) != MRTC_STATUS_OK ||
        mrtc_dtls_session_set_remote_fingerprint(&server, fingerprint) != MRTC_STATUS_OK ||
        mrtc_dtls_session_start(server.role == MRTC_DTLS_ROLE_SERVER ? &server : &client) != MRTC_STATUS_OK ||
        mrtc_dtls_session_start(&client) != MRTC_STATUS_OK) {
        mrtc_dtls_session_deinit(&client);
        mrtc_dtls_session_deinit(&server);
        return 0;
    }

    for (i = 0; i < 16; ++i) {
        packet_len = 0;
        if (mrtc_dtls_session_drain_outbound_packet(&client, packet, sizeof(packet), &packet_len) == MRTC_STATUS_OK &&
            packet_len > 0u) {
            (void) mrtc_dtls_session_handle_inbound_packet(&server, packet, packet_len);
        }
        packet_len = 0;
        if (mrtc_dtls_session_drain_outbound_packet(&server, packet, sizeof(packet), &packet_len) == MRTC_STATUS_OK &&
            packet_len > 0u) {
            (void) mrtc_dtls_session_handle_inbound_packet(&client, packet, packet_len);
        }
        if (client.state == MRTC_DTLS_STATE_FAILED) {
            mrtc_dtls_session_deinit(&client);
            mrtc_dtls_session_deinit(&server);
            return 1;
        }
    }

    mrtc_dtls_session_deinit(&client);
    mrtc_dtls_session_deinit(&server);
    return 0;
}

int main(void)
{
    MRTC_DTLS_SESSION client;
    MRTC_DTLS_SESSION server;
    MRTC_DTLS_KEYING_MATERIAL client_keying_material;
    MRTC_DTLS_KEYING_MATERIAL server_keying_material;
    MRTC_SRTP_SESSION client_srtp;
    MRTC_SRTP_SESSION server_srtp;
    AppDataState app_state = {{0}, 0};
    unsigned char rtp_packet[128] = {0x80, 0x60, 0x12, 0x34, 0x00, 0x00, 0x00, 0x05,
                                     0x00, 0x00, 0x00, 0x07, 0x65, 0x88, 0x84};
    unsigned char rtcp_packet[128] = {0x80, 0xc8, 0x00, 0x06, 0x00, 0x00, 0x00, 0x07,
                                      0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02,
                                      0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04,
                                      0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x06};
    unsigned char original_rtp[sizeof(rtp_packet)];
    unsigned char original_rtcp[sizeof(rtcp_packet)];
    size_t rtp_size = 15;
    size_t rtcp_size = 32;
    size_t original_rtp_size = rtp_size;
    size_t original_rtcp_size = rtcp_size;
    char generated_fp[96];
    size_t generated_len = 0;

    memset(&client, 0, sizeof(client));
    memset(&server, 0, sizeof(server));
    memset(&client_srtp, 0, sizeof(client_srtp));
    memset(&server_srtp, 0, sizeof(server_srtp));

    if (mrtc_dtls_generate_fingerprint(generated_fp, sizeof(generated_fp), &generated_len) != MRTC_STATUS_OK ||
        generated_len < 90 ||
        generated_fp[2] != ':') {
        return 1;
    }
    if (!mismatch_fails(generated_fp)) {
        return 1;
    }
    if (!handshake_pair(&client, &server, generated_fp)) {
        return 1;
    }
    if (mrtc_dtls_session_verify_remote_fingerprint(&client, generated_fp) != MRTC_STATUS_OK ||
        mrtc_dtls_session_verify_remote_fingerprint(&server, generated_fp) != MRTC_STATUS_OK) {
        return 1;
    }
    if (mrtc_dtls_session_export_srtp_keying_material(&client, &client_keying_material) != MRTC_STATUS_OK ||
        mrtc_dtls_session_export_srtp_keying_material(&server, &server_keying_material) != MRTC_STATUS_OK ||
        all_zero(client_keying_material.client_write_key, sizeof(client_keying_material.client_write_key)) ||
        all_zero(client_keying_material.server_write_key, sizeof(client_keying_material.server_write_key)) ||
        memcmp(client_keying_material.client_write_key,
               server_keying_material.client_write_key,
               sizeof(client_keying_material.client_write_key)) != 0 ||
        strcmp(client_keying_material.profile, "SRTP_AES128_CM_SHA1_80") != 0) {
        return 1;
    }
    if (mrtc_dtls_session_set_application_data_callback(&server, on_app_data, &app_state) != MRTC_STATUS_OK ||
        mrtc_dtls_session_send_application_data(&client, (const uint8_t *) "hello", 5) != MRTC_STATUS_OK ||
        !pump_dtls(&client, &server) ||
        app_state.data_len != 5 ||
        memcmp(app_state.data, "hello", 5) != 0) {
        return 1;
    }
    if (mrtc_srtp_session_init_from_dtls(&client_srtp, &client_keying_material, MRTC_DTLS_ROLE_CLIENT) != MRTC_STATUS_OK ||
        mrtc_srtp_session_init_from_dtls(&server_srtp, &client_keying_material, MRTC_DTLS_ROLE_SERVER) != MRTC_STATUS_OK) {
        return 1;
    }
    if (mrtc_srtp_session_is_passthrough(&client_srtp)) {
        printf("strict SRTP expected libsrtp, got passthrough\n");
        return 1;
    }

    memcpy(original_rtp, rtp_packet, sizeof(rtp_packet));
    memcpy(original_rtcp, rtcp_packet, sizeof(rtcp_packet));
    if (mrtc_srtp_protect_rtp(&client_srtp, rtp_packet, sizeof(rtp_packet), &rtp_size) != MRTC_STATUS_OK ||
        rtp_size <= original_rtp_size ||
        mrtc_srtp_unprotect_rtp(&server_srtp, rtp_packet, &rtp_size) != MRTC_STATUS_OK ||
        rtp_size != original_rtp_size ||
        memcmp(rtp_packet, original_rtp, original_rtp_size) != 0) {
        return 1;
    }
    if (mrtc_srtp_protect_rtcp(&client_srtp, rtcp_packet, sizeof(rtcp_packet), &rtcp_size) != MRTC_STATUS_OK ||
        rtcp_size <= original_rtcp_size ||
        mrtc_srtp_unprotect_rtcp(&server_srtp, rtcp_packet, &rtcp_size) != MRTC_STATUS_OK ||
        rtcp_size != original_rtcp_size ||
        memcmp(rtcp_packet, original_rtcp, original_rtcp_size) != 0) {
        return 1;
    }
    printf("DTLS packet BIO handshake and libsrtp protect/unprotect active\n");
    mrtc_srtp_session_deinit(&client_srtp);
    mrtc_srtp_session_deinit(&server_srtp);
    mrtc_dtls_session_deinit(&client);
    mrtc_dtls_session_deinit(&server);
    return 0;
}
