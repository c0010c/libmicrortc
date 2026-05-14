#ifndef MRTC_TURN_CLIENT_H
#define MRTC_TURN_CLIENT_H

#include <micrortc/micrortc.h>

#include "../stun/stun_message.h"

#include <stddef.h>
#include <stdint.h>

typedef struct MRTC_ICE_SERVER_URL {
    char scheme[8];
    char host[256];
    unsigned short port;
    char transport[8];
} MRTC_ICE_SERVER_URL;

typedef struct MRTC_TURN_CLIENT {
    int socket_fd;
    char server_host[256];
    unsigned short server_port;
    char username[128];
    char password[128];
    char realm[128];
    char nonce[256];
    uint8_t long_term_key[MRTC_STUN_LONG_TERM_KEY_LEN];
    int has_long_term_key;
    MRTC_STUN_ADDRESS mapped_address;
    int has_mapped_address;
    MRTC_STUN_ADDRESS relay_address;
    int has_relay_address;
} MRTC_TURN_CLIENT;

MRTC_STATUS mrtc_ice_server_url_parse(const char *url, MRTC_ICE_SERVER_URL *parsed);
MRTC_STATUS mrtc_turn_client_init(MRTC_TURN_CLIENT *client,
                                  const char *server_host,
                                  unsigned short server_port,
                                  const char *username,
                                  const char *password);
void mrtc_turn_client_free(MRTC_TURN_CLIENT *client);
MRTC_STATUS mrtc_turn_client_allocate(MRTC_TURN_CLIENT *client);
MRTC_STATUS mrtc_turn_client_create_permission(MRTC_TURN_CLIENT *client,
                                               const char *peer_ip,
                                               unsigned short peer_port);
MRTC_STATUS mrtc_turn_client_send(MRTC_TURN_CLIENT *client,
                                  const char *peer_ip,
                                  unsigned short peer_port,
                                  const uint8_t *data,
                                  size_t data_len);
MRTC_STATUS mrtc_turn_client_poll_data(MRTC_TURN_CLIENT *client,
                                       int timeout_ms,
                                       MRTC_STUN_ADDRESS *peer_address,
                                       uint8_t *buffer,
                                       size_t buffer_len,
                                       size_t *received_len);
MRTC_STATUS mrtc_turn_client_get_relay_address(const MRTC_TURN_CLIENT *client,
                                               MRTC_STUN_ADDRESS *relay_address);
MRTC_STATUS mrtc_turn_client_get_mapped_address(const MRTC_TURN_CLIENT *client,
                                                MRTC_STUN_ADDRESS *mapped_address);

#endif /* MRTC_TURN_CLIENT_H */
