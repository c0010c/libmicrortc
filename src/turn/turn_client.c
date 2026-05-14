#define _POSIX_C_SOURCE 200112L

#include "turn_client.h"

#include <errno.h>
#include <netdb.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#define MRTC_TURN_PACKET_CAPACITY 2048u
#define MRTC_TURN_REQUEST_TIMEOUT_MS 500

MRTC_STATUS mrtc_ice_server_url_parse(const char *url, MRTC_ICE_SERVER_URL *parsed)
{
    const char *colon;
    const char *host_start;
    const char *port_start;
    const char *query;
    unsigned int port = 0;
    size_t scheme_len;
    size_t host_len;

    if (url == 0 || parsed == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }

    memset(parsed, 0, sizeof(*parsed));
    colon = strchr(url, ':');
    if (colon == 0) {
        return MRTC_STATUS_PARSE_ERROR;
    }

    scheme_len = (size_t) (colon - url);
    if (scheme_len == 0 || scheme_len >= sizeof(parsed->scheme)) {
        return MRTC_STATUS_PARSE_ERROR;
    }
    memcpy(parsed->scheme, url, scheme_len);
    parsed->scheme[scheme_len] = '\0';
    if (strcmp(parsed->scheme, "stun") != 0 && strcmp(parsed->scheme, "turn") != 0) {
        return MRTC_STATUS_PARSE_ERROR;
    }

    host_start = colon + 1;
    port_start = strrchr(host_start, ':');
    if (port_start == 0 || port_start == host_start) {
        return MRTC_STATUS_PARSE_ERROR;
    }
    query = strchr(port_start + 1, '?');

    host_len = (size_t) (port_start - host_start);
    if (host_len == 0 || host_len >= sizeof(parsed->host)) {
        return MRTC_STATUS_PARSE_ERROR;
    }
    memcpy(parsed->host, host_start, host_len);
    parsed->host[host_len] = '\0';

    if (sscanf(port_start + 1, "%u", &port) != 1 || port == 0 || port > 65535) {
        return MRTC_STATUS_PARSE_ERROR;
    }
    parsed->port = (unsigned short) port;

    strcpy(parsed->transport, "udp");
    if (query != 0) {
        const char *transport = strstr(query, "transport=");
        if (transport != 0) {
            transport += strlen("transport=");
            if (strncmp(transport, "udp", 3) != 0) {
                return MRTC_STATUS_PARSE_ERROR;
            }
        }
    }

    return MRTC_STATUS_OK;
}

static MRTC_STATUS mrtc_turn_copy_string(char *dst, size_t dst_len, const char *src, int required)
{
    size_t src_len;

    if (dst == 0 || dst_len == 0u || (src == 0 && required)) {
        return MRTC_STATUS_INVALID_ARG;
    }
    if (src == 0) {
        dst[0] = '\0';
        return MRTC_STATUS_OK;
    }
    src_len = strlen(src);
    if ((required && src_len == 0u) || src_len >= dst_len) {
        return MRTC_STATUS_INVALID_ARG;
    }
    memcpy(dst, src, src_len);
    dst[src_len] = '\0';
    return MRTC_STATUS_OK;
}

static MRTC_STATUS mrtc_turn_wait_readable(int fd, int timeout_ms)
{
    fd_set readfds;
    struct timeval timeout;
    int result;

    if (fd < 0) {
        return MRTC_STATUS_INVALID_STATE;
    }

    FD_ZERO(&readfds);
    FD_SET(fd, &readfds);
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;

    do {
        result = select(fd + 1, &readfds, 0, 0, &timeout);
    } while (result < 0 && errno == EINTR);

    if (result < 0) {
        return MRTC_STATUS_INVALID_STATE;
    }
    if (result == 0) {
        return MRTC_STATUS_PARSE_ERROR;
    }
    return MRTC_STATUS_OK;
}

static MRTC_STATUS mrtc_turn_send_packet(MRTC_TURN_CLIENT *client, const uint8_t *packet, size_t packet_len)
{
    ssize_t sent_len;

    if (client == 0 || client->socket_fd < 0 || packet == 0 || packet_len == 0u) {
        return MRTC_STATUS_INVALID_ARG;
    }

    do {
        sent_len = send(client->socket_fd, packet, packet_len, 0);
    } while (sent_len < 0 && errno == EINTR);

    return sent_len == (ssize_t) packet_len ? MRTC_STATUS_OK : MRTC_STATUS_INVALID_STATE;
}

