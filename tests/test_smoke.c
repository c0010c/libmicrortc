#include <stdint.h>

#include "webrtc/webrtc.h"

int main(void)
{
    webrtc_instance_t* instance = NULL;
    webrtc_status_t status = WEBRTC_STATUS_OK;

    status = webrtc_init(&instance);
    if (status != WEBRTC_STATUS_OK || instance == NULL) {
        return 1;
    }

    status = webrtc_pump_step(instance, (uint64_t) 0, (uint32_t) 1);
    if (status != WEBRTC_STATUS_NOT_IMPLEMENTED) {
        webrtc_deinit(instance);
        return 2;
    }

    webrtc_deinit(instance);
    return 0;
}
