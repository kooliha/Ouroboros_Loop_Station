#include "daisy_seed.h"
#include "daisysp.h"
#include "max7219.h"
#include "looper_layer.h"

using namespace daisy;
using namespace daisy::seed;
using namespace daisysp;

#define kBuffSize (48000 * 66) // 2.5 minutes of floats at 48 kHz

DaisySeed hw;
Max7219 LedDriver;

float DSY_SDRAM_BSS buffer_l[2][kBuffSize];
float DSY_SDRAM_BSS buffer_r[2][kBuffSize];
LooperLayer layers[2];

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

    // Pin assignments for 2 layers
    daisy::Pin button_pins[2] = {D17, D18};
    daisy::Pin input_select_pins[2] = {D21, D25};

    // ADC pins for each layer (3 per layer)
    daisy::Pin adc_pins[6] = {D19, D20, D22, D26, D27, D28};
    AdcChannelConfig adc_cfgs[6];
    for(int i = 0; i < 6; i++)
        adc_cfgs[i].InitSingle(adc_pins[i]);
    hw.adc.Init(adc_cfgs, 6);
    hw.adc.Start();

    for(int i = 0; i < 2; i++)
    {
        static Switch buttons[2];
        static GPIO input_selects[2];
        buttons[i].Init(button_pins[i], 1000);
        input_selects[i].Init(input_select_pins[i], GPIO::Mode::INPUT, GPIO::Pull::PULLUP);

        layers[i].buffer_l = buffer_l[i];
        layers[i].buffer_r = buffer_r[i];
        layers[i].buffer_size = kBuffSize;
        layers[i].button = &buttons[i];
        layers[i].input_select_switch = &input_selects[i];
    }
}

void UpdateLEDs()
{
    uint8_t segs = 0x00;
    if(layers[0].recording) segs |= LED_LAYER1_REC;
    if(layers[0].recorded && !layers[0].recording && !layers[0].paused) segs |= LED_LAYER1_PLAY;
    if(layers[1].recording) segs |= LED_LAYER2_REC;
    if(layers[1].recorded && !layers[1].recording && !layers[1].paused) segs |= LED_LAYER2_PLAY;
    LedDriver.Send(1, segs);
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

    for(int i = 0; i < 2; i++)
        layers[i].Process(i * 3, in, out, size, &LedDriver, i, &hw);

UpdateLEDs();



}




// --- Main ---
int main(void)
{
    SetupHardware();
    hw.StartAudio(AudioCallback);
    while(1) {}
}