static MRTC_STATUS mrtc_turn_recv_packet(MRTC_TURN_CLIENT *client,
                                         uint8_t *packet,
                                         size_t packet_capacity,
                                         size_t *packet_len,
                                         int timeout_ms)
{
    ssize_t recv_len;
    MRTC_STATUS status;

    if (client == 0 || client->socket_fd < 0 || packet == 0 || packet_len == 0 || packet_capacity == 0u) {
        return MRTC_STATUS_INVALID_ARG;
    }

    status = mrtc_turn_wait_readable(client->socket_fd, timeout_ms);
    if (status != MRTC_STATUS_OK) {
        return status;
    }

    do {
        recv_len = recv(client->socket_fd, packet, packet_capacity, 0);
    } while (recv_len < 0 && errno == EINTR);

    if (recv_len <= 0) {
        return MRTC_STATUS_INVALID_STATE;
    }
    *packet_len = (size_t) recv_len;
    return MRTC_STATUS_OK;
}

static MRTC_STATUS mrtc_turn_recv_allocate_response(MRTC_TURN_CLIENT *client,
                                                    const uint8_t transaction_id[MRTC_STUN_TRANSACTION_ID_LEN],
                                                    MRTC_STUN_MESSAGE *message)
{
    uint8_t packet[MRTC_TURN_PACKET_CAPACITY];
    size_t packet_len = 0u;
    MRTC_STATUS status;

    status = mrtc_turn_recv_packet(client,
                                   packet,
                                   sizeof(packet),
                                   &packet_len,
                                   MRTC_TURN_REQUEST_TIMEOUT_MS);
    if (status != MRTC_STATUS_OK) {
        return status;
    }
    return mrtc_stun_parse_turn_allocate_response(packet,
                                                  packet_len,
                                                  transaction_id,
                                                  client->has_long_term_key ? client->long_term_key : 0,
                                                  client->has_long_term_key ? MRTC_STUN_LONG_TERM_KEY_LEN : 0u,
                                                  message);
}

static MRTC_STATUS mrtc_turn_recv_response(MRTC_TURN_CLIENT *client,
                                           uint16_t expected_message_type,
                                           const uint8_t transaction_id[MRTC_STUN_TRANSACTION_ID_LEN])
{
    uint8_t packet[MRTC_TURN_PACKET_CAPACITY];
    size_t packet_len = 0u;
    MRTC_STUN_MESSAGE message;
    MRTC_STATUS status;

    status = mrtc_turn_recv_packet(client,
                                   packet,
                                   sizeof(packet),
                                   &packet_len,
                                   MRTC_TURN_REQUEST_TIMEOUT_MS);
    if (status != MRTC_STATUS_OK) {
        return status;
    }
    return mrtc_stun_parse_response(packet,
                                    packet_len,
                                    expected_message_type,
                                    transaction_id,
                                    client->has_long_term_key ? client->long_term_key : 0,
                                    client->has_long_term_key ? MRTC_STUN_LONG_TERM_KEY_LEN : 0u,
                                    &message);
}

MRTC_STATUS mrtc_turn_client_init(MRTC_TURN_CLIENT *client,
                                  const char *server_host,
                                  unsigned short server_port,
                                  const char *username,
                                  const char *password)
{
    struct addrinfo hints;
    struct addrinfo *result = 0;
    struct addrinfo *current;
    char port_text[16];
    int fd = -1;
    int gai_result;
    MRTC_STATUS status;

    if (client == 0 || server_host == 0 || server_host[0] == '\0' || server_port == 0u) {
        return MRTC_STATUS_INVALID_ARG;
    }

    memset(client, 0, sizeof(*client));
    client->socket_fd = -1;
    status = mrtc_turn_copy_string(client->server_host, sizeof(client->server_host), server_host, 1);
    if (status != MRTC_STATUS_OK) {
        return status;
    }
    status = mrtc_turn_copy_string(client->username, sizeof(client->username), username, 1);
    if (status != MRTC_STATUS_OK) {
        return status;
    }
    status = mrtc_turn_copy_string(client->password, sizeof(client->password), password, 1);
    if (status != MRTC_STATUS_OK) {
        return status;
    }
    client->server_port = server_port;

    (void) snprintf(port_text, sizeof(port_text), "%u", (unsigned int) server_port);
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    gai_result = getaddrinfo(server_host, port_text, &hints, &result);
    if (gai_result != 0) {
        return MRTC_STATUS_INVALID_STATE;
    }

    for (current = result; current != 0; current = current->ai_next) {
        fd = socket(current->ai_family, current->ai_socktype, current->ai_protocol);
        if (fd < 0) {
            continue;
        }
        if (connect(fd, current->ai_addr, current->ai_addrlen) == 0) {
            break;
        }
        close(fd);
        fd = -1;
    }
    freeaddrinfo(result);

    if (fd < 0) {
        return MRTC_STATUS_INVALID_STATE;
    }
    client->socket_fd = fd;
    return MRTC_STATUS_OK;
}

