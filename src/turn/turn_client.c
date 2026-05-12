#include "turn_client.h"

#include <stdio.h>
#include <string.h>

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
