# USB Hub Support for Multiple MIDI Keyboards

## Overview

The TeensySynth project currently supports a single MIDI keyboard connected directly to the Teensy 4.1's USB host port. This document outlines the requirements and implementation steps to support multiple MIDI keyboards simultaneously via a USB hub.

## Current Architecture

### Existing USB Host Setup

```cpp
// Current implementation in main.cpp
USBHost myusb;
MIDIDevice midi1(myusb);
MIDIController midiController(synth, myusb, midi1);
```

The current setup:
- Single `USBHost` instance managing the USB host controller
- Single `MIDIDevice` instance for one MIDI keyboard
- Single `MIDIController` routing MIDI events to the synthesizer

## Hardware Requirements

### USB Hub Selection

**Recommended Hub Specifications:**
- **USB 2.0 Full Speed** (12 Mbps) - MIDI devices don't need high speed
- **Powered hub** - Teensy 4.1 can only provide 500mA total on USB host port
- **4-7 ports** - Sufficient for 2-4 MIDI controllers plus future expansion
- **Compatible with USB Host Shield** - Must work with Teensy USB host implementation

**Tested Compatible Hubs:**
- D-Link 4-Port USB 2.0 Hub (DUB-H4)
- Belkin 4-Port Powered Hub
- Generic powered USB 2.0 hubs (most should work)

**Avoid:**
- USB 3.0 hubs (unnecessary complexity, may have compatibility issues)
- Unpowered hubs when using multiple devices
- Hubs with built-in card readers or other complex features

### Power Considerations

MIDI keyboards typically draw:
- **Class-compliant MIDI controllers**: 50-100mA each
- **Keyboards with displays/lights**: 200-500mA each
- **Bus-powered synthesizers**: 500mA or more

**Power Budget:**
- Teensy 4.1 USB host port: **500mA maximum**
- With 2 MIDI keyboards: Likely **100-300mA total**
- **Recommendation**: Use powered USB hub to avoid any power issues

## Software Implementation

### Step 1: Declare Multiple MIDIDevice Instances

The USBHost_t36 library supports up to **4 USB MIDI devices** simultaneously:

```cpp
// In main.cpp
USBHost myusb;
MIDIDevice midi1(myusb);  // First MIDI keyboard
MIDIDevice midi2(myusb);  // Second MIDI keyboard
MIDIDevice midi3(myusb);  // Third MIDI keyboard (optional)
MIDIDevice midi4(myusb);  // Fourth MIDI keyboard (optional)
```

### Step 2: Modify MIDIController Class

The `MIDIController` class needs to support multiple MIDI devices:

#### Option A: Multiple MIDIController Instances (Simple)

```cpp
// Create separate controllers for each MIDI device
MIDIController midiController1(synth, myusb, midi1);
MIDIController midiController2(synth, myusb, midi2);

// In setup()
midiController1.begin();
midiController2.begin();

// In loop()
midiController1.update();
midiController2.update();
```

**Pros:**
- Minimal code changes
- Each keyboard can be independently configured
- Easy to implement different channel mappings

**Cons:**
- Duplicate code if both keyboards need same behavior
- More memory overhead

#### Option B: Multi-Device MIDIController (Preferred)

Refactor `MIDIController` to handle multiple devices:

```cpp
class MIDIController {
public:
    static const int MAX_MIDI_DEVICES = 4;
    
    MIDIController(HybridSynthesizer& synth, USBHost& usbHost, 
                   MIDIDevice* devices[], int deviceCount);
    
    void begin();
    void update();
    
    // Existing handlers...
    void handleNoteOn(byte channel, byte note, byte velocity);
    void handleNoteOff(byte channel, byte note, byte velocity);
    void handleControlChange(byte channel, byte control, byte value);
    void handlePitchChange(byte channel, int bend);

private:
    HybridSynthesizer& synth;
    USBHost& usbHost;
    MIDIDevice** midiDevices;
    int deviceCount;
    
    // Track which device is active
    int activeDevice;
    
    // Static callback wrappers for each device
    static void OnNoteOnDevice1(byte channel, byte note, byte velocity);
    static void OnNoteOnDevice2(byte channel, byte note, byte velocity);
    // ... etc
};
```

**Implementation in main.cpp:**
```cpp
MIDIDevice* midiDevices[] = {&midi1, &midi2};
MIDIController midiController(synth, myusb, midiDevices, 2);
```

### Step 3: MIDI Channel Assignment Strategy

With multiple keyboards, you need a strategy for routing MIDI data:

