#include "rtp_rolling_buffer.h"

#include <stdlib.h>
#include <string.h>

typedef struct MRTC_RTP_ROLLING_BUFFER_ENTRY {
    int valid;
    uint16_t sequence_number;
    uint8_t *packet;
    size_t packet_size;
} MRTC_RTP_ROLLING_BUFFER_ENTRY;

struct MRTC_RTP_ROLLING_BUFFER {
    size_t capacity;
    size_t size;
    size_t write_index;
    MRTC_RTP_ROLLING_BUFFER_ENTRY *entries;
};

static void mrtc_rtp_rolling_buffer_clear_entry(MRTC_RTP_ROLLING_BUFFER_ENTRY *entry)
{
    if (entry == 0) {
        return;
    }
    free(entry->packet);
    memset(entry, 0, sizeof(*entry));
}

static MRTC_RTP_ROLLING_BUFFER_ENTRY *mrtc_rtp_rolling_buffer_find(MRTC_RTP_ROLLING_BUFFER *buffer,
                                                                   uint16_t sequence_number)
{
    size_t i;

    if (buffer == 0) {
        return 0;
    }
    for (i = 0; i < buffer->capacity; ++i) {
        if (buffer->entries[i].valid && buffer->entries[i].sequence_number == sequence_number) {
            return &buffer->entries[i];
        }
    }
    return 0;
}

static const MRTC_RTP_ROLLING_BUFFER_ENTRY *mrtc_rtp_rolling_buffer_find_const(const MRTC_RTP_ROLLING_BUFFER *buffer,
                                                                              uint16_t sequence_number)
{
    size_t i;

    if (buffer == 0) {
        return 0;
    }
    for (i = 0; i < buffer->capacity; ++i) {
        if (buffer->entries[i].valid && buffer->entries[i].sequence_number == sequence_number) {
            return &buffer->entries[i];
        }
    }
    return 0;
}

MRTC_STATUS mrtc_rtp_rolling_buffer_create(size_t capacity, MRTC_RTP_ROLLING_BUFFER **buffer)
{
    MRTC_RTP_ROLLING_BUFFER *created;

    if (capacity == 0u || buffer == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }

    created = (MRTC_RTP_ROLLING_BUFFER *) calloc(1, sizeof(*created));
    if (created == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }
    created->entries = (MRTC_RTP_ROLLING_BUFFER_ENTRY *) calloc(capacity, sizeof(*created->entries));
    if (created->entries == 0) {
        free(created);
        return MRTC_STATUS_INVALID_ARG;
    }
    created->capacity = capacity;
    *buffer = created;
    return MRTC_STATUS_OK;
}

void mrtc_rtp_rolling_buffer_free(MRTC_RTP_ROLLING_BUFFER *buffer)
{
    size_t i;

    if (buffer == 0) {
        return;
    }
    for (i = 0; i < buffer->capacity; ++i) {
        mrtc_rtp_rolling_buffer_clear_entry(&buffer->entries[i]);
    }
    free(buffer->entries);
    free(buffer);
}

MRTC_STATUS mrtc_rtp_rolling_buffer_add(MRTC_RTP_ROLLING_BUFFER *buffer,
                                        uint16_t sequence_number,
                                        const uint8_t *packet,
                                        size_t packet_size)
{
    MRTC_RTP_ROLLING_BUFFER_ENTRY *entry;
    uint8_t *copy;

    if (buffer == 0 || packet == 0 || packet_size == 0u) {
        return MRTC_STATUS_INVALID_ARG;
    }

    copy = (uint8_t *) malloc(packet_size);
    if (copy == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }
    memcpy(copy, packet, packet_size);

    entry = mrtc_rtp_rolling_buffer_find(buffer, sequence_number);
    if (entry == 0) {
        entry = &buffer->entries[buffer->write_index % buffer->capacity];
        if (!entry->valid) {
            buffer->size++;
        }
        buffer->write_index = (buffer->write_index + 1u) % buffer->capacity;
    }

    free(entry->packet);
    entry->packet = copy;
    entry->packet_size = packet_size;
    entry->sequence_number = sequence_number;
    entry->valid = 1;
    return MRTC_STATUS_OK;
}

MRTC_STATUS mrtc_rtp_rolling_buffer_lookup(const MRTC_RTP_ROLLING_BUFFER *buffer,
                                           uint16_t sequence_number,
                                           const uint8_t **packet,
                                           size_t *packet_size)
{
    const MRTC_RTP_ROLLING_BUFFER_ENTRY *entry;

    if (buffer == 0 || packet == 0 || packet_size == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }

    *packet = 0;
    *packet_size = 0;
    entry = mrtc_rtp_rolling_buffer_find_const(buffer, sequence_number);
    if (entry == 0) {
        return MRTC_STATUS_INVALID_STATE;
    }
    *packet = entry->packet;
    *packet_size = entry->packet_size;
    return MRTC_STATUS_OK;
}

MRTC_STATUS mrtc_rtp_rolling_buffer_copy(const MRTC_RTP_ROLLING_BUFFER *buffer,
                                         uint16_t sequence_number,
                                         uint8_t *packet,
                                         size_t packet_capacity,
                                         size_t *packet_size)
{
    const uint8_t *stored;
    size_t stored_size;
    MRTC_STATUS status;

    if (packet_size == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }

    status = mrtc_rtp_rolling_buffer_lookup(buffer, sequence_number, &stored, &stored_size);
    if (status != MRTC_STATUS_OK) {
        *packet_size = 0;
        return status;
    }
    *packet_size = stored_size;
    if (packet == 0) {
        return MRTC_STATUS_OK;
    }
    if (packet_capacity < stored_size) {
        return MRTC_STATUS_INVALID_ARG;
    }
    memcpy(packet, stored, stored_size);
    return MRTC_STATUS_OK;
}

MRTC_STATUS mrtc_rtp_rolling_buffer_remove(MRTC_RTP_ROLLING_BUFFER *buffer, uint16_t sequence_number)
{
    MRTC_RTP_ROLLING_BUFFER_ENTRY *entry;

    if (buffer == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }
    entry = mrtc_rtp_rolling_buffer_find(buffer, sequence_number);
    if (entry == 0) {
        return MRTC_STATUS_INVALID_STATE;
    }
    mrtc_rtp_rolling_buffer_clear_entry(entry);
    buffer->size--;
    return MRTC_STATUS_OK;
}

size_t mrtc_rtp_rolling_buffer_size(const MRTC_RTP_ROLLING_BUFFER *buffer)
{
    return buffer == 0 ? 0u : buffer->size;
}
