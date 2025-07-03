#include "daisy_seed.h"
using namespace daisy;

struct Max7219
{
    SpiHandle* spi;
    GPIO cs;

    void Init(SpiHandle* spi_handle)
    {
        spi = spi_handle;
        cs.Init(D10, GPIO::Mode::OUTPUT);
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

DaisySeed hw;
Max7219 max;

int main(void)
{
    hw.Configure();
    hw.Init();
    hw.spi.Init(); // Use default SPI settings

    max.Init(&hw.spi);

    while(1)
    {
        // Blink LED 1 ON, LED 2 OFF
        max.Send(1, 0x01); // LED 1 ON (row 1, first column)
        max.Send(2, 0x00); // LED 2 OFF (row 2, all columns off)
        hw.Delay(500);

        // Blink LED 1 OFF, LED 2 ON
        max.Send(1, 0x00); // LED 1 OFF
        max.Send(2, 0x01); // LED 2 ON (row 2, first column)
        hw.Delay(500);
    }
}