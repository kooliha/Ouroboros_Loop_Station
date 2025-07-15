#include "daisy_seed.h"
using namespace daisy;

struct Max7219
{
    SpiHandle* spi;
    GPIO cs;

    void Init(SpiHandle* spi_handle, Pin cs_pin)
    {
        spi = spi_handle;
        cs.Init(cs_pin, GPIO::Mode::OUTPUT);
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
SpiHandle spi;
Max7219 LedDriver;

int main(void)
{
    hw.Configure();
    hw.Init();

    // SPI configuration for Daisy Seed rev 7
    SpiHandle::Config spi_cfg;
    spi_cfg.periph = SpiHandle::Config::Peripheral::SPI_1;
    spi_cfg.mode   = SpiHandle::Config::Mode::MASTER;
    spi_cfg.direction = SpiHandle::Config::Direction::TWO_LINES_TX_ONLY;
    spi_cfg.datasize  = 8;
    spi_cfg.clock_polarity  = SpiHandle::Config::ClockPolarity::LOW;
    spi_cfg.clock_phase     = SpiHandle::Config::ClockPhase::ONE_EDGE;
    spi_cfg.nss             = SpiHandle::Config::NSS::SOFT;
    spi_cfg.baud_prescaler  = SpiHandle::Config::BaudPrescaler::PS_64;
    spi_cfg.pin_config.sclk = daisy::seed::D8;   // SCK
    spi_cfg.pin_config.miso = daisy::seed::D9;   // MISO (not used for MAX7219)
    spi_cfg.pin_config.mosi = daisy::seed::D10;  // MOSI

    spi.Init(spi_cfg);

    LedDriver.Init(&spi, daisy::seed::D7); // Use D7 for CS/LOAD

while(1)
{

LedDriver.Send(1, 0x01); // G Dig0. Track 1 Play
LedDriver.Send(1, 0x02); // F Dig0. Track 1 Rec
LedDriver.Send(1, 0x04); // E Dig0. Track 2 Play
LedDriver.Send(1, 0x08); // D Dig0. Track 2 Rec
LedDriver.Send(1, 0x10); // C Dig0. Track 3 Play
LedDriver.Send(1, 0x20); // B Dig0. Track 3 Rec
LedDriver.Send(1, 0x40); // A Dig0. Track 4 Play        
LedDriver.Send(1, 0x80); // DP Dig0. Track 4 Rec
LedDriver.Send(2, 0x80); // DP Dig1. Track 5 Play
LedDriver.Send(2, 0x40); // A Dig1 Track 5 Rec

    
}

}