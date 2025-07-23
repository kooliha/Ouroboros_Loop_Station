#include "daisy_seed.h"
#include "74hc165.h"

using namespace daisy;
using namespace daisy::seed;

DaisySeed hw;
SR74HC165 ReadButtons;

void SetupHardware()
{
    hw.Configure();
    hw.Init();
    hw.StartLog(true);

    // MISO_74HC165 - D9, CLK_74HC165 - D24, Load_74HC165 - D23
    ReadButtons.Init(D24, D9, D23, &hw);
}

int main(void)
{
    SetupHardware();

    while(1)
    {
        uint8_t buttons = ReadButtons.Read();

        bool layer1_control = buttons & 0x01; // Q0 (A)
        bool layer1_source  = buttons & 0x02; // Q1 (B)

        hw.PrintLine("Layer1_Control: %s, Layer1_Source: %s",
                     layer1_control ? "Pressed" : "Released",
                     layer1_source  ? "Pressed" : "Released");

        hw.DelayMs(200);
    }
}