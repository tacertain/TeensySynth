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
    pinMode(LED_BUILTIN, OUTPUT);
    midiController.begin();
    AudioMemory(32); // Memory for audio processing

    Serial.println("Hello, world!");
    AudioControlSGTL5000 audioShield;
    audioShield.enable();
    audioShield.volume(0.9);
    // Initialize soundfont synthesizer
    if (synth.initializeSoundfont()) {
        Serial.println("Soundfont synthesizer initialized successfully");
    } else {
        Serial.println("Warning: Soundfont synthesizer initialization failed");
    }
    
    // Set up TUSK_CHORD mode as default
    synth.setSynthMode(HybridSynthesizer::TUSK_CHORD);
    synth.setSplitPoint(60);
    synth.loadTuskInstruments();
    synth.loadTuskTrumpetMap();
    Serial.println("Started in TUSK_CHORD mode");

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
    // 0.5 Hz heartbeat on the onboard LED: toggle once per second so a full
    // on/off cycle is 2 s. If loop() hangs, the LED freezes on whatever it was
    // last set to; if the framework's fault_isr fires, it overrides this with
    // the SOS blink pattern. Either way the LED tells us at a glance whether
    // loop() is alive.
    static uint32_t lastLedToggleMs = 0;
    static bool ledState = false;
    uint32_t nowMs = millis();
    if (nowMs - lastLedToggleMs >= 1000) {
        ledState = !ledState;
        digitalWrite(LED_BUILTIN, ledState);
        lastLedToggleMs = nowMs;
    }

    midiController.update();
    
    // Update synthesizer envelopes and other processing
    synth.update();
    
#ifdef TFT_DISPLAY
    displayController.update();
#endif
}
