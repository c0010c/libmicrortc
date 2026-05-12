#include "mrtc_socket.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>

MRTC_STATUS mrtc_socket_address_from_ipv4(const char *ip, unsigned short port, MRTC_SOCKET_ADDRESS *address)
{
    if (ip == 0 || address == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }

    memset(address, 0, sizeof(*address));
    address->addr.sin_family = AF_INET;
    address->addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &address->addr.sin_addr) != 1) {
        return MRTC_STATUS_PARSE_ERROR;
    }
    address->len = (socklen_t) sizeof(address->addr);
    return MRTC_STATUS_OK;
}

MRTC_STATUS mrtc_socket_address_to_string(const MRTC_SOCKET_ADDRESS *address,
                                          char *buffer,
                                          size_t buffer_len)
{
    char ip[INET_ADDRSTRLEN];

    if (address == 0 || buffer == 0 || buffer_len == 0 || address->addr.sin_family != AF_INET) {
        return MRTC_STATUS_INVALID_ARG;
    }

    if (inet_ntop(AF_INET, &address->addr.sin_addr, ip, sizeof(ip)) == 0) {
        return MRTC_STATUS_INVALID_STATE;
    }

    if (snprintf(buffer, buffer_len, "%s:%u", ip, (unsigned int) ntohs(address->addr.sin_port)) >= (int) buffer_len) {
        return MRTC_STATUS_INVALID_ARG;
    }

    return MRTC_STATUS_OK;
}

unsigned short mrtc_socket_address_port(const MRTC_SOCKET_ADDRESS *address)
{
    if (address == 0 || address->addr.sin_family != AF_INET) {
        return 0;
    }

    return ntohs(address->addr.sin_port);
}
