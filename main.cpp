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

#define kBuffSize (48000 * 66) // 2.5 minutes of floats at 48 kHz

float DSY_SDRAM_BSS buffer_l1[kBuffSize];
float DSY_SDRAM_BSS buffer_r1[kBuffSize];
float DSY_SDRAM_BSS buffer_l2[kBuffSize];
float DSY_SDRAM_BSS buffer_r2[kBuffSize];

LooperTrack track1;
LooperTrack track2;

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

    track1.buffer_l = buffer_l1;
    track1.buffer_r = buffer_r1;
    track1.buffer_size = kBuffSize;
    track1.button = &ch1_button;
    track1.record_led = &ch1_record_led;
    track1.play_led = &ch1_play_led;
    track1.input_select_switch = &ch1_input_select;



    
    // Track 2 hardware
    static Switch ch2_button;
    static GPIO ch2_record_led, ch2_play_led, ch2_input_select;
    ch2_button.Init(hw.GetPin(18), 1000);
    ch2_record_led.Init(D23, GPIO::Mode::OUTPUT);
    ch2_play_led.Init(D24, GPIO::Mode::OUTPUT);
    ch2_input_select.Init(D25, GPIO::Mode::INPUT, GPIO::Pull::PULLUP);

    track2.buffer_l = buffer_l2;
    track2.buffer_r = buffer_r2;
    track2.buffer_size = kBuffSize;
    track2.button = &ch2_button;
    track2.record_led = &ch2_record_led;
    track2.play_led = &ch2_play_led;
    track2.input_select_switch = &ch2_input_select;

    // ADC setup for speed, volume, pan for both tracks
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

// --- Track Processing ---
void ProcessTrack(LooperTrack& t, int adc_offset, AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size)
{
    t.button->Debounce();

    // --- Button logic ---
    static bool was_pressed[2] = {false, false};
    int track_idx = (t.button == track1.button) ? 0 : 1;
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
    if(was_pressed[track_idx] && !pressed)
    {
        if(t.recording)
        {
            t.recording = false;
            t.record_len = t.write_idx > 0 ? t.write_idx : 1;
            t.play_pos = 0.0f;
            t.recorded = true;
        }
        else
        {
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

            if(t.click_count == 2)
            {
                t.recorded = false;
                t.record_len = 0;
                t.play_pos = 0.0f;
                t.click_count = 0;
                t.paused = false;
            }
            else if(t.click_count == 1)
            {
                if(t.recorded)
                    t.paused = !t.paused;
            }
        }
    }
    was_pressed[track_idx] = pressed;

    // LED feedback
    t.record_led->Write(t.recording);
    t.play_led->Write(t.recorded && !t.recording && !t.paused);

    // --- Read pots for this track ---
   // float speed_pot = hw.adc.GetFloat(adc_offset + 0);
  //  float vol_pot   = hw.adc.GetFloat(adc_offset + 1);
   // float pan_pot   = hw.adc.GetFloat(adc_offset + 2);

   float speed_pot, vol_pot, pan_pot;
if(&t == &track2) {
    speed_pot = 0.5f;
    vol_pot   = 1.0f;
    pan_pot   = 0.5f;
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
            out[0][i] += selected_input * t.volume;
            out[1][i] += selected_input * t.volume;
        }
        else if(t.recorded && t.record_len > 0 && !t.paused)
        {
            int idx0 = (int)t.play_pos;
            int idx1 = (idx0 + 1) % t.record_len;
            float frac = t.play_pos - idx0;

            out[0][i] += (t.buffer_l[idx0] * (1.0f - frac) + t.buffer_l[idx1] * frac) * t.volume * panL;
            out[1][i] += (t.buffer_r[idx0] * (1.0f - frac) + t.buffer_r[idx1] * frac) * t.volume * panR;

            t.play_pos += t.speed;
            while(t.play_pos >= t.record_len) t.play_pos -= t.record_len;
            while(t.play_pos < 0) t.play_pos += t.record_len;
        }
        else
        {
            // Do not clear output here, so tracks can mix
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

    // Process both tracks (adc_offset: 0 for track1, 3 for track2)
    ProcessTrack(track1, 0, in, out, size);
    ProcessTrack(track2, 3, in, out, size);
}

// --- Main ---
int main(void)
{
    SetupHardware();
    hw.StartAudio(AudioCallback);
    while(1) {}
}