#include "stun/stun.h"
#include "test_runner.h"

#include <string.h>

static void test_write_u16(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)(value >> 8);
    data[1] = (uint8_t)value;
}

static void test_write_u32(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)(value >> 24);
    data[1] = (uint8_t)(value >> 16);
    data[2] = (uint8_t)(value >> 8);
    data[3] = (uint8_t)value;
}

int rtc_test_stun(void)
{
    const uint8_t txid[RTC_STUN_TRANSACTION_ID_BYTES] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
        0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b};
    const uint8_t expected_request[20] = {
        0x00, 0x01, 0x00, 0x00, 0x21, 0x12, 0xa4, 0x42, 0x00, 0x01,
        0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b};
    uint8_t packet[32];
    uint8_t response[32];
    rtc_stun_header_t header;
    rtc_stun_xor_mapped_address_t mapped;
    size_t out_len = 0;

    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_stun_write_binding_request(
                                       packet, sizeof(packet), txid, &out_len));
    RTC_TEST_EQ_INT(20, out_len);
    RTC_TEST_EQ_INT(0, memcmp(packet, expected_request, sizeof(expected_request)));
    RTC_TEST_ASSERT(rtc_stun_is_datagram(packet, out_len));

    packet[4] = 0;
    RTC_TEST_ASSERT(!rtc_stun_is_datagram(packet, out_len));

    RTC_TEST_ASSERT(!rtc_stun_is_datagram(expected_request, 19));

    memcpy(packet, expected_request, sizeof(expected_request));
    packet[3] = 4;
    RTC_TEST_EQ_INT(RTC_STATUS_PROTOCOL_ERROR,
                    rtc_stun_parse_header(packet, sizeof(expected_request),
                                          &header));

    memset(response, 0, sizeof(response));
    test_write_u16(response, RTC_STUN_BINDING_SUCCESS_RESPONSE);
    test_write_u16(response + 2, 12);
    test_write_u32(response + 4, RTC_STUN_MAGIC_COOKIE);
    memcpy(response + 8, txid, RTC_STUN_TRANSACTION_ID_BYTES);
    /* XOR-MAPPED-ADDRESS: IPv4 203.0.113.7:54321 */
    test_write_u16(response + 20, 0x0020);
    test_write_u16(response + 22, 8);
    response[24] = 0;
    response[25] = 0x01;
    test_write_u16(response + 26,
                   (uint16_t)(54321u ^ (RTC_STUN_MAGIC_COOKIE >> 16)));
    test_write_u32(response + 28, 0xcb007107u ^ RTC_STUN_MAGIC_COOKIE);

    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_stun_parse_header(response, sizeof(response),
                                          &header));
    RTC_TEST_EQ_INT(RTC_STUN_BINDING_SUCCESS_RESPONSE, header.type);
    RTC_TEST_EQ_INT(12, header.length);
    RTC_TEST_EQ_INT(0, memcmp(header.transaction_id, txid,
                              RTC_STUN_TRANSACTION_ID_BYTES));
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_stun_parse_xor_mapped_address(response,
                                                      sizeof(response),
                                                      &mapped));
    RTC_TEST_EQ_INT(0, strcmp(mapped.ip, "203.0.113.7"));
    RTC_TEST_EQ_INT(54321, mapped.port);

    return 0;
}
