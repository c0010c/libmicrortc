#include "ice_agent.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

MRTC_STATUS mrtc_ice_parse_candidate(const char *candidate, MRTC_ICE_CANDIDATE *parsed)
{
    int matched;

    if (candidate == 0 || parsed == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }

    memset(parsed, 0, sizeof(*parsed));
    matched = sscanf(candidate,
                     "candidate:%15s %u %7s %u %63s %hu typ %15s",
                     parsed->foundation,
                     &parsed->component,
                     parsed->protocol,
                     &parsed->priority,
                     parsed->ip,
                     &parsed->port,
                     parsed->type);

    if (matched != 7 || parsed->component == 0 || parsed->port == 0) {
        return MRTC_STATUS_PARSE_ERROR;
    }

    if (strcmp(parsed->protocol, "UDP") != 0 && strcmp(parsed->protocol, "udp") != 0) {
        return MRTC_STATUS_PARSE_ERROR;
    }

    if (strcmp(parsed->type, "host") != 0 && strcmp(parsed->type, "srflx") != 0 && strcmp(parsed->type, "relay") != 0) {
        return MRTC_STATUS_PARSE_ERROR;
    }

    return MRTC_STATUS_OK;
}

void mrtc_ice_host_endpoint_init(MRTC_ICE_HOST_ENDPOINT *endpoint)
{
    if (endpoint == 0) {
        return;
    }
    memset(endpoint, 0, sizeof(*endpoint));
    endpoint->fd = -1;
}

MRTC_STATUS mrtc_ice_host_endpoint_bind(MRTC_ICE_HOST_ENDPOINT *endpoint)
{
    int fd;
    struct sockaddr_in addr;
    socklen_t addr_len;

    if (endpoint == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }
    if (endpoint->fd >= 0 && endpoint->port != 0) {
        return MRTC_STATUS_OK;
    }

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        return MRTC_STATUS_INVALID_STATE;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(0);
    if (inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) != 1) {
        close(fd);
        return MRTC_STATUS_INVALID_STATE;
    }
    if (bind(fd, (const struct sockaddr *) &addr, (socklen_t) sizeof(addr)) != 0) {
        close(fd);
        return errno == EADDRINUSE ? MRTC_STATUS_INVALID_STATE : MRTC_STATUS_INVALID_STATE;
    }

    addr_len = (socklen_t) sizeof(addr);
    if (getsockname(fd, (struct sockaddr *) &addr, &addr_len) != 0) {
        close(fd);
        return MRTC_STATUS_INVALID_STATE;
    }

    endpoint->fd = fd;
    (void) snprintf(endpoint->ip, sizeof(endpoint->ip), "127.0.0.1");
    endpoint->port = ntohs(addr.sin_port);
    if (endpoint->port == 0u) {
        mrtc_ice_host_endpoint_close(endpoint);
        return MRTC_STATUS_INVALID_STATE;
    }
    return MRTC_STATUS_OK;
}

void mrtc_ice_host_endpoint_close(MRTC_ICE_HOST_ENDPOINT *endpoint)
{
    if (endpoint == 0) {
        return;
    }
    if (endpoint->fd >= 0) {
        close(endpoint->fd);
    }
    endpoint->fd = -1;
    endpoint->ip[0] = '\0';
    endpoint->port = 0;
}

MRTC_STATUS mrtc_ice_format_host_endpoint_candidate(const MRTC_ICE_HOST_ENDPOINT *endpoint,
                                                    char *buffer,
                                                    size_t buffer_len,
                                                    size_t *required_len)
{
    if (endpoint == 0 || endpoint->fd < 0 || endpoint->ip[0] == '\0' || endpoint->port == 0u) {
        return MRTC_STATUS_INVALID_STATE;
    }
    return mrtc_ice_format_candidate("host", endpoint->ip, endpoint->port, buffer, buffer_len, required_len);
}

MRTC_STATUS mrtc_ice_format_host_candidate(char *buffer, size_t buffer_len, size_t *required_len)
{
    return mrtc_ice_format_candidate("host", "127.0.0.1", 9, buffer, buffer_len, required_len);
}

MRTC_STATUS mrtc_ice_format_candidate(const char *type,
                                      const char *ip,
                                      unsigned short port,
                                      char *buffer,
                                      size_t buffer_len,
                                      size_t *required_len)
{
    char candidate[160];
    size_t needed;

    if (type == 0 || ip == 0 || required_len == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }
    if (strcmp(type, "host") != 0 && strcmp(type, "srflx") != 0 && strcmp(type, "relay") != 0) {
        return MRTC_STATUS_PARSE_ERROR;
    }

    (void) snprintf(candidate,
                    sizeof(candidate),
                    "candidate:1 1 UDP 2122252543 %s %u typ %s",
                    ip,
                    (unsigned int) port,
                    type);
    needed = strlen(candidate) + 1;
    *required_len = needed;
    if (buffer == 0 || buffer_len < needed) {
        return MRTC_STATUS_INVALID_ARG;
    }
    memcpy(buffer, candidate, needed);
    return MRTC_STATUS_OK;
}

int mrtc_ice_packet_is_stun(const unsigned char *packet, size_t packet_len)
{
    if (packet == 0 || packet_len < 20) {
        return 0;
    }
    return packet[4] == 0x21 && packet[5] == 0x12 && packet[6] == 0xA4 && packet[7] == 0x42;
}
