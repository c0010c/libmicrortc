#include "../../src/dtls/dtls_session.h"
#include "../../src/ice/ice_agent.h"
#include "../../src/sctp/sctp_session.h"
#include "../../src/stun/stun_message.h"
#include "../../src/turn/turn_client.h"

#include <stdint.h>
#include <string.h>

int main(void)
{
    uint8_t stun[20];
    uint8_t txn[MRTC_STUN_TRANSACTION_ID_LEN];
    size_t len = 0;
    char candidate[128];
    char fingerprint[128];
    MRTC_ICE_CANDIDATE parsed_candidate;
    MRTC_ICE_SERVER_URL parsed_url;
    MRTC_SCTP_SESSION sctp;

    if (mrtc_stun_write_binding_request(stun, sizeof(stun), &len, txn) != MRTC_STATUS_OK || len != 20) {
        return 1;
    }
    if (mrtc_stun_validate_header(stun, len) != MRTC_STATUS_OK) {
        return 1;
    }
    if (mrtc_ice_format_host_candidate(candidate, sizeof(candidate), &len) != MRTC_STATUS_OK) {
        return 1;
    }
    if (mrtc_ice_parse_candidate(candidate, &parsed_candidate) != MRTC_STATUS_OK || strcmp(parsed_candidate.type, "host") != 0) {
        return 1;
    }
    if (mrtc_ice_server_url_parse("turn:example.test:3478?transport=udp", &parsed_url) != MRTC_STATUS_OK) {
        return 1;
    }
    if (strcmp(parsed_url.host, "example.test") != 0 || parsed_url.port != 3478 || strcmp(parsed_url.transport, "udp") != 0) {
        return 1;
    }
    if (mrtc_dtls_generate_fingerprint(fingerprint, sizeof(fingerprint), &len) != MRTC_STATUS_OK || len == 0) {
        return 1;
    }
    if (mrtc_dtls_role_from_remote_setup("active") != MRTC_DTLS_ROLE_SERVER) {
        return 1;
    }
    if (mrtc_sctp_session_init(&sctp) != MRTC_STATUS_OK) {
        return 1;
    }
    mrtc_sctp_session_deinit(&sctp);
    return 0;
}
