#include <micrortc/micrortc.h>

#ifndef MRTC_VERSION_STRING
#define MRTC_VERSION_STRING "0.1.0"
#endif

const char *mrtc_version_string(void)
{
    return MRTC_VERSION_STRING;
}

MRTC_STATUS mrtc_initialize(void)
{
    return MRTC_STATUS_OK;
}

void mrtc_shutdown(void)
{
}