#### Strategy 1: Channel-Based Routing
Different keyboards play different synth modes based on MIDI channel:

```cpp
void MIDIController::handleNoteOn(byte channel, byte note, byte velocity) {
    switch (channel) {
        case 1:  // Keyboard 1 - SOUNDFONT mode
            synth.setSynthMode(HybridSynthesizer::SOUNDFONT);
            synth.noteOn(channel, note, freq, vel);
            break;
        case 2:  // Keyboard 2 - DRONE mode
            synth.setSynthMode(HybridSynthesizer::DRONE);
            synth.noteOn(channel, note, freq, vel);
            break;
        // ... etc
    }
}
```

#### Strategy 2: Device-Based Routing
Different keyboards automatically map to different synth modes:

```cpp
void MIDIController::handleNoteOn(int deviceIndex, byte channel, byte note, byte velocity) {
    if (deviceIndex == 0) {
        // Keyboard 1 always plays SOUNDFONT
        synth.setSynthMode(HybridSynthesizer::SOUNDFONT);
    } else if (deviceIndex == 1) {
        // Keyboard 2 always plays DRONE
        synth.setSynthMode(HybridSynthesizer::DRONE);
    }
    synth.noteOn(channel, note, freq, vel);
}
```

#### Strategy 3: Layer Mode
Both keyboards play the same notes in different synth modes for layered sound:

```cpp
void MIDIController::handleNoteOn(int deviceIndex, byte channel, byte note, byte velocity) {
    // Keyboard 1: SOUNDFONT layer
    if (deviceIndex == 0) {
        synth.noteOn(1, note, freq, vel);  // SOUNDFONT on channel 1
    }
    // Keyboard 2: DRONE layer
    else if (deviceIndex == 1) {
        synth.noteOn(2, note, freq, vel);  // DRONE on channel 2
    }
}
```

### Step 4: USB Hub Detection and Enumeration

The USBHost_t36 library automatically detects devices connected through a hub:

```cpp
void setup() {
    Serial.begin(115200);
    delay(1000);
    
    myusb.begin();
    
    // The library handles hub enumeration automatically
    // Wait for devices to enumerate
    Serial.println("Waiting for USB MIDI devices...");
    
    delay(2000);  // Give devices time to enumerate
    
    // Check which devices are connected
    if (midi1) {
        Serial.println("MIDI Device 1 connected");
    }
    if (midi2) {
        Serial.println("MIDI Device 2 connected");
    }
    if (midi3) {
        Serial.println("MIDI Device 3 connected");
    }
    if (midi4) {
        Serial.println("MIDI Device 4 connected");
    }
}
```

### Step 5: Dynamic Device Connection Handling

Handle hot-plugging of MIDI devices:

```cpp
void loop() {
    myusb.Task();  // Process USB events
    
    // Check if device connections have changed
    static bool midi1WasConnected = false;
    static bool midi2WasConnected = false;
    
    bool midi1Connected = (bool)midi1;
    bool midi2Connected = (bool)midi2;
    
    if (midi1Connected && !midi1WasConnected) {
        Serial.println("MIDI Device 1 connected");
        midiController.beginDevice(0);
    } else if (!midi1Connected && midi1WasConnected) {
        Serial.println("MIDI Device 1 disconnected");
    }
    
    if (midi2Connected && !midi2WasConnected) {
        Serial.println("MIDI Device 2 connected");
        midiController.beginDevice(1);
    } else if (!midi2Connected && midi2WasConnected) {
        Serial.println("MIDI Device 2 disconnected");
    }
    
    midi1WasConnected = midi1Connected;
    midi2WasConnected = midi2Connected;
    
    // Update MIDI processing
    midi1.read();
    midi2.read();
    
    synth.update();
}
```

## Memory Considerations

### RAM Usage

Each `MIDIDevice` instance requires:
- **Device object**: ~100 bytes
- **USB buffers**: ~512 bytes per device
- **Total per device**: ~600-800 bytes

With 4 MIDI devices:
- **Additional RAM**: ~2.4-3.2 KB
- **Teensy 4.1 total RAM**: 1024 KB (plenty of headroom)

### Audio Memory

Multiple keyboards playing simultaneously may require more audio memory:

```cpp
// Current setup
AudioMemory(20);  // 20 blocks

// With multiple keyboards
AudioMemory(30);  // Increase to 30-40 blocks for safety
```

Each audio memory block is 256 samples (512 bytes). Monitor usage with:
```cpp
Serial.println(AudioMemoryUsage());
Serial.println(AudioMemoryUsageMax());
```

