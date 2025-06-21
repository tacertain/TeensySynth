// Simple test of USB Host
//
// This example is in the public domain

#include "USBHost_t36.h"
#include <synth_karplusstrong.h>
#include <output_i2s.h>
#include "Synthesizer.h"

/**
 * Pin usage:
 *
 * TFT Display:
 * CS: 10
 * DC: 9
 * SDI/MOSI: 11
 * SCK: 13
 * SDO/MISO: 12
 * 
 * 
 * Audio:
 * WSEL/LRCLK: 20
 * BCLK: 21
 * DIN: 7
 */

Synthesizer synth;

USBHost myusb;
KeyboardController keyboard1(myusb);
MIDIDevice midi1(myusb);

// Function prototypes
void OnPress(int key);
void OnRawPress(uint8_t keycode);
void OnRawRelease(uint8_t keycode);
void OnNoteOn(byte channel, byte note, byte velocity);
void OnNoteOff(byte channel, byte note, byte velocity);
void OnControlChange(byte channel, byte control, byte value);
void OnPitchChange(byte channel, int bend); 

void setup()
{
    while (!Serial) ; 
    myusb.begin();
    keyboard1.attachPress(OnPress);
    keyboard1.attachRawPress(OnRawPress);
    keyboard1.attachRawRelease(OnRawRelease);
    midi1.setHandleNoteOff(OnNoteOff);
    midi1.setHandleNoteOn(OnNoteOn);
    midi1.setHandleControlChange(OnControlChange);
    midi1.setHandlePitchChange(OnPitchChange); 
    AudioMemory(15);

}


void loop()
{
	myusb.Task();
	midi1.read();
}


void OnPress(int key)
{
	Serial.print("key '");
	Serial.print((char)key);
	Serial.print("'  ");
	Serial.println(key);
	//Serial.print("key ");
	//Serial.print((char)keyboard1.getKey());
	//Serial.print("  ");
	//Serial.print((char)keyboard2.getKey());
	//Serial.println();
}

void OnRawPress(uint8_t keycode)
{
	Serial.print("raw key press: ");
	Serial.println((int)keycode);
}

void OnRawRelease(uint8_t keycode)
{
	Serial.print("raw key release: ");
	Serial.println((int)keycode);
}

void OnNoteOn(byte channel, byte note, byte velocity)
{
    Serial.print("Note On, ch=");
    Serial.print(channel);
    Serial.print(", note=");
    Serial.print(note);
    Serial.print(", velocity=");
    Serial.print(velocity);
    Serial.println();

    float freq = 440.0f * exp2f((float)(note - 69) * 0.0833333f);
    float vel = (float)velocity/128.0f;
    Serial.print("Synthesizer.noteOn(");
    Serial.print(freq);
    Serial.print(", ");
    Serial.print(vel);
    Serial.print(")");
    Serial.println();
    synth.noteOn(note, freq, vel);
}

void OnNoteOff(byte channel, byte note, byte velocity)
{
	Serial.print("Note Off, ch=");
	Serial.print(channel);
	Serial.print(", note=");
	Serial.print(note);
	Serial.println();
	synth.noteOff(note); 

}

void OnControlChange(byte channel, byte control, byte value)
{
    Serial.print("Control Change, ch=");
    Serial.print(channel);
    Serial.print(", control=");
    Serial.print(control);
    Serial.print(", value=");
    Serial.print(value);
    Serial.println();

    if(channel == 1 && control == 21) {
        synth.setAttenuation(value);
    }
    if(channel == 1 && control == 22) {
        synth.setFilterStrength(value);
    }

    if(channel == 1 && control == 7) {
        synth.setVolume((float)value / 127.0f);
    }

    if(channel == 1 && control == 51 && value == 127) {
        Serial.print("Max CPU Usage = ");
        Serial.print(AudioProcessorUsageMax(), 1);
        Serial.print("% Max memory usage = ");
		Serial.print(AudioMemoryUsageMax(), 1);
		Serial.print("%");
		Serial.println();
    }
}

void OnPitchChange(byte channel, int bend) // <-- Changed function name
{
    Serial.print("Pitch Bend, ch=");
    Serial.print(channel);
    Serial.print(", bend=");
    Serial.println(bend);
    
    // Convert MIDI pitch bend (-8192 to +8191) to -4.0-4.0 range
    float bendAmount = (float)bend / 8192.0f * 4.0f;
    synth.setPitchBend(bendAmount);
}

