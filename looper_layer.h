#pragma once
#include "daisy_seed.h"
#include "daisysp.h"
#include "max7219.h"

using namespace daisy;
using namespace daisy::seed;

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

    void Reset();

    void Process(int adc_offset,
                 AudioHandle::InputBuffer in,
                 AudioHandle::OutputBuffer out,
                 size_t size,
                 Max7219* LedDriver,
                 int layer_idx,
                 DaisySeed* hw);
};