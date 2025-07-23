#pragma once
#include "daisy_seed.h"

struct Max7219
{
    daisy::SpiHandle* spi;
    daisy::GPIO cs;

    void Init(daisy::SpiHandle* spi_handle, daisy::Pin cs_pin)
    {
        spi = spi_handle;
        cs.Init(cs_pin, daisy::GPIO::Mode::OUTPUT);
        cs.Write(true);

        Send(0x09, 0x00); // Decode mode: none
        Send(0x0A, 0x03); // Intensity: 3
        Send(0x0B, 0x07); // Scan limit: all digits
        Send(0x0C, 0x01); // Normal operation
        Send(0x0F, 0x00); // Display test: off

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

