#pragma once

#include "esphome.h"
#include "esphome/core/hal.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace qualia {

// PCA9554 I2C Register Map
#define PCA9554_INPUT_PORT 0x00
#define PCA9554_OUTPUT_PORT 0x01
#define PCA9554_POLARITY_INV 0x02
#define PCA9554_CONFIG_PORT 0x03// PCA9554 Pin Mapping for Adafruit Qualia S3 (Page 153 of PDF)
// Bit 0: SPI Clock (SCL)
// Bit 1: SPI Chip Select (CS) - Active Low
// Bit 2: Display Reset (RST) - Active Low
// Bit 3: Backlight (High = On)
// Bit 7: SPI Data (SDA)

#define PIN_TFT_SCL       (1 << 0)
#define PIN_TFT_CS        (1 << 1)
#define PIN_TFT_RESET     (1 << 2)
#define PIN_TFT_BACKLIGHT (1 << 3)
#define PIN_TFT_SDA       (1 << 7)

class QualiaDisplayInit : public Component, public i2c::I2CDevice {
 protected:
  uint8_t current_outputs_ = 0x00;

  void write_pca_output(uint8_t value) {
    this->current_outputs_ = value;
    this->write_register(PCA9554_OUTPUT_PORT, &this->current_outputs_, 1);
  }

  void spi_write(bool is_command, uint8_t data) {
    // Start bit transfer: SCL Low, CS Low
    uint8_t out = this->current_outputs_ & ~(PIN_TFT_SCL | PIN_TFT_CS);
    this->write_pca_output(out);

    // 9 bits: D/C bit then 8 data bits (MSB first)
    uint16_t word = (is_command ? 0x000 : 0x100) | data;

    for (int i = 0; i < 9; i++) {
        bool bit = (word & (1 << (8 - i))) != 0;
        
        // Select SDA bit
        if (bit) out |= PIN_TFT_SDA;
        else out &= ~PIN_TFT_SDA;
        
        // SCL Low
        out &= ~PIN_TFT_SCL;
        this->write_pca_output(out);

        // SCL High (Sample on rising edge)
        out |= PIN_TFT_SCL;
        this->write_pca_output(out);
    }

    // End transfer: SCL Low, CS High
    out &= ~PIN_TFT_SCL;
    out |= PIN_TFT_CS;
    this->write_pca_output(out);
  }

  void send_command(uint8_t cmd) { spi_write(true, cmd); }
  void send_data(uint8_t data) { spi_write(false, data); }