void mrtc_turn_client_free(MRTC_TURN_CLIENT *client)
{
    if (client == 0) {
        return;
    }
    if (client->socket_fd >= 0) {
        close(client->socket_fd);
    }
    memset(client, 0, sizeof(*client));
    client->socket_fd = -1;
}

MRTC_STATUS mrtc_turn_client_allocate(MRTC_TURN_CLIENT *client)
{
    uint8_t packet[MRTC_TURN_PACKET_CAPACITY];
    uint8_t transaction_id[MRTC_STUN_TRANSACTION_ID_LEN];
    size_t packet_len = 0u;
    MRTC_STUN_MESSAGE response;
    MRTC_STATUS status;

    if (client == 0 || client->socket_fd < 0) {
        return MRTC_STATUS_INVALID_ARG;
    }

    status = mrtc_stun_write_turn_allocate_request(packet, sizeof(packet), &packet_len, transaction_id);
    if (status != MRTC_STATUS_OK) {
        return status;
    }
    status = mrtc_turn_send_packet(client, packet, packet_len);
    if (status != MRTC_STATUS_OK) {
        return status;
    }
    status = mrtc_turn_recv_allocate_response(client, transaction_id, &response);
    if (status != MRTC_STATUS_OK) {
        return status;
    }

    if (response.message_type == MRTC_STUN_TYPE_ALLOCATE_SUCCESS_RESPONSE) {
        if (response.has_xor_mapped_address) {
            client->mapped_address = response.xor_mapped_address;
            client->has_mapped_address = 1;
        }
        client->relay_address = response.xor_relayed_address;
        client->has_relay_address = 1;
        return MRTC_STATUS_OK;
    }
    if (!response.has_realm || !response.has_nonce) {
        return MRTC_STATUS_PARSE_ERROR;
    }
    status = mrtc_turn_copy_string(client->realm, sizeof(client->realm), response.realm, 1);
    if (status != MRTC_STATUS_OK) {
        return status;
    }
    status = mrtc_turn_copy_string(client->nonce, sizeof(client->nonce), response.nonce, 1);
    if (status != MRTC_STATUS_OK) {
        return status;
    }
    status = mrtc_stun_make_long_term_key(client->username,
                                          client->realm,
                                          client->password,
                                          client->long_term_key);
    if (status != MRTC_STATUS_OK) {
        return status;
    }
    client->has_long_term_key = 1;

    status = mrtc_stun_write_turn_allocate_request_with_credentials(packet,
                                                                    sizeof(packet),
                                                                    &packet_len,
                                                                    transaction_id,
                                                                    client->username,
                                                                    client->realm,
                                                                    client->nonce,
                                                                    client->long_term_key);
    if (status != MRTC_STATUS_OK) {
        return status;
    }
    status = mrtc_turn_send_packet(client, packet, packet_len);
    if (status != MRTC_STATUS_OK) {
        return status;
    }
    status = mrtc_turn_recv_allocate_response(client, transaction_id, &response);
    if (status != MRTC_STATUS_OK) {
        return status;
    }
    if (response.has_xor_mapped_address) {
        client->mapped_address = response.xor_mapped_address;
        client->has_mapped_address = 1;
    }
    client->relay_address = response.xor_relayed_address;
    client->has_relay_address = 1;
    return MRTC_STATUS_OK;
}

