#include "sctp_session.h"

#include "common/mrtc_mutex.h"

static MRTC_MUTEX g_sctp_mutex;
static int g_sctp_mutex_ready = 0;
static unsigned int g_sctp_ref_count = 0;

MRTC_STATUS mrtc_sctp_global_init(void)
{
    if (!g_sctp_mutex_ready) {
        MRTC_STATUS status = mrtc_mutex_init(&g_sctp_mutex);
        if (status != MRTC_STATUS_OK) {
            return status;
        }
        g_sctp_mutex_ready = 1;
    }

    if (mrtc_mutex_lock(&g_sctp_mutex) != MRTC_STATUS_OK) {
        return MRTC_STATUS_INVALID_STATE;
    }
    ++g_sctp_ref_count;
    mrtc_mutex_unlock(&g_sctp_mutex);
    return MRTC_STATUS_OK;
}

void mrtc_sctp_global_deinit(void)
{
    if (!g_sctp_mutex_ready) {
        return;
    }

    if (mrtc_mutex_lock(&g_sctp_mutex) == MRTC_STATUS_OK) {
        if (g_sctp_ref_count > 0) {
            --g_sctp_ref_count;
        }
        mrtc_mutex_unlock(&g_sctp_mutex);
    }
}

MRTC_STATUS mrtc_sctp_session_init(MRTC_SCTP_SESSION *session)
{
    MRTC_STATUS status;

    if (session == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }

    status = mrtc_sctp_global_init();
    if (status != MRTC_STATUS_OK) {
        return status;
    }
    session->initialized = 1;
    session->connected = 0;
    return MRTC_STATUS_OK;
}

void mrtc_sctp_session_deinit(MRTC_SCTP_SESSION *session)
{
    if (session != 0 && session->initialized) {
        session->initialized = 0;
        session->connected = 0;
        mrtc_sctp_global_deinit();
    }
}