 public:
  void setup() override {
    ESP_LOGI("qualia", "Initializing PCA9554 and ST7701S (3-Wire SPI Mode 0)...");

    // 1. Direction: 0x70 (Bits 0,1,2,3,7=Output; 4,5,6=Input)
    // We drive the backlight explicitly.
    uint8_t config = 0x70; 
    this->write_register(PCA9554_CONFIG_PORT, &config, 1);

    // 2. Initial state: CS High, SCL Low, RESET High, Backlight HIGH.
    this->current_outputs_ = PIN_TFT_CS | PIN_TFT_RESET | PIN_TFT_BACKLIGHT;
    this->write_pca_output(this->current_outputs_);

    // 3. Hardware Reset Pulse
    delay(20);
    this->write_pca_output(this->current_outputs_ & ~PIN_TFT_RESET); // Reset LOW
    delay(20);
    this->write_pca_output(this->current_outputs_ | PIN_TFT_RESET);  // Reset Released
    delay(120);

    // 4. ST7701S Initialization Sequence for Adafruit TL032FWV01-I1440A
    send_command(0x11);
    delay(100);

    send_command(0xFF);
    send_data(0x77); send_data(0x01); send_data(0x00); send_data(0x00); send_data(0x13);

    send_command(0xEF);
    send_data(0x08);

    send_command(0xFF);
    send_data(0x77); send_data(0x01); send_data(0x00); send_data(0x00); send_data(0x10);

    send_command(0xC0);
    send_data(0xE5); send_data(0x02);

    send_command(0xC1);
    send_data(0x0C); send_data(0x0A);

    send_command(0xC2);
    send_data(0x07); send_data(0x0F);

    send_command(0xC3);
    send_data(0x02);

    send_command(0xCC);
    send_data(0x10);

    send_command(0xCD);
    send_data(0x08);

    send_command(0xB0);
    send_data(0x00); send_data(0x08); send_data(0x51); send_data(0x0D);
    send_data(0xCE); send_data(0x06); send_data(0x00); send_data(0x08);
    send_data(0x08); send_data(0x1D); send_data(0x02); send_data(0xD0);
    send_data(0x0F); send_data(0x6F); send_data(0x36); send_data(0x3F);

    send_command(0xB1);
    send_data(0x00); send_data(0x10); send_data(0x4F); send_data(0x0C);
    send_data(0x11); send_data(0x05); send_data(0x00); send_data(0x07);
    send_data(0x07); send_data(0x1F); send_data(0x05); send_data(0xD3);
    send_data(0x11); send_data(0x6E); send_data(0x34); send_data(0x3F);

    send_command(0xFF);
    send_data(0x77); send_data(0x01); send_data(0x00); send_data(0x00); send_data(0x11);

    send_command(0xB0); send_data(0x4D);
    send_command(0xB1); send_data(0x1C);
    send_command(0xB2); send_data(0x87);
    send_command(0xB3); send_data(0x80);
    send_command(0xB5); send_data(0x47);
    send_command(0xB7); send_data(0x85);
    send_command(0xB8); send_data(0x21);
    send_command(0xB9); send_data(0x10);
    send_command(0xC1); send_data(0x78);
    send_command(0xC2); send_data(0x78);
    send_command(0xD0); send_data(0x88);

    delay(100);

    send_command(0xE0);
    send_data(0x80); send_data(0x00); send_data(0x02);

    send_command(0xE1);
    send_data(0x04); send_data(0xA0); send_data(0x00); send_data(0x00);
    send_data(0x05); send_data(0xA0); send_data(0x00); send_data(0x00);
    send_data(0x00); send_data(0x60); send_data(0x60);

    send_command(0xE2);
    send_data(0x30); send_data(0x30); send_data(0x60); send_data(0x60);
    send_data(0x3C); send_data(0xA0); send_data(0x00); send_data(0x00);
    send_data(0x3D); send_data(0xA0); send_data(0x00); send_data(0x00);
    send_data(0x00);

    send_command(0xE3);
    send_data(0x00); send_data(0x00); send_data(0x33); send_data(0x33);

    send_command(0xE4);
    send_data(0x44); send_data(0x44);

    send_command(0xE5);
    send_data(0x06); send_data(0x3E); send_data(0xA0); send_data(0xA0);
    send_data(0x08); send_data(0x40); send_data(0xA0); send_data(0xA0);
    send_data(0x0A); send_data(0x42); send_data(0xA0); send_data(0xA0);
    send_data(0x0C); send_data(0x44); send_data(0xA0); send_data(0xA0);

    send_command(0xE6);
    send_data(0x00); send_data(0x00); send_data(0x33); send_data(0x33);

    send_command(0xE7);
    send_data(0x44); send_data(0x44);

    send_command(0xE8);
    send_data(0x07); send_data(0x3F); send_data(0xA0); send_data(0xA0);
    send_data(0x09); send_data(0x41); send_data(0xA0); send_data(0xA0);
    send_data(0x0B); send_data(0x43); send_data(0xA0); send_data(0xA0);
    send_data(0x0D); send_data(0x45); send_data(0xA0); send_data(0xA0);

    send_command(0xEB);
    send_data(0x00); send_data(0x01); send_data(0x4E); send_data(0x4E);
    send_data(0xEE); send_data(0x44); send_data(0x00);

    send_command(0xED);
    send_data(0xFF); send_data(0xFF); send_data(0x04); send_data(0x56);
    send_data(0x72); send_data(0xFF); send_data(0xFF); send_data(0xFF);
    send_data(0xFF); send_data(0xFF); send_data(0xFF); send_data(0x27);
    send_data(0x65); send_data(0x40); send_data(0xFF); send_data(0xFF);

    send_command(0xEF);
    send_data(0x10); send_data(0x0D); send_data(0x04); send_data(0x08);
    send_data(0x3F); send_data(0x1F);

    send_command(0xFF);
    send_data(0x77); send_data(0x01); send_data(0x00); send_data(0x00);
    send_data(0x13);

    send_command(0xE8);
    send_data(0x00); send_data(0x0E);

    send_command(0xFF);
    send_data(0x77); send_data(0x01); send_data(0x00); send_data(0x00);
    send_data(0x00);

    send_command(0x11);
    delay(120);

    send_command(0xFF);
    send_data(0x77); send_data(0x01); send_data(0x00); send_data(0x00);
    send_data(0x13);

    send_command(0xE8);
    send_data(0x00); send_data(0x0C);

    delay(10);

    send_command(0xE8);
    send_data(0x00); send_data(0x00);

    send_command(0xFF);
    send_data(0x77); send_data(0x01); send_data(0x00); send_data(0x00);
    send_data(0x00);

    send_command(0x36);
    send_data(0x00);

    send_command(0x3A);
    send_data(0x66);

    send_command(0x11);
    delay(120);

    // Display On
    send_command(0x29);
    delay(120);

    // Ensure Backlight is ON (it should be already from setup but let's be explicit)
    this->write_pca_output(this->current_outputs_ | PIN_TFT_BACKLIGHT);

    // Done
    ESP_LOGI("qualia", "ST7701S Initialization Complete!");
  }
};

} // namespace qualia
} // namespace esphome
