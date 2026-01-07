#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "dac.pio.h"

// TIP Modified from RPI RP2040 DMA control_blocks example <a href="https://github.com/raspberrypi/pico-examples/tree/master/dma/control_blocks">[Link]</a>

// R2R analog converter pin configuration

// TIP Inspired by rgco <a href="https://www.instructables.com/Arbitrary-Wave-Generator-With-the-Raspberry-Pi-Pic/">[link]</a>

// DATA_BASE = 2  - Pins starting from GPIO2 to avoid interference with default UART.
// DATA_NPINS = 8 - 8 bits analog converter
// Here, GPIO pins 2~9 are designated for DAC.
#define DATA_BASE 2
#define DATA_NPINS 8
// This buffers will be DMA'd to the PIO.
uint32_t buffer[2] = {0x00000000, 0xAA000000};

int main() {

    stdio_init_all();
    puts("DMA control block example:");

    // Buffer parameters (Length and address, we need to store the buffer pointer in a pointer variable, so we can have a pointer point to the buffer pointer).
    uint32_t buf_length = count_of(buffer);
    uint32_t *buf_addr  = &buffer[0];

    // Initialize PIO.
    PIO pio;
    uint sm;
    uint offset;

    bool success = pio_claim_free_sm_and_add_program_for_gpio_range(&dac_program, &pio, &sm, &offset, DATA_BASE, DATA_NPINS, true);
    hard_assert(success);

    dac_program_init(pio,sm,offset,DATA_BASE,DATA_NPINS);

    int ctrl_chan = dma_claim_unused_channel(true);
    int data_chan = dma_claim_unused_channel(true);

    dma_channel_config c = dma_channel_get_default_config(ctrl_chan);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_read_increment(&c, false);
    channel_config_set_write_increment(&c, false);

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

    dma_channel_configure(
            ctrl_chan,
            &c,
            &dma_hw->ch[data_chan].al3_read_addr_trig, // Point to alias 3 read address. We simply need to point it to a read address, no matter alias x.
            &buf_addr,
            1,
            false
    );

    c = dma_channel_get_default_config(data_chan);
    // PIO receives 32 bit data.
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    // Register PIO to DMA, allowing DMA waiting for PIO transmission.
    channel_config_set_dreq(&c, pio_get_dreq(pio, sm, true));
    // Trigger ctrl_chan when data_chan completes (reaches to the end of buffer)
    channel_config_set_chain_to(&c, ctrl_chan);
    // Suppress irq flags.
    channel_config_set_irq_quiet(&c, true);

    dma_channel_configure(
            data_chan,
            &c,
            &pio->txf[sm],
            NULL, // Set null so currently data_chan is halted. Don't start... for instance.
            buf_length,
            false
    );

    // Start control DMA.
    dma_start_channel_mask(1u << ctrl_chan);

    // System idle thread
    while(!(dma_hw->intr & 1u << data_chan)) {
        tight_loop_contents();
    }

    // DMA never finishes. :D
    puts("DMA finished.");
}