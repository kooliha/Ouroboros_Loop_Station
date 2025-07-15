#include "daisy_seed.h"
#include "max7219.h"
using namespace daisy;

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
        LedDriver.Send(2, 0x40); // A Dig1 Track 5 Rec
        LedDriver.Send(2, 0x80); // DP Dig1. Track 5 Play
        LedDriver.Send(1, 0x01); // G Dig0. Track 4 Rec
        LedDriver.Send(1, 0x02); // F Dig0. Track 4 Play
        LedDriver.Send(1, 0x04); // E Dig0. Track 3 Rec
        LedDriver.Send(1, 0x08); // D Dig0. Track 3 Play
        LedDriver.Send(1, 0x10); // C Dig0. Track 2 Rec
        LedDriver.Send(1, 0x20); // B Dig0. Track 2 Play
        LedDriver.Send(1, 0x40); // A Dig0. Track 1 Rec        
        LedDriver.Send(1, 0x80); // DP Dig0. Track 1 Play


    }
}