#include "daisy_seed.h"
#include "daisysp.h"
#include "max7219.h"

using namespace daisy;
using namespace daisy::seed;
using namespace daisysp;


// --- Layer Structure ---
struct LooperLayer
{
    float* buffer_l;
    float* buffer_r;
    size_t buffer_size;

    size_t record_len = 0;
    size_t write_idx = 0;
    float play_pos = 0.0f;

    bool recording = false;
    bool recorded = false;
    bool paused = false;

    // Controls
    float speed = 1.0f;
    float volume = 1.0f;
    float pan = 0.5f;

    // Hardware
    Switch* button = nullptr;
    GPIO* input_select_switch = nullptr;

    // Multi-click detection
    uint32_t last_release = 0;
    int click_count = 0;
    static constexpr uint32_t double_click_time = 400; // ms

    void Reset()
    {
        record_len = 0;
        write_idx = 0;
        play_pos = 0.0f;
        recording = false;
        recorded = false;
        paused = false;
        click_count = 0;
    }
};

// --- Globals ---
DaisySeed hw;
Max7219 LedDriver;

#define kBuffSize (48000 * 66) // 2.5 minutes of floats at 48 kHz

float DSY_SDRAM_BSS buffer_l1[kBuffSize];
float DSY_SDRAM_BSS buffer_r1[kBuffSize];
float DSY_SDRAM_BSS buffer_l2[kBuffSize];
float DSY_SDRAM_BSS buffer_r2[kBuffSize];

LooperLayer layer1;
LooperLayer layer2;

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
    spi_cfg.pin_config.sclk = daisy::seed::D8;   // SCK
    spi_cfg.pin_config.miso = daisy::seed::D9;   // MISO (not used for MAX7219)
    spi_cfg.pin_config.mosi = daisy::seed::D10;  // MOSI

    static SpiHandle spi;
    spi.Init(spi_cfg);

    LedDriver.Init(&spi, daisy::seed::D7); // Use D7 for CS/LOAD

    // Layer 1 hardware
    static Switch l1_button;
    static GPIO l1_input_select;
    l1_button.Init(hw.GetPin(17), 1000);
    l1_input_select.Init(D21, GPIO::Mode::INPUT, GPIO::Pull::PULLUP);

    layer1.buffer_l = buffer_l1;
    layer1.buffer_r = buffer_r1;
    layer1.buffer_size = kBuffSize;
    layer1.button = &l1_button;
    layer1.input_select_switch = &l1_input_select;

    // Layer 2 hardware
    static Switch l2_button;
    static GPIO l2_input_select;
    l2_button.Init(hw.GetPin(18), 1000);
    l2_input_select.Init(D25, GPIO::Mode::INPUT, GPIO::Pull::PULLUP);

    layer2.buffer_l = buffer_l2;
    layer2.buffer_r = buffer_r2;
    layer2.buffer_size = kBuffSize;
    layer2.button = &l2_button;
    layer2.input_select_switch = &l2_input_select;

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

