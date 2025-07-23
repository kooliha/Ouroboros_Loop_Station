#include "daisy_seed.h"
#include "cd4051.h"

using namespace daisy;
using namespace daisy::seed;

DaisySeed hw;
CD4051 ReadPots;

void SetupHardware()
{
    hw.Configure();
    hw.Init();
    hw.StartLog(true); // Enable USB serial logging

    // CD4051: A=D17, B=D18, C=D19, Enable=D20, ADC=A0
    ReadPots.Init(D17, D18, D19, D20, A0, &hw);
    ReadPots.Enable(true); // Enable the IC
}

int main(void)
{
    SetupHardware();

    while(1)
    {
        float volume = ReadPots.ReadPot(0); // IO0
        float speed  = ReadPots.ReadPot(1); // IO1
        float pan    = ReadPots.ReadPot(2); // IO2

        hw.PrintLine("Layer1 Volume: %0.3f, Speed: %0.3f, Pan: %0.3f", volume, speed, pan);
    }
}