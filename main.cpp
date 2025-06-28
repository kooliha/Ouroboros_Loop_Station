#include "daisy_seed.h"
#include "daisysp.h"

using namespace daisy;
using namespace daisy::seed;
using namespace daisysp;

// --- Track Structure ---
struct LooperTrack
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
    GPIO* record_led = nullptr;
    GPIO* play_led = nullptr;
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

#define kBuffSize (48000 * 30 * 5) // 2.5 minutes of floats at 48 kHz

float DSY_SDRAM_BSS buffer_l1[kBuffSize];
float DSY_SDRAM_BSS buffer_r1[kBuffSize];

LooperTrack track1;

// --- Hardware Setup ---
void SetupHardware()
{
    hw.Configure();
    hw.Init();

    // Track 1 hardware
    static Switch ch1_button;
    static GPIO ch1_record_led, ch1_play_led, ch1_input_select;
    ch1_button.Init(hw.GetPin(17), 1000);
    ch1_record_led.Init(D15, GPIO::Mode::OUTPUT);
    ch1_play_led.Init(D16, GPIO::Mode::OUTPUT);
    ch1_input_select.Init(D21, GPIO::Mode::INPUT, GPIO::Pull::PULLUP);

    // Assign to struct
    track1.buffer_l = buffer_l1;
    track1.buffer_r = buffer_r1;
    track1.buffer_size = kBuffSize;
    track1.button = &ch1_button;
    track1.record_led = &ch1_record_led;
    track1.play_led = &ch1_play_led;
    track1.input_select_switch = &ch1_input_select;

    // ADC setup for speed, volume, pan
    static AdcChannelConfig speed_adc, volume_adc, pan_adc;
    speed_adc.InitSingle(D19);
    volume_adc.InitSingle(D20);
    pan_adc.InitSingle(D22);
    AdcChannelConfig adc_cfgs[3] = {speed_adc, volume_adc, pan_adc};
    hw.adc.Init(adc_cfgs, 3);
    hw.adc.Start();
}

// --- Track Processing ---
void ProcessTrack(LooperTrack& t, AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size)
{
    t.button->Debounce();

    // --- Button logic ---
    static bool was_pressed = false;
    bool pressed = t.button->Pressed();

    // Start recording on long press
    if(t.button->TimeHeldMs() > 400 && pressed && !t.recording)
    {
        t.recording = true;
        t.write_idx = 0;
        t.recorded = false;
        t.paused = false;
        t.click_count = 0;
    }

    // On release
    if(was_pressed && !pressed)
    {
        if(t.recording)
        {
            // Finish recording
            t.recording = false;
            t.record_len = t.write_idx > 0 ? t.write_idx : 1;
            t.play_pos = 0.0f;
            t.recorded = true;
        }
        else
        {
            // Not recording: count clicks
            uint32_t now = System::GetNow();
            if(now - t.last_release < t.double_click_time)
            {
                t.click_count++;
            }
            else
            {
                t.click_count = 1;
            }
            t.last_release = now;

            // Double click: clear buffer
            if(t.click_count == 2)
            {
                t.recorded = false;
                t.record_len = 0;
                t.play_pos = 0.0f;
                t.click_count = 0;
                t.paused = false;
            }
            // Single click: toggle pause/play
            else if(t.click_count == 1)
            {
                if(t.recorded)
                    t.paused = !t.paused;
            }
        }
    }
    was_pressed = pressed;

    // LED feedback
    t.record_led->Write(t.recording);
    t.play_led->Write(t.recorded && !t.recording && !t.paused);

    // --- Read pots ---
    float speed_pot = hw.adc.GetFloat(0);
    float vol_pot = hw.adc.GetFloat(1);
    float pan_pot = hw.adc.GetFloat(2);

    // Speed logic
    const float center = 0.5f;
    const float dead_zone = 0.09f;
    if(speed_pot < center - dead_zone)
    {
        float tt = (speed_pot - 0.0f) / (center - dead_zone);
        t.speed = 0.3f + tt * (1.0f - 0.3f);
    }
    else if(speed_pot > center + dead_zone)
    {
        float tt = (speed_pot - (center + dead_zone)) / (1.0f - (center + dead_zone));
        t.speed = 1.0f + tt * (2.0f - 1.0f);
    }
    else
    {
        t.speed = 1.0f;
    }

    t.volume = powf(vol_pot, 2.5f) * 3.0f;
    if(t.volume < 0.0f) t.volume = 0.0f;
    t.pan = pan_pot;
    float panL = 1.0f - t.pan;
    float panR = t.pan;

    // --- Input selection ---
    bool input_selection = (t.input_select_switch->Read() == 0); // LOW = mic, HIGH = guitar

    for(size_t i = 0; i < size; i++)
    {
        float mic_in = in[0][i];
        float guitar_in = in[1][i];
        float selected_input = input_selection ? mic_in : guitar_in;

        if(t.recording)
        {
            if(t.write_idx < t.buffer_size)
            {
                t.buffer_l[t.write_idx] = selected_input;
                t.buffer_r[t.write_idx] = selected_input;
                t.write_idx++;
            }
            out[0][i] = selected_input * t.volume;
            out[1][i] = selected_input * t.volume;
        }
        else if(t.recorded && t.record_len > 0 && !t.paused)
        {
            // Interpolated playback for variable speed
            int idx0 = (int)t.play_pos;
            int idx1 = (idx0 + 1) % t.record_len;
            float frac = t.play_pos - idx0;

            out[0][i] = (t.buffer_l[idx0] * (1.0f - frac) + t.buffer_l[idx1] * frac) * t.volume * panL;
            out[1][i] = (t.buffer_r[idx0] * (1.0f - frac) + t.buffer_r[idx1] * frac) * t.volume * panR;

            t.play_pos += t.speed;
            while(t.play_pos >= t.record_len) t.play_pos -= t.record_len;
            while(t.play_pos < 0) t.play_pos += t.record_len;
        }
        else
        {
            out[0][i] = 0.0f;
            out[1][i] = 0.0f;
        }
    }
}

// --- Audio Callback ---
void AudioCallback(AudioHandle::InputBuffer in,
                   AudioHandle::OutputBuffer out,
                   size_t size)
{
    ProcessTrack(track1, in, out, size);
    // For more tracks, call ProcessTrack for each track
}

// --- Main ---
int main(void)
{
    SetupHardware();
    hw.StartAudio(AudioCallback);
    while(1) {}
}