MRTC_STATUS mrtc_turn_client_create_permission(MRTC_TURN_CLIENT *client,
                                               const char *peer_ip,
                                               unsigned short peer_port)
{
    uint8_t packet[MRTC_TURN_PACKET_CAPACITY];
    uint8_t transaction_id[MRTC_STUN_TRANSACTION_ID_LEN];
    size_t packet_len = 0u;
    MRTC_STATUS status;

    if (client == 0 || client->socket_fd < 0 || !client->has_long_term_key) {
        return MRTC_STATUS_INVALID_ARG;
    }

    status = mrtc_stun_write_turn_create_permission_request(packet,
                                                            sizeof(packet),
                                                            &packet_len,
                                                            transaction_id,
                                                            peer_ip,
                                                            peer_port,
                                                            client->username,
                                                            client->realm,
                                                            client->nonce,
                                                            client->long_term_key);
    if (status != MRTC_STATUS_OK) {
        return status;
    }
    status = mrtc_turn_send_packet(client, packet, packet_len);
    if (status != MRTC_STATUS_OK) {
        return status;
    }
    return mrtc_turn_recv_response(client,
                                   MRTC_STUN_TYPE_CREATE_PERMISSION_SUCCESS_RESPONSE,
                                   transaction_id);
}

MRTC_STATUS mrtc_turn_client_send(MRTC_TURN_CLIENT *client,
                                  const char *peer_ip,
                                  unsigned short peer_port,
                                  const uint8_t *data,
                                  size_t data_len)
{
    uint8_t packet[MRTC_TURN_PACKET_CAPACITY];
    uint8_t transaction_id[MRTC_STUN_TRANSACTION_ID_LEN];
    size_t packet_len = 0u;
    MRTC_STATUS status;

    if (client == 0 || client->socket_fd < 0 || data_len + 64u > sizeof(packet)) {
        return MRTC_STATUS_INVALID_ARG;
    }

    status = mrtc_stun_write_turn_send_indication(packet,
                                                  sizeof(packet),
                                                  &packet_len,
                                                  transaction_id,
                                                  peer_ip,
                                                  peer_port,
                                                  data,
                                                  data_len);
    if (status != MRTC_STATUS_OK) {
        return status;
    }
    return mrtc_turn_send_packet(client, packet, packet_len);
}

MRTC_STATUS mrtc_turn_client_poll_data(MRTC_TURN_CLIENT *client,
                                       int timeout_ms,
                                       MRTC_STUN_ADDRESS *peer_address,
                                       uint8_t *buffer,
                                       size_t buffer_len,
                                       size_t *received_len)
{
    uint8_t *packet;
    size_t packet_capacity;
    size_t packet_len = 0u;
    MRTC_STUN_MESSAGE message;
    MRTC_STATUS status;

    if (client == 0 || client->socket_fd < 0 || peer_address == 0 ||
        buffer == 0 || received_len == 0 || timeout_ms < 0) {
        return MRTC_STATUS_INVALID_ARG;
    }
    packet_capacity = buffer_len + 128u;
    if (packet_capacity < MRTC_TURN_PACKET_CAPACITY) {
        packet_capacity = MRTC_TURN_PACKET_CAPACITY;
    }
    packet = (uint8_t *) malloc(packet_capacity);
    if (packet == 0) {
        return MRTC_STATUS_INVALID_STATE;
    }

    status = mrtc_turn_recv_packet(client, packet, packet_capacity, &packet_len, timeout_ms);
    if (status == MRTC_STATUS_OK) {
        status = mrtc_stun_parse_turn_data_indication(packet, packet_len, &message);
    }
    if (status == MRTC_STATUS_OK) {
        if (message.data_len > buffer_len) {
            status = MRTC_STATUS_INVALID_ARG;
        } else {
            *peer_address = message.xor_peer_address;
            memcpy(buffer, message.data, message.data_len);
            *received_len = message.data_len;
        }
    }
    free(packet);
    return status;
}

MRTC_STATUS mrtc_turn_client_get_relay_address(const MRTC_TURN_CLIENT *client,
                                               MRTC_STUN_ADDRESS *relay_address)
{
    if (client == 0 || relay_address == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }
    if (!client->has_relay_address) {
        return MRTC_STATUS_INVALID_STATE;
    }
    *relay_address = client->relay_address;
    return MRTC_STATUS_OK;
}

MRTC_STATUS mrtc_turn_client_get_mapped_address(const MRTC_TURN_CLIENT *client,
                                                MRTC_STUN_ADDRESS *mapped_address)
{
    if (client == 0 || mapped_address == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }
    if (!client->has_mapped_address) {
        return MRTC_STATUS_INVALID_STATE;
    }
    *mapped_address = client->mapped_address;
    return MRTC_STATUS_OK;
}
