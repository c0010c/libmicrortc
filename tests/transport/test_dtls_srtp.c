#include "../../src/dtls/dtls_session.h"
#include "../../src/srtp/srtp_session.h"

#include <stdio.h>
#include <string.h>

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

int main(void)
{
    const char *client_fp = "AA:BB:CC:DD:EE:FF:00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF:00:11:22:33:44:55:66:77:88:99";
    const char *server_fp = "11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF:00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF:00";
    MRTC_DTLS_SESSION client;
    MRTC_DTLS_SESSION server;
    MRTC_DTLS_KEYING_MATERIAL keying_material;
    MRTC_SRTP_SESSION client_srtp;
    MRTC_SRTP_SESSION server_srtp;
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

    if (mrtc_dtls_generate_fingerprint(generated_fp, sizeof(generated_fp), &generated_len) != MRTC_STATUS_OK ||
        generated_len < 90 ||
        generated_fp[2] != ':') {
        return 1;
    }

    if (mrtc_dtls_session_init(&client, MRTC_DTLS_ROLE_CLIENT, client_fp) != MRTC_STATUS_OK ||
        mrtc_dtls_session_init(&server, MRTC_DTLS_ROLE_SERVER, server_fp) != MRTC_STATUS_OK) {
        return 1;
    }
    if (mrtc_dtls_session_pair_connect(&client, &server) != MRTC_STATUS_OK ||
        client.state != MRTC_DTLS_STATE_CONNECTED ||
        server.state != MRTC_DTLS_STATE_CONNECTED) {
        return 1;
    }
    if (mrtc_dtls_session_verify_remote_fingerprint(&client, server_fp) != MRTC_STATUS_OK ||
        mrtc_dtls_session_verify_remote_fingerprint(&client, client_fp) != MRTC_STATUS_INVALID_STATE) {
        return 1;
    }
    if (mrtc_dtls_session_export_srtp_keying_material(&client, &keying_material) != MRTC_STATUS_OK ||
        all_zero(keying_material.client_write_key, sizeof(keying_material.client_write_key)) ||
        all_zero(keying_material.server_write_key, sizeof(keying_material.server_write_key)) ||
        strcmp(keying_material.profile, "SRTP_AES128_CM_SHA1_80") != 0) {
        return 1;
    }
    if (mrtc_srtp_session_init_from_dtls(&client_srtp, &keying_material, MRTC_DTLS_ROLE_CLIENT) != MRTC_STATUS_OK ||
        mrtc_srtp_session_init_from_dtls(&server_srtp, &keying_material, MRTC_DTLS_ROLE_SERVER) != MRTC_STATUS_OK) {
        return 1;
    }
    if (memcmp(client_srtp.transmit_key, keying_material.client_write_key, sizeof(client_srtp.transmit_key)) != 0 ||
        memcmp(client_srtp.receive_key, keying_material.server_write_key, sizeof(client_srtp.receive_key)) != 0 ||
        memcmp(client_srtp.transmit_salt, keying_material.client_write_salt, sizeof(client_srtp.transmit_salt)) != 0 ||
        memcmp(client_srtp.receive_salt, keying_material.server_write_salt, sizeof(client_srtp.receive_salt)) != 0 ||
        memcmp(server_srtp.transmit_key, keying_material.server_write_key, sizeof(server_srtp.transmit_key)) != 0 ||
        memcmp(server_srtp.receive_key, keying_material.client_write_key, sizeof(server_srtp.receive_key)) != 0 ||
        memcmp(server_srtp.transmit_salt, keying_material.server_write_salt, sizeof(server_srtp.transmit_salt)) != 0 ||
        memcmp(server_srtp.receive_salt, keying_material.client_write_salt, sizeof(server_srtp.receive_salt)) != 0) {
        return 1;
    }

    memcpy(original_rtp, rtp_packet, sizeof(rtp_packet));
    memcpy(original_rtcp, rtcp_packet, sizeof(rtcp_packet));
    if (mrtc_srtp_protect_rtp(&client_srtp, rtp_packet, sizeof(rtp_packet), &rtp_size) != MRTC_STATUS_OK ||
        mrtc_srtp_unprotect_rtp(&server_srtp, rtp_packet, &rtp_size) != MRTC_STATUS_OK ||
        rtp_size != original_rtp_size ||
        memcmp(rtp_packet, original_rtp, original_rtp_size) != 0) {
        return 1;
    }
    if (mrtc_srtp_protect_rtcp(&client_srtp, rtcp_packet, sizeof(rtcp_packet), &rtcp_size) != MRTC_STATUS_OK ||
        mrtc_srtp_unprotect_rtcp(&server_srtp, rtcp_packet, &rtcp_size) != MRTC_STATUS_OK ||
        rtcp_size != original_rtcp_size ||
        memcmp(rtcp_packet, original_rtcp, original_rtcp_size) != 0) {
        return 1;
    }
    if (mrtc_srtp_session_is_passthrough(&client_srtp)) {
        printf("SRTP passthrough fallback active; RTP/RTCP round trip deterministic\n");
    } else {
        printf("SRTP libsrtp protect/unprotect active\n");
    }
    mrtc_srtp_session_deinit(&client_srtp);
    mrtc_srtp_session_deinit(&server_srtp);
    mrtc_dtls_session_deinit(&client);
    mrtc_dtls_session_deinit(&server);
    return 0;
}
