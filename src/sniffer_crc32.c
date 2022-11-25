#include "pico_wifi_boot/sniffer_crc32.h"

#include <string.h>

#include "hardware/dma.h"

uint32_t reverse_uint32(uint32_t n) {
    n = (n << 16) | (n >> 16);
    n = ((n << 8) & 0xFF00FF00) | ((n >> 8) & 0x00FF00FF);
    n = ((n << 4) & 0xF0F0F0F0) | ((n >> 4) & 0x0F0F0F0F);
    n = ((n << 2) & 0xCCCCCCCC) | ((n >> 2) & 0x33333333);
    n = ((n << 1) & 0xAAAAAAAA) | ((n >> 1) & 0x55555555);
    return n;
}

uint32_t sniffer_crc32(uint8_t* aligned_addr, uint32_t len) {
    int channel = dma_claim_unused_channel(true);
    dma_channel_config default_config = dma_channel_get_default_config(channel);
    dma_channel_set_config(channel, &default_config, false);

    // DMA does not increment writes by default, simply dump data into a placeholder
    uint32_t write_placeholder;
    dma_channel_set_write_addr(channel, &write_placeholder, false);

    // Configure the sniffer for bit-reversed CRC-32 and set initial value
    dma_sniffer_enable(channel, 0x1, true);
    dma_hw->sniff_data = 0xFFFFFFFF;

    dma_channel_transfer_from_buffer_now(channel, aligned_addr, len / 4);
    dma_channel_wait_for_finish_blocking(channel);

    // If the length was not a multiple of 4, pad with zeros
    uint32_t spare_bytes = len % 4;
    if (spare_bytes) {
        uint8_t padded[4] = {0};
        memcpy(padded, aligned_addr + len - spare_bytes, spare_bytes);
        dma_channel_transfer_from_buffer_now(channel, padded, 1);
        dma_channel_wait_for_finish_blocking(channel);
    }

    dma_sniffer_disable();
    dma_channel_unclaim(channel);

    // Flip and reverse bits to match common implementations
    return reverse_uint32(dma_hw->sniff_data ^ 0xFFFFFFFF);
}
