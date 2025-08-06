#include "looper_layer.h"
#include <math.h>

void LooperLayer::Reset()
{
    record_len = 0;
    write_idx = 0;
    play_pos = 0.0f;
    recording = false;
    recorded = false;
    paused = false;
    click_count = 0;
}

void LooperLayer::Process(int adc_offset,
                          AudioHandle::InputBuffer in,
                          AudioHandle::OutputBuffer out,
                          size_t size,
                          Switch* record_play_button,
                          GPIO* input_select_switch,
                          DaisySeed* hw,
                          int selected_channel)
{
    record_play_button->Debounce();

    static bool was_pressed = false;
    bool pressed = record_play_button->Pressed();

    // Start recording on long press
    if(record_play_button->TimeHeldMs() > 400 && pressed && !recording)
    {
        recording = true;
        write_idx = 0;
        recorded = false;
        paused = false;
        click_count = 0;
    }

    // On release
    if(was_pressed && !pressed)
    {
        if(recording)
        {
            recording = false;
            record_len = write_idx > 0 ? write_idx : 1;
            play_pos = 0.0f;
            recorded = true;
            recorded_channel = selected_channel;
        }
        else
        {
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

            if(click_count == 2)
            {
                recorded = false;
                record_len = 0;
                play_pos = 0.0f;
                click_count = 0;
                paused = false;
            }
            else if(click_count == 1)
            {
                if(recorded)
                    paused = !paused;
            }
        }
    }
    was_pressed = pressed;

    // --- Read pots for this layer ---
    float speed_pot = hw->adc.GetFloat(adc_offset + 0);
    float vol_pot   = hw->adc.GetFloat(adc_offset + 1);
    float pan_pot   = hw->adc.GetFloat(adc_offset + 2);

    // Speed logic
    const float center = 0.5f;
    const float dead_zone = 0.09f;
    if(speed_pot < center - dead_zone)
    {
        float tt = (speed_pot - 0.0f) / (center - dead_zone);
        speed = 0.3f + tt * (1.0f - 0.3f);
    }
    else if(speed_pot > center + dead_zone)
    {
        float tt = (speed_pot - (center + dead_zone)) / (1.0f - (center + dead_zone));
        speed = 1.0f + tt * (2.0f - 1.0f);
    }
    else
    {
        speed = 1.0f;
    }

    volume = powf(vol_pot, 2.5f) * 3.0f;
    if(volume < 0.0f) volume = 0.0f;
    pan = pan_pot;
    float panL = 1.0f - pan;
    float panR = pan;

    // --- Input selection ---
    bool input_selection = (input_select_switch->Read() == 0); // LOW = mic, HIGH = guitar

    for(size_t i = 0; i < size; i++)
    {
        float mic_in = in[0][i];
        float guitar_in = in[1][i];
        float selected_input = input_selection ? mic_in : guitar_in;

        if(recording)
        {
            if(write_idx < buffer_size)
            {
                buffer_l[write_idx] = selected_input;
                buffer_r[write_idx] = selected_input;
                write_idx++;
            }
            out[0][i] += selected_input * volume;
            out[1][i] += selected_input * volume;
        }
        else if(recorded && record_len > 0 && !paused)
        {
            int idx0 = (int)play_pos;
            int idx1 = (idx0 + 1) % record_len;
            float frac = play_pos - idx0;

            out[0][i] += (buffer_l[idx0] * (1.0f - frac) + buffer_l[idx1] * frac) * volume * panL;
            out[1][i] += (buffer_r[idx0] * (1.0f - frac) + buffer_r[idx1] * frac) * volume * panR;

            play_pos += speed;
            while(play_pos >= record_len) play_pos -= record_len;
            while(play_pos < 0) play_pos += record_len;
        }
        else
        {
            // Do not clear output here, so layers can mix
        }
    }
}

void LooperLayer::ProcessPlaybackOnly(AudioHandle::InputBuffer in,
                                      AudioHandle::OutputBuffer out,
                                      size_t size,
                                      DaisySeed* hw)
{
    // Only playback, no controls
    if(recorded && record_len > 0 && !paused)
    {
        float panL = 1.0f - pan;
        float panR = pan;
        for(size_t i = 0; i < size; i++)
        {
            int idx0 = (int)play_pos;
            int idx1 = (idx0 + 1) % record_len;
            float frac = play_pos - idx0;

            out[0][i] += (buffer_l[idx0] * (1.0f - frac) + buffer_l[idx1] * frac) * volume * panL;
            out[1][i] += (buffer_r[idx0] * (1.0f - frac) + buffer_r[idx1] * frac) * volume * panR;

            play_pos += speed;
            while(play_pos >= record_len) play_pos -= record_len;
            while(play_pos < 0) play_pos += record_len;
        }
    }
}