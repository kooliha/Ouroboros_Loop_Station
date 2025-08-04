#include "daisy_seed.h"
#include "daisysp.h"
#include "max7219.h"
#include "looper_layer.h"

using namespace daisy;
using namespace daisy::seed;
using namespace daisysp;

#define kBuffSize 1600000 // ~33 seconds at 48kHz per each layer (5 in total)
#define kNumLayers 5

DaisySeed hw;
Max7219 LedDriver;

float DSY_SDRAM_BSS buffer_l[kNumLayers][kBuffSize];
float DSY_SDRAM_BSS buffer_r[kNumLayers][kBuffSize];

LooperLayer layers[kNumLayers];

Switch record_play_button;
GPIO input_select_switch;
Switch layer1_select_button;
Switch layer2_select_button;
Switch layer3_select_button;
Switch layer4_select_button;
Switch layer5_select_button;

Switch channel_button;

// Layer selection
int selected_layer = 0; // 0 = Layer 1, 1 = Layer 2, ..., 4 = Layer 5

// Input selection
int selected_channel = 0; // 0 = Guitar, 1 = Mic, 2 = Line

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
    record_play_button.Init(D17, 300);        // Record/Play button
    input_select_switch.Init(D21, GPIO::Mode::INPUT, GPIO::Pull::PULLUP); // Input select switch

    // Layer select buttons
    layer1_select_button.Init(D14, 300); // Layer 1 select button
    layer2_select_button.Init(D13, 300); // Layer 2 select button
    layer3_select_button.Init(D12, 300); // Layer 3 select button
    layer4_select_button.Init(D11, 300); // Layer 4 select button
    layer5_select_button.Init(D6, 300);  // Layer 5 select button

    channel_button.Init(D15, 300); // Channel select button, fast debounce

    // ADC pins for main controls (3 knobs)
    daisy::Pin adc_pins[3] = {D19, D20, D22};
    AdcChannelConfig adc_cfgs[3];
    for(int i = 0; i < 3; i++)
        adc_cfgs[i].InitSingle(adc_pins[i]);
    hw.adc.Init(adc_cfgs, 3);
    hw.adc.Start();

    // Layer buffers
    for(int i = 0; i < kNumLayers; i++)
    {
        layers[i].buffer_l = buffer_l[i];
        layers[i].buffer_r = buffer_r[i];
        layers[i].buffer_size = kBuffSize;
        layers[i].paused = false;
    }
}

void UpdateChannelLEDs()
{
    uint8_t segs = 0x00;
    if(selected_channel == 0) segs = LED_CHANNEL_GUITAR.segment;
    else if(selected_channel == 1) segs = LED_CHANNEL_MIC.segment;
    else if(selected_channel == 2) segs = LED_CHANNEL_LINE.segment;

    LedDriver.Send(LED_CHANNEL_GUITAR.digit, segs); // All are on Dig2
}

void UpdateLEDs()
{
    uint8_t segs_dig0 = 0x00;
    uint8_t segs_dig1 = 0x00;

    // Dig0: Recording/playing status for layers 1-4
    if(layers[0].recording) segs_dig0 |= LED_LAYER1_REC.segment;
    if(layers[0].recorded && !layers[0].recording && !layers[0].paused) segs_dig0 |= LED_LAYER1_PLAY.segment;
    if(layers[1].recording) segs_dig0 |= LED_LAYER2_REC.segment;
    if(layers[1].recorded && !layers[1].recording && !layers[1].paused) segs_dig0 |= LED_LAYER2_PLAY.segment;
    if(layers[2].recording) segs_dig0 |= LED_LAYER3_REC.segment;
    if(layers[2].recorded && !layers[2].recording && !layers[2].paused) segs_dig0 |= LED_LAYER3_PLAY.segment;
    if(layers[3].recording) segs_dig0 |= LED_LAYER4_REC.segment;
    if(layers[3].recorded && !layers[3].recording && !layers[3].paused) segs_dig0 |= LED_LAYER4_PLAY.segment;

    // Dig1: Layer 5 play/rec
    if(layers[4].recording) segs_dig1 |= LED_LAYER5_REC.segment;
    if(layers[4].recorded && !layers[4].recording && !layers[4].paused) segs_dig1 |= LED_LAYER5_PLAY.segment;

    // Dig1: Selected layer indicator for all layers
    if(selected_layer == 0) segs_dig1 |= LED_LAYER1_Selected.segment;
    if(selected_layer == 1) segs_dig1 |= LED_LAYER2_Selected.segment;
    if(selected_layer == 2) segs_dig1 |= LED_LAYER3_Selected.segment;
    if(selected_layer == 3) segs_dig1 |= LED_LAYER4_Selected.segment;
    if(selected_layer == 4) segs_dig1 |= LED_LAYER5_Selected.segment;

    LedDriver.Send(LED_LAYER1_PLAY.digit, segs_dig0); // Dig0
    LedDriver.Send(LED_LAYER1_Selected.digit, segs_dig1); // Dig1
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

    // Layer select buttons debounce and switching
    layer1_select_button.Debounce();
    layer2_select_button.Debounce();
    layer3_select_button.Debounce();
    layer4_select_button.Debounce();
    layer5_select_button.Debounce();

    static bool last_layer_btn[5] = {false, false, false, false, false};
    bool layer_btn_pressed[5] = {
        layer1_select_button.Pressed(),
        layer2_select_button.Pressed(),
        layer3_select_button.Pressed(),
        layer4_select_button.Pressed(),
        layer5_select_button.Pressed()
    };

    for(int i = 0; i < kNumLayers; i++)
    {
        if(!last_layer_btn[i] && layer_btn_pressed[i])
        {
            selected_layer = i;
        }
        last_layer_btn[i] = layer_btn_pressed[i];
    }

    // Process main controls for selected layer
    layers[selected_layer].Process(
        0, // ADC offset is 0, since only 3 ADCs
        in, out, size,
        &record_play_button,
        &input_select_switch,
        &hw
    );

    // Process playback for all non-selected layers
    for(int i = 0; i < kNumLayers; i++)
    {
        if(i != selected_layer)
            layers[i].ProcessPlaybackOnly(in, out, size, &hw);
    }

    UpdateLEDs();

    // Channel selection switch
       channel_button.Debounce();

    static bool last_btn = false;
    bool btn_pressed = channel_button.Pressed();

    if(!last_btn && btn_pressed)
    {
        selected_channel = (selected_channel + 1) % 3;
    }
    last_btn = btn_pressed;

    UpdateChannelLEDs();
}

int main(void)
{
    SetupHardware();
    hw.StartAudio(AudioCallback);
    while(1) {}
}