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

Switch record_play_button;
GPIO input_select_switch;
Switch layer_select_button;

int selected_layer = 0; // 0 = Layer 1, 1 = Layer 2

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

    // Main controls (shared)
    record_play_button.Init(D17, 1000);        // Record/Play button
    input_select_switch.Init(D21, GPIO::Mode::INPUT, GPIO::Pull::PULLUP); // Input select switch
    layer_select_button.Init(A8, 1000);        // Layer select button

    // ADC pins for main controls (3 knobs)
    daisy::Pin adc_pins[3] = {D19, D20, D22};
    AdcChannelConfig adc_cfgs[3];
    for(int i = 0; i < 3; i++)
        adc_cfgs[i].InitSingle(adc_pins[i]);
    hw.adc.Init(adc_cfgs, 3);
    hw.adc.Start();

    // Layer buffers
    for(int i = 0; i < 2; i++)
    {
        layers[i].buffer_l = buffer_l[i];
        layers[i].buffer_r = buffer_r[i];
        layers[i].buffer_size = kBuffSize;
    }
}

void UpdateLEDs()
{
    uint8_t segs = 0x00;
    // Recording/playing status
    if(layers[0].recording) segs |= LED_LAYER1_REC;
    if(layers[0].recorded && !layers[0].recording && !layers[0].paused) segs |= LED_LAYER1_PLAY;
    if(layers[1].recording) segs |= LED_LAYER2_REC;
    if(layers[1].recorded && !layers[1].recording && !layers[1].paused) segs |= LED_LAYER2_PLAY;
    // Selected layer indicator
    if(selected_layer == 0) segs |= LED_LAYER1_Selected;
    if(selected_layer == 1) segs |= LED_LAYER2_Selected;
    LedDriver.Send(1, segs);
}

void AudioCallback(AudioHandle::InputBuffer in,
                   AudioHandle::OutputBuffer out,
                   size_t size)
{
    for(size_t i = 0; i < size; i++)
    {
        out[0][i] = 0.0f;
        out[1][i] = 0.0f;
    }

    // Layer select button debounce and switching
    layer_select_button.Debounce();
    static bool last_layer_btn = false;
    bool layer_btn_pressed = layer_select_button.Pressed();
    if(!last_layer_btn && layer_btn_pressed)
    {
        selected_layer = 1 - selected_layer; // Toggle between 0 and 1
    }
    last_layer_btn = layer_btn_pressed;

    // Process main controls for selected layer
    layers[selected_layer].Process(
        0, // ADC offset is 0, since only 3 ADCs
        in, out, size,
        &record_play_button,
        &input_select_switch,
        &hw
    );

    // Process playback for non-selected layer (no controls, just playback)
    int other_layer = 1 - selected_layer;
    layers[other_layer].ProcessPlaybackOnly(in, out, size, &hw);

    UpdateLEDs();
}

int main(void)
{
    SetupHardware();
    hw.StartAudio(AudioCallback);
    while(1) {}
}