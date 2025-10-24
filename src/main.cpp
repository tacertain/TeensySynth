#include "USBHost_t36.h"
#include <synth_karplusstrong.h>
#include <output_i2s.h>
#include <SD.h>
#include "HybridSynthesizer.h"
#include "hardware_config.h"
#include "MIDIController.h"
#include "DisplayController.h"

HybridSynthesizer synth; // Use the new hybrid synthesizer

USBHost myusb;
MIDIDevice midi1(myusb);
MIDIController midiController(synth, myusb, midi1);

DisplayController displayController;

uint32_t count = 0;

void setup()
{
    //while (!Serial);
    midiController.begin();
    AudioMemory(20); // Memory for audio processing

    Serial.println("Hello, world!");
    Serial8.begin(400000, SERIAL_8N1);

    // Initialize TFT Display
    if (displayController.begin()) {
        // Set the first string synthesizer to use the TFT display
        synth.getString(0).setTFTDisplay(displayController.getGraphics());
        Serial.println("TFT display assigned to string synthesizer 0");
    } else {
        Serial.println("Running without TFT display - audio functionality will work normally");
    }

    midiController.queryUSBDeviceInfo();
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
    
    // Update display if available
    displayController.update();
}