## Testing and Validation

### Test Plan

1. **Single Keyboard via Hub**
   - Connect one keyboard through hub
   - Verify all MIDI functions work
   - Confirm latency is acceptable (<10ms)

2. **Two Keyboards Simultaneously**
   - Connect both keyboards
   - Play notes on both at the same time
   - Verify no note stealing or dropped messages
   - Test all synth modes

3. **Hot-Plug Testing**
   - Connect/disconnect keyboards while running
   - Verify graceful handling of connection changes
   - Ensure no crashes or hangs

4. **Stress Testing**
   - Play rapid note sequences on both keyboards
   - Hold many notes simultaneously (test voice allocation)
   - Send rapid CC messages from both keyboards

### Debugging Tips

```cpp
// Enable detailed USB debug output
#define USBHOST_PRINT_DEBUG

// Monitor MIDI traffic
void MIDIController::handleNoteOn(int device, byte channel, byte note, byte velocity) {
    Serial.print("Device ");
    Serial.print(device);
    Serial.print(": Note On Ch");
    Serial.print(channel);
    Serial.print(" Note ");
    Serial.print(note);
    Serial.print(" Vel ");
    Serial.println(velocity);
    // ... handle note
}
```

## Example Performance Modes

### Mode 1: Split Keyboard Setup
- **Keyboard 1** (lower): Bass/accompaniment (DRONE or SOUNDFONT)
- **Keyboard 2** (upper): Lead/melody (STRING_PADS or SOUNDFONT)

### Mode 2: Layer Mode
- Both keyboards play same notes
- **Keyboard 1**: SOUNDFONT (brass)
- **Keyboard 2**: STRING_PADS (strings)
- Creates rich layered sound

### Mode 3: Multi-Timbral
- **Keyboard 1**: Channels 1-8 for different instruments
- **Keyboard 2**: Channels 9-16 for drum/percussion
- Each keyboard controls different parts of arrangement

## Potential Issues and Solutions

### Issue 1: USB Enumeration Failures

**Symptom**: Keyboards not detected or randomly disconnect

**Solutions:**
- Use powered USB hub
- Add longer delay in setup() for enumeration
- Check USB cable quality
- Try different hub model

### Issue 2: MIDI Latency

**Symptom**: Noticeable delay between key press and sound

**Solutions:**
- Reduce audio memory block size if possible
- Optimize synth update() loop
- Ensure USB Task() is called frequently
- Reduce serial print statements in MIDI handlers

### Issue 3: Voice Stealing

**Symptom**: Notes cut off unexpectedly with both keyboards

**Solutions:**
- Increase polyphony in each synthesizer
- Implement better voice allocation algorithm
- Increase AudioMemory() allocation
- Use separate synthesizer instances per keyboard

### Issue 4: Channel Conflicts

**Symptom**: Both keyboards control same synth

**Solutions:**
- Configure keyboards to transmit on different MIDI channels
- Implement device-based routing instead of channel-based
- Add configuration mode to assign devices to synth modes

## Future Enhancements

### 1. MIDI Learn Mode
- Press button to enter learn mode
- Next MIDI control assigns to parameter
- Store mappings in EEPROM

### 2. Multi-Device Display
- Show which device is connected on TFT
- Display active synth mode per keyboard
- Visual feedback for MIDI activity per device

### 3. Configuration Persistence
- Save device-to-synth mappings in EEPROM
- Remember last used configuration
- Quick preset recall for common setups

### 4. MIDI Merge
- Merge output from both keyboards to single virtual channel
- Useful for layer modes
- Adjustable velocity curves per device

## References

- [USBHost_t36 Library Documentation](https://github.com/PaulStoffregen/USBHost_t36)
- [Teensy 4.1 USB Host Information](https://www.pjrc.com/teensy/td_libs_USBHostShield.html)
- [MIDI Specification](https://www.midi.org/specifications)
- [USB Hub Compatibility List](https://forum.pjrc.com/threads/25050-USB-Host-Hub-Compatibility-List)

## Summary

Supporting multiple MIDI keyboards via USB hub requires:

1. **Hardware**: Powered USB 2.0 hub
2. **Software**: Declare multiple `MIDIDevice` instances
3. **Architecture**: Refactor `MIDIController` for multi-device support
4. **Strategy**: Choose routing approach (channel, device, or layer based)
5. **Testing**: Thorough validation of all modes and edge cases

The USBHost_t36 library provides excellent support for USB hubs and multiple MIDI devices, making this a straightforward enhancement to the TeensySynth project.
