#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/stdio_usb.h"
#include "hardware/spi.h"

// SPI pins for Pico SPI0
#define PIN_MISO 16   // ADXL345 SDO
#define PIN_CS   17   // ADXL345 CS
#define PIN_SCK  18   // ADXL345 SCL
#define PIN_MOSI 19   // ADXL345 SDA

#define ADXL_SPI spi0

// ADXL345 registers
#define REG_DEVID       0x00
#define REG_BW_RATE     0x2C
#define REG_POWER_CTL   0x2D
#define REG_DATA_FORMAT 0x31
#define REG_DATAX0      0x32

static inline void cs_select() {
    asm volatile("nop \n nop \n nop");
    gpio_put(PIN_CS, 0);
    asm volatile("nop \n nop \n nop");
}

static inline void cs_deselect() {
    asm volatile("nop \n nop \n nop");
    gpio_put(PIN_CS, 1);
    asm volatile("nop \n nop \n nop");
}

void adxl_write_register(uint8_t reg, uint8_t value) {
    uint8_t buf[2] = {reg, value};

    cs_select();
    spi_write_blocking(ADXL_SPI, buf, 2);
    cs_deselect();
}

uint8_t adxl_read_register(uint8_t reg) {
    uint8_t tx[2] = {static_cast<uint8_t>(0x80 | reg), 0x00};
    uint8_t rx[2] = {0};

    cs_select();
    spi_write_read_blocking(ADXL_SPI, tx, rx, 2);
    cs_deselect();

    return rx[1];
}

void adxl_read_accel(float *x_g, float *y_g, float *z_g) {
    uint8_t tx[7] = {0};
    uint8_t rx[7] = {0};

    // Read bit = 0x80, multi-byte bit = 0x40
    tx[0] = 0x80 | 0x40 | REG_DATAX0;

    cs_select();
    spi_write_read_blocking(ADXL_SPI, tx, rx, 7);
    cs_deselect();

    int16_t x_raw = static_cast<int16_t>((rx[2] << 8) | rx[1]);
    int16_t y_raw = static_cast<int16_t>((rx[4] << 8) | rx[3]);
    int16_t z_raw = static_cast<int16_t>((rx[6] << 8) | rx[5]);

    // Full-resolution mode scale: about 3.9 mg/LSB
    const float scale = 0.0039f;

    *x_g = x_raw * scale;
    *y_g = y_raw * scale;
    *z_g = z_raw * scale;
}

void adxl_init() {
    // 100 Hz output data rate
    adxl_write_register(REG_BW_RATE, 0x0A);

    // Full-resolution mode, +/-16g
    adxl_write_register(REG_DATA_FORMAT, 0x0B);

    // Measurement mode
    adxl_write_register(REG_POWER_CTL, 0x08);
}

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

int main() {
    stdio_init_all();
    pico_led_init();
    while (!stdio_usb_connected()) {
        sleep_ms(100);
    }

    printf("USB Connected!\n");

    // Initialize SPI0
    spi_init(ADXL_SPI, 5 * 1000 * 1000);  // 5 MHz

    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(PIN_SCK,  GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);

    gpio_init(PIN_CS);
    gpio_set_dir(PIN_CS, GPIO_OUT);
    gpio_put(PIN_CS, 1);

    // ADXL345 uses SPI mode 3
    spi_set_format(
        ADXL_SPI,
        8,
        SPI_CPOL_1,
        SPI_CPHA_1,
        SPI_MSB_FIRST
    );

    uint8_t id = adxl_read_register(REG_DEVID);

    printf("ADXL345 Device ID: 0x%02X\n", id);

    if (id != 0xE5) {
        printf("ADXL345 not detected. Check wiring.\n");
    }

    adxl_init();

    printf("x_g,y_g,z_g\n");

    while (true) {
        float x, y, z;
        adxl_read_accel(&x, &y, &z);

        printf("x=%.4f,y=%.4f,z=%.4f\n", x, y, z);

        sleep_ms(10);  // 100 Hz
    }
}