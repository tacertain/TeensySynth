// Third-party libraries
#include <output_i2s.h>
#include <synth_karplusstrong.h>
#include <SD.h>
#include "USBHost_t36.h"

// Project headers
#include "hardware_config.h"
#include "DisplayController.h"
#include "HybridSynthesizer.h"
#include "MIDIController.h"

HybridSynthesizer synth; // Use the new hybrid synthesizer

USBHost myusb;
MIDIDevice midi1(myusb);
MIDIController midiController(synth, myusb, midi1);

#ifdef TFT_DISPLAY
DisplayController displayController;
#endif

uint32_t count = 0;

void setup()
{
    //while (!Serial);
    midiController.begin();
    AudioMemory(20); // Memory for audio processing

    Serial.println("Hello, world!");

#ifdef TFT_DISPLAY
    // Initialize TFT Display
    displayController.begin();
    // Set the first string synthesizer to use the TFT display
    synth.getString(0).setTFTDisplay(displayController.getGraphics());
    Serial.println("TFT display assigned to string synthesizer 0");
#else
    Serial.println("Running without TFT display - audio functionality will work normally");
#endif
    Serial.println("Setup complete - ready for input");
}

void loop()
{
#if 0
    static uint64_t i = 0;
    if (++i % 1000000 == 0) {
        Serial.print("Alive ");
        Serial.print(i / 1000000);
        Serial.print(" ");
        Serial.println(count);
    }
#endif
    midiController.update();
    
    // Update synthesizer envelopes and other processing
    synth.update();
    
#ifdef TFT_DISPLAY
    displayController.update();
#endif
}