// --- Layer Processing ---
void ProcessLayer(LooperLayer& l, int adc_offset, AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size)
{
    l.button->Debounce();

    // --- Button logic ---
    static bool was_pressed[2] = {false, false};
    int layer_idx = (l.button == layer1.button) ? 0 : 1;
    bool pressed = l.button->Pressed();

    // Start recording on long press
    if(l.button->TimeHeldMs() > 400 && pressed && !l.recording)
    {
        l.recording = true;
        l.write_idx = 0;
        l.recorded = false;
        l.paused = false;
        l.click_count = 0;
    }

    // On release
    if(was_pressed[layer_idx] && !pressed)
    {
        if(l.recording)
        {
            l.recording = false;
            l.record_len = l.write_idx > 0 ? l.write_idx : 1;
            l.play_pos = 0.0f;
            l.recorded = true;
        }
        else
        {
            uint32_t now = System::GetNow();
            if(now - l.last_release < l.double_click_time)
            {
                l.click_count++;
            }
            else
            {
                l.click_count = 1;
            }
            l.last_release = now;

            if(l.click_count == 2)
            {
                l.recorded = false;
                l.record_len = 0;
                l.play_pos = 0.0f;
                l.click_count = 0;
                l.paused = false;
            }
            else if(l.click_count == 1)
            {
                if(l.recorded)
                    l.paused = !l.paused;
            }
        }
    }
    was_pressed[layer_idx] = pressed;

    // --- LED feedback using MAX7219 ---
    uint8_t segs = 0x00;
    if(layer1.recording) segs |= LED_LAYER1_REC;
    if(layer1.recorded && !layer1.recording && !layer1.paused) segs |= LED_LAYER1_PLAY;
    if(layer2.recording) segs |= LED_LAYER2_REC;
    if(layer2.recorded && !layer2.recording && !layer2.paused) segs |= LED_LAYER2_PLAY;
    LedDriver.Send(1, segs);

    // --- Read pots for this layer ---
    float speed_pot, vol_pot, pan_pot;
    if(&l == &layer2) {
        speed_pot = hw.adc.GetFloat(adc_offset + 0);
        vol_pot   = hw.adc.GetFloat(adc_offset + 1);
        pan_pot   = hw.adc.GetFloat(adc_offset + 2);
    } else {
        speed_pot = hw.adc.GetFloat(adc_offset + 0);
        vol_pot   = hw.adc.GetFloat(adc_offset + 1);
        pan_pot   = hw.adc.GetFloat(adc_offset + 2);
    }

    // Speed logic
    const float center = 0.5f;
    const float dead_zone = 0.09f;
    if(speed_pot < center - dead_zone)
    {
        float tt = (speed_pot - 0.0f) / (center - dead_zone);
        l.speed = 0.3f + tt * (1.0f - 0.3f);
    }
    else if(speed_pot > center + dead_zone)
    {
        float tt = (speed_pot - (center + dead_zone)) / (1.0f - (center + dead_zone));
        l.speed = 1.0f + tt * (2.0f - 1.0f);
    }
    else
    {
        l.speed = 1.0f;
    }

    l.volume = powf(vol_pot, 2.5f) * 3.0f;
    if(l.volume < 0.0f) l.volume = 0.0f;
    l.pan = pan_pot;
    float panL = 1.0f - l.pan;
    float panR = l.pan;

    // --- Input selection ---
    bool input_selection = (l.input_select_switch->Read() == 0); // LOW = mic, HIGH = guitar

    for(size_t i = 0; i < size; i++)
    {
        float mic_in = in[0][i];
        float guitar_in = in[1][i];
        float selected_input = input_selection ? mic_in : guitar_in;

        if(l.recording)
        {
            if(l.write_idx < l.buffer_size)
            {
                l.buffer_l[l.write_idx] = selected_input;
                l.buffer_r[l.write_idx] = selected_input;
                l.write_idx++;
            }
            out[0][i] += selected_input * l.volume;
            out[1][i] += selected_input * l.volume;
        }
        else if(l.recorded && l.record_len > 0 && !l.paused)
        {
            int idx0 = (int)l.play_pos;
            int idx1 = (idx0 + 1) % l.record_len;
            float frac = l.play_pos - idx0;

            out[0][i] += (l.buffer_l[idx0] * (1.0f - frac) + l.buffer_l[idx1] * frac) * l.volume * panL;
            out[1][i] += (l.buffer_r[idx0] * (1.0f - frac) + l.buffer_r[idx1] * frac) * l.volume * panR;

            l.play_pos += l.speed;
            while(l.play_pos >= l.record_len) l.play_pos -= l.record_len;
            while(l.play_pos < 0) l.play_pos += l.record_len;
        }
        else
        {
            // Do not clear output here, so layers can mix
        }
    }
}

// --- Audio Callback ---
void AudioCallback(AudioHandle::InputBuffer in,
                   AudioHandle::OutputBuffer out,
                   size_t size)
{
    // Zero outputs before mixing
    for(size_t i = 0; i < size; i++)
    {
        out[0][i] = 0.0f;
        out[1][i] = 0.0f;
    }

    // Process both layers (adc_offset: 0 for layer1, 3 for layer2)
    ProcessLayer(layer1, 0, in, out, size);
    ProcessLayer(layer2, 3, in, out, size);
}

// --- Main ---
int main(void)
{
    SetupHardware();
    hw.StartAudio(AudioCallback);
    while(1) {}
}