#pragma once
#include "daisy_seed.h"

struct LedIndicator
{
    uint8_t digit;   // 1 = Dig0, 2 = Dig1, etc.
    uint8_t segment; // Segment bitmask
};

// Layer play/rec indicators
constexpr LedIndicator LED_LAYER1_PLAY     = {1, 0x80}; // DP Dig0
constexpr LedIndicator LED_LAYER1_REC      = {1, 0x40}; // A Dig0

constexpr LedIndicator LED_LAYER2_PLAY     = {1, 0x20}; // B Dig0
constexpr LedIndicator LED_LAYER2_REC      = {1, 0x10}; // C Dig0

constexpr LedIndicator LED_LAYER3_PLAY     = {1, 0x08}; // D Dig0
constexpr LedIndicator LED_LAYER3_REC      = {1, 0x04}; // E Dig0

constexpr LedIndicator LED_LAYER4_PLAY     = {1, 0x02}; // F Dig0
constexpr LedIndicator LED_LAYER4_REC      = {1, 0x01}; // G Dig0

constexpr LedIndicator LED_LAYER5_PLAY     = {2, 0x80}; // DP Dig1
constexpr LedIndicator LED_LAYER5_REC      = {2, 0x40}; // A Dig1

// Layer selected indicators
constexpr LedIndicator LED_LAYER1_Selected = {2, 0x20}; // B Dig1
constexpr LedIndicator LED_LAYER2_Selected = {2, 0x10}; // C Dig1
constexpr LedIndicator LED_LAYER3_Selected = {2, 0x08}; // D Dig1
constexpr LedIndicator LED_LAYER4_Selected = {2, 0x04}; // E Dig1
constexpr LedIndicator LED_LAYER5_Selected = {2, 0x02}; // F Dig1

// Channel indicators
constexpr LedIndicator LED_CHANNEL_GUITAR = {3, 0x80}; // DP Dig2
constexpr LedIndicator LED_CHANNEL_MIC    = {3, 0x40}; // A  Dig2
constexpr LedIndicator LED_CHANNEL_LINE   = {3, 0x20}; // B  Dig2



struct Max7219
{
    daisy::SpiHandle* spi;
    daisy::GPIO cs;

    void Init(daisy::SpiHandle* spi_handle, daisy::Pin cs_pin)
    {
        spi = spi_handle;
        cs.Init(cs_pin, daisy::GPIO::Mode::OUTPUT);
        cs.Write(true);

        // MAX7219 init sequence
        Send(0x09, 0x00); // Decode mode: none
        Send(0x0A, 0x03); // Intensity: 3
        Send(0x0B, 0x07); // Scan limit: all digits
        Send(0x0C, 0x01); // Normal operation
        Send(0x0F, 0x00); // Display test: off

        // Clear display
        for (uint8_t i = 1; i <= 8; i++)
            Send(i, 0x00);
    }

    void Send(uint8_t reg, uint8_t data)
    {
        cs.Write(false);
        uint8_t buf[2] = {reg, data};
        spi->BlockingTransmit(buf, 2, 1000);
        cs.Write(true);
    }
};