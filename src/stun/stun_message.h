#ifndef MRTC_STUN_MESSAGE_H
#define MRTC_STUN_MESSAGE_H

#include <micrortc/micrortc.h>

#include <stddef.h>
#include <stdint.h>

#define MRTC_STUN_MAGIC_COOKIE 0x2112A442u
#define MRTC_STUN_TRANSACTION_ID_LEN 12u

MRTC_STATUS mrtc_stun_write_binding_request(uint8_t *buffer,
                                            size_t buffer_len,
                                            size_t *written_len,
                                            uint8_t transaction_id[MRTC_STUN_TRANSACTION_ID_LEN]);
MRTC_STATUS mrtc_stun_validate_header(const uint8_t *buffer, size_t buffer_len);

#endif /* MRTC_STUN_MESSAGE_H */
