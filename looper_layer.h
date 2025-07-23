#pragma once
#include "daisy_seed.h"
#include "max7219.h"

constexpr uint8_t LED_LAYER1_REC  = 0x40; // A
constexpr uint8_t LED_LAYER1_PLAY = 0x80; // DP
constexpr uint8_t LED_LAYER2_REC  = 0x10; // C
constexpr uint8_t LED_LAYER2_PLAY = 0x20; // B

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

    float speed = 1.0f;
    float volume = 1.0f;
    float pan = 0.5f;

    daisy::Switch* button = nullptr;
    daisy::GPIO* input_select_switch = nullptr;

    uint32_t last_release = 0;
    int click_count = 0;
    static constexpr uint32_t double_click_time = 400;

    void Init(float* buf_l, float* buf_r, size_t buf_size, daisy::Switch* btn, daisy::GPIO* input_sel)
    {
        buffer_l = buf_l;
        buffer_r = buf_r;
        buffer_size = buf_size;
        button = btn;
        input_select_switch = input_sel;
        Reset();
    }

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

    void Process(int adc_offset,
                 daisy::AudioHandle::InputBuffer in,
                 daisy::AudioHandle::OutputBuffer out,
                 size_t size,
                 LooperLayer* other_layer,
                 Max7219* LedDriver,
                 daisy::DaisySeed* hw);
};