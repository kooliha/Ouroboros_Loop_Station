#include "daisy_seed.h"
#include "daisysp.h"

using namespace daisy;
using namespace daisy::seed;
using namespace daisysp;

DaisySeed hw;
Switch Ch1Button;
GPIO ch1_record_led, ch1_play_led, input_select_switch;

// Potentiometer for speed control (T1-Speed)
AdcChannelConfig T1_Speed_adc_cfg;
float T1_Speed = 0.5f; // 0.0 to 1.0

//Pot for volume control
AdcChannelConfig T1_Volume_adc_cfg;
float T1_Volume = 1.0f; // 0.0 (min) to 1.0 (max)


#define kBuffSize (48000 * 30 * 5) // 2.5 minutes of floats at 48 khz

float DSY_SDRAM_BSS buffer_l[kBuffSize];
float DSY_SDRAM_BSS buffer_r[kBuffSize];

size_t record_len = 0;
size_t write_idx = 0;
size_t play_idx = 0;
float play_pos = 0.0f;
bool recording = false;
bool recorded = false;
bool paused = false;

// Variables for Multi-click detection
uint32_t last_release = 0;
int click_count = 0;
const uint32_t double_click_time = 400; // ms

void AudioCallback(AudioHandle::InputBuffer in,
                   AudioHandle::OutputBuffer out,
                   size_t size)
{
    Ch1Button.Debounce();

    // --- Double-click and short click logic ---
    static bool was_pressed = false;
    bool pressed = Ch1Button.Pressed();

    // Start recording on long press
    if(Ch1Button.TimeHeldMs() > 400 && pressed && !recording)
    {
        recording = true;
        write_idx = 0;
        recorded = false;
        paused = false;
        click_count = 0; // cancel click sequence
    }

    // On release
    if(was_pressed && !pressed)
    {
        if(recording)
        {
            // Finish recording
            recording = false;
            record_len = write_idx > 0 ? write_idx : 1;
            play_idx = 0;
            recorded = true;
        }
        else
        {
            // Not recording: count clicks
            uint32_t now = System::GetNow();
            if(now - last_release < double_click_time)
            {
                click_count++;
            }
            else
            {
                click_count = 1;
            }
            last_release = now;

            // Double click: clear buffer
            if(click_count == 2)
            {
                recorded = false;
                record_len = 0;
                play_idx = 0;
                click_count = 0;
                paused = false;
            }
            // Single click: toggle pause/play
            else if(click_count == 1)
            {
                if(recorded)
                    paused = !paused;
            }
        }
    }
    was_pressed = pressed;

    // LED feedback
    ch1_record_led.Write(recording);
    ch1_play_led.Write(recorded && !recording && !paused);

T1_Speed = hw.adc.GetFloat(0); // 0.0 (min) to 1.0 (max)
T1_Volume = 1.0f + (hw.adc.GetFloat(1) - 0.5f) * 4.0f; // 0.0 = -1.0, 0.5 = 1.0, 1.0 = 3.0
if(T1_Volume < 0.0f) T1_Volume = 0.0f;

float Volume_pot = hw.adc.GetFloat(1); // 0.0 to 1.0
T1_Volume = powf(Volume_pot, 2.5f) * 3.0f;    // tweak the exponent for better curve

const float center = 0.5f;
const float dead_zone = 0.09f;

float speed = 1.0f;

if(T1_Speed < center - dead_zone)
{
    // Left side: 0.3x to 1.0x
    float t = (T1_Speed - 0.0f) / (center - dead_zone); // 0 to 1
    speed = 0.3f + t * (1.0f - 0.3f); // 0.3 to 1.0
}
else if(T1_Speed > center + dead_zone)
{
    // Right side: 1.0x to 2.0x
    float t = (T1_Speed - (center + dead_zone)) / (1.0f - (center + dead_zone)); // 0 to 1
    speed = 1.0f + t * (2.0f - 1.0f); // 1.0 to 2.0
}
else
{
    // Inside dead zone
    speed = 1.0f;
}

    bool input_selection = (input_select_switch.Read() == 0); // LOW = mic, HIGH = guitar
    for(size_t i = 0; i < size; i++)
    {
    float mic_in = in[0][i];      // Microphone input (left)
    float guitar_in = in[1][i];   // Guitar input (right)
    float selected_input = input_selection ? mic_in : guitar_in;

        if(recording)
        {
            if(write_idx < kBuffSize)
            {
                buffer_l[write_idx] = selected_input;
                buffer_r[write_idx] = selected_input;
                write_idx++;
            }
            out[0][i] = selected_input * T1_Volume;
            out[1][i] = selected_input * T1_Volume;
        }

        else if(recorded && record_len > 0 && !paused)
        {
            // Interpolated playback for variable speed
            int idx0 = (int)play_pos;
            int idx1 = (idx0 + 1) % record_len;
            float frac = play_pos - idx0;

            out[0][i] = (buffer_l[idx0] * (1.0f - frac) + buffer_l[idx1] * frac) * T1_Volume;
            out[1][i] = (buffer_r[idx0] * (1.0f - frac) + buffer_r[idx1] * frac) * T1_Volume;

            play_pos += speed;
            while(play_pos >= record_len) play_pos -= record_len;
            while(play_pos < 0) play_pos += record_len;
        }

        else
        {
            out[0][i] = 0.0f;
            out[1][i] = 0.0f;
        }
    }
}



int main(void)
{
    hw.Configure();
    hw.Init();

    // Init button and LEDs
    Ch1Button.Init(hw.GetPin(17), 1000);
    ch1_record_led.Init(D15, GPIO::Mode::OUTPUT);
    ch1_play_led.Init(D16, GPIO::Mode::OUTPUT);
    input_select_switch.Init(D21, GPIO::Mode::INPUT, GPIO::Pull::PULLUP);

    // Init ADC for potentiometer (T1-Speed) on D15/A0
    T1_Speed_adc_cfg.InitSingle(D19);
    T1_Volume_adc_cfg.InitSingle(D20);
    AdcChannelConfig adc_cfgs[2] = {T1_Speed_adc_cfg, T1_Volume_adc_cfg};
    hw.adc.Init(adc_cfgs, 2);
    hw.adc.Start();

    hw.StartAudio(AudioCallback);
    while(1) {}//test
}
