#include <micrortc/micrortc.h>

int main(void)
{
    const char *version = mrtc_version_string();

    if (version == 0 || version[0] == '\0') {
        return 1;
    }

    if (mrtc_initialize() != MRTC_STATUS_OK) {
        return 1;
    }

    mrtc_shutdown();
    return 0;
}
