#include "daisy_seed.h"
#include "daisysp.h"
#include "max7219.h"
#include "looper_layer.h"

using namespace daisy;
using namespace daisy::seed;
using namespace daisysp;

#define kBuffSize 1600000 // ~33 seconds at 48kHz per each layer (5 in total)
#define kNumLayers 5

// ===== PIN DEFINITIONS =====
// SPI pins for MAX7219 LED driver
constexpr Pin SPI_SCLK = D8;
constexpr Pin SPI_MISO = D9;
constexpr Pin SPI_MOSI = D10;
constexpr Pin SPI_CS   = D7;

// Control buttons
constexpr Pin RECORD_PLAY_BTN = D17;
constexpr Pin INPUT_SELECT_SW = D21;
constexpr Pin CHANNEL_BTN     = D15;

// Layer select buttons
constexpr Pin LAYER1_BTN = D14;
constexpr Pin LAYER2_BTN = D13;
constexpr Pin LAYER3_BTN = D12;
constexpr Pin LAYER4_BTN = D11;
constexpr Pin LAYER5_BTN = D6;

// Relay control pins
constexpr Pin INSTRUMENTAL_RELAY = D25;
constexpr Pin LINE_RELAY         = D26;

// ADC pins for knobs
constexpr Pin SPEED_POT  = D19;
constexpr Pin VOLUME_POT = D20;
constexpr Pin PAN_POT    = D22;

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

// Relay control pins
GPIO instrumental_relay; // D25 - controls instrumental input relay
GPIO line_relay;         // D26 - controls line input relay

// Layer selection
int selected_layer = 0; // 0 = Layer 1, 1 = Layer 2, ..., 4 = Layer 5

// Input selection
int selected_channel = 0; // 0 = Guitar, 1 = Mic, 2 = Line
bool channel_override_active = false; // True when user has manually changed channel

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
    spi_cfg.pin_config.sclk = SPI_SCLK;
    spi_cfg.pin_config.miso = SPI_MISO;
    spi_cfg.pin_config.mosi = SPI_MOSI;

    static SpiHandle spi;
    spi.Init(spi_cfg);

    LedDriver.Init(&spi, SPI_CS);

    // Main controls (shared)
    record_play_button.Init(RECORD_PLAY_BTN, 300);        // Record/Play button
    input_select_switch.Init(INPUT_SELECT_SW, GPIO::Mode::INPUT, GPIO::Pull::PULLUP); // Input select switch

    // Layer select buttons
    layer1_select_button.Init(LAYER1_BTN, 300); // Layer 1 select button
    layer2_select_button.Init(LAYER2_BTN, 300); // Layer 2 select button
    layer3_select_button.Init(LAYER3_BTN, 300); // Layer 3 select button
    layer4_select_button.Init(LAYER4_BTN, 300); // Layer 4 select button
    layer5_select_button.Init(LAYER5_BTN, 300); // Layer 5 select button

    channel_button.Init(CHANNEL_BTN, 300); // Channel select button, fast debounce

    // Initialize relay control pins
    instrumental_relay.Init(INSTRUMENTAL_RELAY, GPIO::Mode::OUTPUT);
    line_relay.Init(LINE_RELAY, GPIO::Mode::OUTPUT);
    
    // Start with both relays OFF (LOW = off, HIGH = on for 2N3904)
    instrumental_relay.Write(false);
    line_relay.Write(false);

    // ADC pins for main controls (3 knobs)
    daisy::Pin adc_pins[3] = {SPEED_POT, VOLUME_POT, PAN_POT};
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
    int channel_to_show;
    
    // If user has manually changed channel, show the selected_channel
    // Otherwise, show the recorded channel for recorded layers
    if(channel_override_active || !layers[selected_layer].recorded)
        channel_to_show = selected_channel;
    else
        channel_to_show = layers[selected_layer].recorded_channel;

    uint8_t segs = 0x00;
    if(channel_to_show == 0) segs = LED_CHANNEL_GUITAR.segment;
    else if(channel_to_show == 1) segs = LED_CHANNEL_MIC.segment;
    else if(channel_to_show == 2) segs = LED_CHANNEL_LINE.segment;

    LedDriver.Send(LED_CHANNEL_GUITAR.digit, segs); // All are on Dig2
}

void UpdateRelays()
{
    // Control relays based on selected channel
    // Channel 0 = Guitar (through instrumental relay), Channel 1 = Mic (through instrumental relay), Channel 2 = Line (through line relay)
    
    switch(selected_channel)
    {
        case 0: // Guitar
            instrumental_relay.Write(true);  // Activate instrumental relay (will select left/right channel in hardware)
            line_relay.Write(false);         // Deactivate line relay
            break;
            
        case 1: // Mic  
            instrumental_relay.Write(true);  // Activate instrumental relay (will select left/right channel in hardware)
            line_relay.Write(false);         // Deactivate line relay
            break;
            
        case 2: // Line
            instrumental_relay.Write(false); // Deactivate instrumental relay  
            line_relay.Write(true);          // Activate line relay
            break;
    }
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
            channel_override_active = false; // Reset override when switching layers
            // Set selected_channel to match the recorded channel (if any)
            if(layers[selected_layer].recorded)
                selected_channel = layers[selected_layer].recorded_channel;
            UpdateRelays(); // Update relay state when switching layers
        }
        last_layer_btn[i] = layer_btn_pressed[i];
    }

    // Process main controls for selected layer
    layers[selected_layer].Process(
        0, // ADC offset is 0, since only 3 ADCs
        in, out, size,
        &record_play_button,
        &input_select_switch,
        &hw,
        selected_channel
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
        channel_override_active = true; // User has manually changed channel
        UpdateRelays(); // Update relay state when channel button is pressed
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