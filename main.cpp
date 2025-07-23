#include "daisy_seed.h"
#include "daisysp.h"
#include "max7219.h"
#include "looper_layer.h"

using namespace daisy;
using namespace daisy::seed;
using namespace daisysp;

// --- Globals ---
DaisySeed hw;
Max7219 LedDriver;

LooperLayer layer1;
LooperLayer layer2;

#define kBuffSize (48000 * 66) // 2.5 minutes of floats at 48 kHz

float DSY_SDRAM_BSS buffer_l1[kBuffSize];
float DSY_SDRAM_BSS buffer_r1[kBuffSize];
float DSY_SDRAM_BSS buffer_l2[kBuffSize];
float DSY_SDRAM_BSS buffer_r2[kBuffSize];


// LED definitions for both layers
constexpr uint8_t LED_LAYER1_REC  = 0x40; // A
constexpr uint8_t LED_LAYER1_PLAY = 0x80; // DP
constexpr uint8_t LED_LAYER2_REC  = 0x10; // C
constexpr uint8_t LED_LAYER2_PLAY = 0x20; // B



// --- Hardware Setup ---
void SetupHardware()
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
    spi_cfg.pin_config.sclk = daisy::seed::D8;
    spi_cfg.pin_config.miso = daisy::seed::D9;
    spi_cfg.pin_config.mosi = daisy::seed::D10;

    static SpiHandle spi;
    spi.Init(spi_cfg);

    LedDriver.Init(&spi, daisy::seed::D7);

    // Layer 1 hardware
    static Switch ch1_button;
    static GPIO ch1_input_select;
    ch1_button.Init(hw.GetPin(17), 50);
    ch1_input_select.Init(D21, GPIO::Mode::INPUT, GPIO::Pull::PULLUP);

    layer1.Init(buffer_l1, buffer_r1, kBuffSize, &ch1_button, &ch1_input_select);

    // Layer 2 hardware
    static Switch ch2_button;
    static GPIO ch2_input_select;
    ch2_button.Init(hw.GetPin(18), 50);
    ch2_input_select.Init(D25, GPIO::Mode::INPUT, GPIO::Pull::PULLUP);

    layer2.Init(buffer_l2, buffer_r2, kBuffSize, &ch2_button, &ch2_input_select);



    // ADC setup for speed, volume, pan for both layers
    static AdcChannelConfig speed_adc1, volume_adc1, pan_adc1;
    static AdcChannelConfig speed_adc2, volume_adc2, pan_adc2;
    speed_adc1.InitSingle(D19);
    volume_adc1.InitSingle(D20);
    pan_adc1.InitSingle(D22);

    speed_adc2.InitSingle(D26);
    volume_adc2.InitSingle(D27);
    pan_adc2.InitSingle(D28);

    AdcChannelConfig adc_cfgs[6] = {
        speed_adc1, volume_adc1, pan_adc1,
        speed_adc2, volume_adc2, pan_adc2
    };
    hw.adc.Init(adc_cfgs, 6);
    hw.adc.Start();
}



// --- Audio Callback ---
void AudioCallback(AudioHandle::InputBuffer in,
                   AudioHandle::OutputBuffer out,
                   size_t size)
{
    for(size_t i = 0; i < size; i++)
    {
        out[0][i] = 0.0f;
        out[1][i] = 0.0f;
    }

    layer1.Process(0, in, out, size, &layer2, &LedDriver, &hw);
    layer2.Process(3, in, out, size, &layer1, &LedDriver, &hw);
}

// --- Main ---
int main(void)
{
    SetupHardware();
    hw.StartAudio(AudioCallback);
    while(1) {}
}