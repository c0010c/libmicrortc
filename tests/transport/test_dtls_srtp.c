#include "../../src/dtls/dtls_session.h"
#include "../../src/srtp/srtp_session.h"

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
        memcmp(server_srtp.transmit_key, keying_material.server_write_key, sizeof(server_srtp.transmit_key)) != 0 ||
        memcmp(server_srtp.receive_key, keying_material.client_write_key, sizeof(server_srtp.receive_key)) != 0) {
        return 1;
    }
    mrtc_srtp_session_deinit(&client_srtp);
    mrtc_srtp_session_deinit(&server_srtp);
    mrtc_dtls_session_deinit(&client);
    mrtc_dtls_session_deinit(&server);
    return 0;
}
