#include "ice_agent.h"

#include <stdio.h>
#include <string.h>

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

MRTC_STATUS mrtc_ice_format_host_candidate(char *buffer, size_t buffer_len, size_t *required_len)
{
    const char *candidate = "candidate:1 1 UDP 2122252543 127.0.0.1 9 typ host";
    size_t needed = strlen(candidate) + 1;

    if (required_len == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }
    *required_len = needed;
    if (buffer == 0 || buffer_len < needed) {
        return MRTC_STATUS_INVALID_ARG;
    }
    memcpy(buffer, candidate, needed);
    return MRTC_STATUS_OK;
}
