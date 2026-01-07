/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

// Use two DMA channels to make a programmed sequence of data transfers to the
// PIO. One channel is responsible for transferring
// the actual data, the other repeatedly reprograms that channel.

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "dac.pio.h"

// This buffers will be DMA'd to the PIO.

#define DATA_BASE 2
#define DATA_NPINS 8
const uint32_t word0[2] = {0xAA,0xAA};

// Note the order of the fields here: it's important that the length is before
// the read address, because the control channel is going to write to the last
// two registers in alias 3 on the data channel:
//           +0x0        +0x4          +0x8          +0xC (Trigger)
// Alias 0:  READ_ADDR   WRITE_ADDR    TRANS_COUNT   CTRL
// Alias 1:  CTRL        READ_ADDR     WRITE_ADDR    TRANS_COUNT
// Alias 2:  CTRL        TRANS_COUNT   READ_ADDR     WRITE_ADDR
// Alias 3:  CTRL        WRITE_ADDR    TRANS_COUNT   READ_ADDR
//
// This will program the transfer count and read address of the data channel,
// and trigger it. Once the data channel completes, it will restart the
// control channel (via CHAIN_TO) to load the next two words into its control
// registers.

#ifndef LED_DELAY_MS
#define LED_DELAY_MS 250
#endif

// Perform initialisation
int pico_led_init(void) {
#if defined(PICO_DEFAULT_LED_PIN)
    // A device like Pico that uses a GPIO for the LED will define PICO_DEFAULT_LED_PIN
    // so we can use normal GPIO functionality to turn the led on and off
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    return PICO_OK;
#elif defined(CYW43_WL_GPIO_LED_PIN)
    // For Pico W devices we need to initialise the driver etc
    return cyw43_arch_init();
#endif
}

// Turn the led on or off
void pico_set_led(bool led_on) {
#if defined(PICO_DEFAULT_LED_PIN)
    // Just set the GPIO on or off
    gpio_put(PICO_DEFAULT_LED_PIN, led_on);
#elif defined(CYW43_WL_GPIO_LED_PIN)
    // Ask the wifi "driver" to set the GPIO on or off
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_on);
#endif
}

const struct {uint32_t len; const uint32_t *data;} control_blocks[] = {
        {count_of(word0), word0}, // Skip null terminator
        {count_of(word0), word0},
        {count_of(word0), word0},
        {count_of(word0), word0},
        {0,NULL}
};

int main() {

    stdio_init_all();
    puts("DMA control block example:");
    pico_led_init();

    // Init PIO
    PIO pio;
    uint sm;
    uint offset;

    bool success = pio_claim_free_sm_and_add_program_for_gpio_range(&dac_program, &pio, &sm, &offset, DATA_BASE, DATA_NPINS, true);
    hard_assert(success);

    dac_program_init(pio,sm,offset,DATA_BASE,DATA_NPINS);

    // ctrl_chan loads control blocks into data_chan, which executes them.
    int ctrl_chan = dma_claim_unused_channel(true);
    int data_chan = dma_claim_unused_channel(true);

    // The control channel transfers two words into the data channel's control
    // registers, then halts. The write address wraps on a two-word
    // (eight-byte) boundary, so that the control channel writes the same two
    // registers when it is next triggered.

    dma_channel_config c = dma_channel_get_default_config(ctrl_chan);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, true);
    channel_config_set_ring(&c, true, 3); // 1 << 3 byte boundary on write ptr
    channel_config_set_ring(&c, false, 3); // 1 << 3 byte boundary on read ptr
    dma_channel_configure(
            ctrl_chan,
            &c,
            &dma_hw->ch[data_chan].al3_transfer_count, // Initial write address
            &control_blocks[0],                        // Initial read address
            2,                               // Halt after each control block
            false                                      // Don't start yet
    );

    // The data channel is set up to write to the PIO FIFO (paced by the
    // PIO's TX data request signal) and then chain to the control channel
    // once it completes. The control channel programs a new read address and
    // data length, and retriggers the data channel.

    c = dma_channel_get_default_config(data_chan);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_dreq(&c, pio_get_dreq(pio, sm, true));
    // Trigger ctrl_chan when data_chan completes
    channel_config_set_chain_to(&c, ctrl_chan);
    // Raise the IRQ flag when 0 is written to a trigger register (end of chain):
    channel_config_set_irq_quiet(&c, true);

    dma_channel_configure(
            data_chan,
            &c,
            &pio->txf[sm],
            NULL,           // Initial read address and transfer count are unimportant;
            0,
            false           // Don't start yet.
    );

    // Everything is ready to go. Tell the control channel to load the first
    // control block. Everything is automatic from here.
    dma_start_channel_mask(1u << ctrl_chan);

    // The data channel will assert its IRQ flag when it gets a null trigger,
    // indicating the end of the control block list. We're just going to wait
    // for the IRQ flag instead of setting up an interrupt handler.

//    while (true) {
    while(!(dma_hw->intr & 1u << data_chan)) {
//        pio_sm_put_blocking(pio,sm,0xAA);
//        pio_sm_put_blocking(pio,sm,0xaa);
        tight_loop_contents();
        pico_set_led(true);
        sleep_ms(100);
//        pio_sm_put_blocking(pio,sm,0x55);
        pico_set_led(false);
        sleep_ms(100);
    }
    puts("DMA finished.");
}