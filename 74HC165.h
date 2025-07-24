#pragma once
#include "daisy_seed.h"

class SR74HC165
{
  public:
    void Init(daisy::Pin clk, daisy::Pin miso, daisy::Pin load, daisy::DaisySeed* hw)
    {
        clk_.Init(clk, daisy::GPIO::Mode::OUTPUT);
        load_.Init(load, daisy::GPIO::Mode::OUTPUT);
        miso_.Init(miso, daisy::GPIO::Mode::INPUT);
        hw_ = hw;

        clk_.Write(false);
        load_.Write(true);
    }

    // Read 8 bits from the shift register
    uint8_t Read()
    {
        uint8_t value = 0;
        load_.Write(false); // Latch parallel inputs
        hw_->DelayMs(1);
        load_.Write(true);

        for(int i = 0; i < 8; i++)
        {
            clk_.Write(false);
            hw_->DelayMs(1);
            value <<= 1;
            if(miso_.Read())
                value |= 0x01;
            clk_.Write(true);
            hw_->DelayMs(1);
        }
        clk_.Write(false);
        return value;
    }

  private:
    daisy::GPIO clk_, load_, miso_;
    daisy::DaisySeed* hw_;
};