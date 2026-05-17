# SF22ASWT Library Architecture

## Overview

SF22ASWT (Soundfont2 to AudioSynthWavetable) is a C++ library specifically designed for Teensy microcontrollers that converts SoundFont 2 (SF2) files into a format compatible with the Teensy Audio Library's `AudioSynthWavetable` synthesizer. The library implements a memory-efficient "lazy loading" approach to work within the constraints of embedded systems.

## Core Architecture

### 1. Lazy Loading Strategy

The library uses a **lazy loading** approach where:
- Only file position pointers and metadata are stored in RAM initially
- Actual sample data is loaded on-demand from SD card
- This dramatically reduces RAM usage compared to loading entire soundfonts

### 2. Key Components

#### ReaderLazy (`sf22aswt_reader_lazy.h/cpp`)
The main interface class that handles:
- SF2 file parsing and validation
- Metadata extraction from SF2 chunks
- On-demand sample data loading
- Instrument list management

#### Converter (`sf22aswt_converter.h/cpp`)
Transforms SF2 data structures into AudioSynthWavetable format:
- Maps SF2 sample parameters to Teensy audio parameters
- Converts envelope timings to sample counts
- Handles pitch, modulation, and vibrato calculations
- Manages memory allocation for final instrument data

#### Structures (`sf22aswt_structures.h/cpp`)
Defines data structures for both intermediate and final formats:
- `sample_header_temp`: Intermediate SF2 sample data
- `sample_header`: Final AudioSynthWavetable-compatible format
- `instrument_data_temp`: Temporary instrument container
- Various SF2 format structures (phdr_rec, bag_rec, etc.)

## SF2 to AudioSynthWavetable Conversion Process

### 1. File Parsing Phase

```
SF2 File Structure:
├── RIFF Header
├── INFO Chunk (metadata)
├── SDTA Chunk (sample data positions)
└── PDTA Chunk (preset/instrument definitions)
```

The library parses these chunks and stores file positions rather than loading all data:

```cpp
// Only positions and sizes are stored, not actual data
sfbk.info_position = file.position();
sfbk.sdta.size = chunkSize;
sfbk.pdta.size = chunkSize;
```

### 2. Instrument Loading

When an instrument is requested:

1. **Metadata Extraction**: Reads instrument definitions from PDTA chunk
2. **Sample Mapping**: Identifies which samples belong to the instrument
3. **Parameter Conversion**: Transforms SF2 parameters to AudioSynthWavetable format
4. **Memory Allocation**: Creates final data structures in RAM

### 3. Sample-to-Note Mapping Process

The SF22ASWT library implements a sophisticated sample mapping system that determines which audio samples are used for each MIDI note. This process involves parsing SoundFont 2 instrument zones and generator parameters.

#### SF2 Instrument Zone Structure

Each SF2 instrument consists of multiple **zones**, where each zone contains:
- A **sample reference** (sampleID generator)
- **Key range** (keyRange generator) - defines which MIDI notes trigger this sample
- **Velocity range** (velRange generator) - defines which velocities use this sample
- **Generator parameters** - pitch, envelope, loop settings, etc.

```cpp
// Example zone structure in SF2:
Zone 1: Sample "Piano_C3.wav", Key Range: 48-59 (C3 to B3), Velocity: 1-127
Zone 2: Sample "Piano_C4.wav", Key Range: 60-71 (C4 to B4), Velocity: 1-127  
Zone 3: Sample "Piano_C5.wav", Key Range: 72-83 (C5 to B5), Velocity: 1-127
```

#### Parsing Process in SF22ASWT

When loading an instrument, the library processes zones sequentially:

```cpp
// In Load_instrument_data():
for (int si = 0; si < inst.sample_count; si++) {
    // Extract key range for this sample zone
    inst.sample_note_ranges[si] = get_key_range_end(bags, si);
    
    // Get the actual sample reference
    shdr_rec shdr;
    get_sample_header(file, sfbk, bags, si, &shdr);
    
    // Store sample parameters
    inst.samples[si].SAMPLE_NOTE = get_sample_note(bags, si, shdr);
    inst.samples[si].sample_start = shdr.dwStart * 2 + sfbk.sdta.smpl.position;
}
```

#### Key Range Extraction

The `get_key_range_end()` function extracts the upper bound of the MIDI note range:

```cpp
int ReaderBase::get_key_range_end(bag_of_gens* bags, int sampleIndex) {
    SF2GeneratorAmount genval;
    // Look for keyRange generator in this sample's zone
    return get_parameter_value(bags, sampleIndex, SFGenerator::keyRange, &genval) 
           ? genval.rangeHigh()  // Use specified range
           : 127;               // Default to full range if not specified
}
```

The `SF2GeneratorAmount` structure handles range encoding:

```cpp
class SF2GeneratorAmount {
    union {
        uint16_t UAmount;
        struct {
            uint8_t LowByte;   // Lower bound of range
            uint8_t HighByte;  // Upper bound of range  
        };
    };
    
    uint8_t rangeLow() { return (LowByte < HighByte) ? LowByte : HighByte; }
    uint8_t rangeHigh() { return (LowByte < HighByte) ? HighByte : LowByte; }
};
```

#### Conversion to AudioSynthWavetable Format

The SF22ASWT converter transforms the SF2 zone-based mapping into AudioSynthWavetable's array-based system:

```cpp
AudioSynthWavetable::instrument_data to_AudioSynthWavetable_instrument_data(
    SF22ASWT::instrument_data_temp &data) 
{
    // Create arrays for samples and their note ranges
    SF22ASWT::sample_header *samples = new SF22ASWT::sample_header[data.sample_count+1];
    uint8_t *note_ranges = new uint8_t[data.sample_count+1];
    
    for (int i = 0; i < data.sample_count; i++) {
        samples[i] = toFinal(data.samples[i]);
        note_ranges[i] = data.sample_note_ranges[i];  // Upper bound of range
    }
    
    // Add dummy sample for notes outside all ranges
    samples[data.sample_count] = {};
    note_ranges[data.sample_count] = 127;
    
    return {
        data.sample_count + 1,
        note_ranges,
        reinterpret_cast<const AudioSynthWavetable::sample_data*>(samples)
    };
}
```

#### AudioSynthWavetable Note Selection Algorithm

When a MIDI note is played, AudioSynthWavetable uses the `note_ranges` array to select the appropriate sample:

```cpp
// Conceptual AudioSynthWavetable note selection:
void AudioSynthWavetable::playNote(int midi_note, int velocity) {
    // Find first sample whose range includes this note
    for (int i = 0; i < instrument.sample_count; i++) {
        if (midi_note <= instrument.note_ranges[i]) {
            // Use samples[i] for this note
            selected_sample = &instrument.samples[i];
            break;
        }
    }
}
```

#### Example Mapping Scenario

Consider a piano instrument with these SF2 zones:

```
SF2 Zones:
Zone 0: Sample "Low_C.wav"    Range: 0-35   (C-1 to B1)   → note_ranges[0] = 35
Zone 1: Sample "Mid_C.wav"    Range: 36-71  (C2 to B4)    → note_ranges[1] = 71  
Zone 2: Sample "High_C.wav"   Range: 72-127 (C5 to G9)    → note_ranges[2] = 127
```

When converted to AudioSynthWavetable format:
- `note_ranges = [35, 71, 127, 127]` (with dummy sample)
- Playing MIDI note 60 (C4): Searches array, finds 60 ≤ 71, uses samples[1] ("Mid_C.wav")
- Playing MIDI note 80 (Ab5): Searches array, finds 80 > 71 but 80 ≤ 127, uses samples[2] ("High_C.wav")

#### Global vs Local Zones

SF2 supports both global and local zones:
- **Global zones** apply parameters to the entire instrument
- **Local zones** contain actual sample references

```cpp
// Detection of global zone in Load_instrument_data():
bool globalExists = (bags[0].count != 0) 
                   ? (bags[0].lastItem().sfGenOper != SFGenerator::sampleID) 
                   : true;

// Adjust sample count based on global zone presence
inst.sample_count = globalExists ? (ibag_count - 1) : ibag_count;
```

#### Velocity Layer Support

While the current implementation focuses on key ranges, SF2 also supports velocity layers:

```cpp
// Velocity range extraction (similar to key range):
SF2GeneratorAmount velRange_gen;
if (get_parameter_value(bags, sampleIndex, SFGenerator::velRange, &velRange_gen)) {
    uint8_t vel_low = velRange_gen.rangeLow();
    uint8_t vel_high = velRange_gen.rangeHigh();
}
```

This mapping system ensures that each MIDI note triggers the most appropriate sample based on the original SoundFont designer's intentions, while adapting to AudioSynthWavetable's streamlined array-based lookup mechanism.

### 4. Parameter Conversion Details

The converter transforms key parameters:

#### Sample Rate and Pitch
```cpp
// Convert SF2 sample rate to Teensy phase increment
float PER_HERTZ_PHASE_INCREMENT = 
    (1 << (32 - LENGTH_BITS)) * 
    WAVETABLE_CENTS_SHIFT(CENTS_OFFSET) * 
    SAMPLE_RATE / 
    WAVETABLE_NOTE_TO_FREQUENCY(SAMPLE_NOTE) / 
    AUDIO_SAMPLE_RATE_EXACT;
```

#### Envelope Parameters
```cpp
// Convert millisecond timings to sample counts
uint32_t DELAY_COUNT = DELAY_ENV * 
    AudioSynthWavetable::SAMPLES_PER_MSEC / 
    AudioSynthWavetable::ENVELOPE_PERIOD;
```

#### Loop Points
```cpp
// Convert sample positions to phase values
uint32_t MAX_PHASE = ((uint32_t)LENGTH - 1) << (32 - LENGTH_BITS);
uint32_t LOOP_PHASE_END = ((uint32_t)LOOP_END - 1) << (32 - LENGTH_BITS);
```

## Memory Management

### RAM Usage Optimization

1. **Sample Data Placement**:
   - Automatically detects if EXTMEM is available
   - Falls back to internal RAM if external memory unavailable
   - Tracks total RAM usage across all loaded instruments

2. **Lazy Sample Loading**:
   ```cpp
   // Samples loaded only when needed
   bool ReadSampleDataFromFile(instrument_data_temp &inst, 
                               bool forceUseInternalRam = false);
   ```

3. **Memory Pool Management**:
   ```cpp
   extern int Samples_Max_Internal_RAM_Cap;  // Configurable limit
   extern int samples_usedRam;               // Current usage tracker
   ```

## Usage Patterns

### Basic Usage
```cpp
SF22ASWTreader sf22aswt;
AudioSynthWavetable::instrument_data *wt_inst;

// Load instrument from SF2 file
sf22aswt.Load_instrument_from_file("gm.sf2", instrumentIndex, &wt_inst);

// Apply to wavetable synthesizer
wavetable.setInstrument(*wt_inst);
```

### Advanced Usage
```cpp
// Parse file first
sf22aswt.ReadFile("soundfont.sf2");

// Load specific instrument
SF22ASWT::instrument_data_temp inst_temp;
sf22aswt.Load_instrument_data(index, inst_temp);

// Convert to final format
AudioSynthWavetable::instrument_data final_inst = 
    SF22ASWT::converter::to_AudioSynthWavetable_instrument_data(inst_temp);
```

## Error Handling

The library provides comprehensive error tracking:
- File access errors
- Format validation errors
- Memory allocation failures
- Invalid parameter ranges

```cpp
if (sf22aswt.getLastError() != SF22ASWT::Errors::NONE) {
    sf22aswt.printSF2ErrorInfo(Serial);
}
```

## Performance Characteristics

### Advantages
- **Low RAM usage**: Only metadata stored initially
- **Fast startup**: No large file loading at boot
- **Scalable**: Can handle large soundfonts on limited hardware
- **Flexible**: Instruments loaded/unloaded as needed

### Trade-offs
- **SD card dependency**: Requires reliable SD card access
- **Loading latency**: Initial instrument loading takes time
- **File system overhead**: Multiple file operations per instrument

## Integration with Teensy Audio Library

The library seamlessly integrates with the Teensy Audio Library by:

1. **Data Structure Compatibility**: Output matches AudioSynthWavetable expectations exactly
2. **Sample Format Alignment**: Handles 16-bit signed integer samples
3. **Timing Synchronization**: Converts all timings to audio sample periods
4. **Memory Layout**: Ensures proper alignment and const-correctness

## Supported SF2 Features

### Fully Supported
- Multiple instruments per file
- Sample loops
- ADSR envelopes
- Pitch modulation
- Vibrato and tremolo effects
- Key velocity mapping
- Note range mapping

### Limitations
- Real-time modulators (simplified)
- Complex filter parameters (basic support)
- Chorus/reverb sends (ignored)
- Multi-sample instruments (supported but may use significant RAM)

## Configuration Options

### Compile-time Options
```cpp
#define SF22ASWT_DEBUG          // Enable debug output
#define USE_LAZY_READER         // Use lazy loading (recommended)
```

### Runtime Configuration
```cpp
SF22ASWT::Samples_Max_Internal_RAM_Cap = 1048576;  // 1MB limit
```

## Pitch Modification and Tuning

The SF22ASWT library provides several mechanisms for modifying the pitch behavior of loaded instruments, both at the conversion stage and during playback.

### 1. SoundFont-Level Pitch Parameters

During the SF2 to AudioSynthWavetable conversion, several pitch-related parameters from the SoundFont are processed:

#### Root Key and Fine Tuning
```cpp
// These SF2 parameters affect the base pitch calculation
sample_header_temp.SAMPLE_NOTE     // Root key (MIDI note number)
sample_header_temp.CENTS_OFFSET    // Fine tuning in cents (+/- 100 cents)
sample_header_temp.SAMPLE_RATE     // Original sample rate
```

#### Coarse and Fine Tune Generators
The library processes SF2 generator values that modify pitch:
- `SFGenerator::coarseTune` - Semitone adjustments
- `SFGenerator::fineTune` - Cent-level adjustments  
- `SFGenerator::overridingRootKey` - Override the sample's root key

### 2. Phase Increment Calculation

The core pitch calculation converts SF2 parameters to a phase increment value used by AudioSynthWavetable:

```cpp
float PER_HERTZ_PHASE_INCREMENT = 
    (1 << (32 - LENGTH_BITS)) * 
    WAVETABLE_CENTS_SHIFT(CENTS_OFFSET) * 
    SAMPLE_RATE / 
    WAVETABLE_NOTE_TO_FREQUENCY(SAMPLE_NOTE) / 
    AUDIO_SAMPLE_RATE_EXACT;
```

### 3. Modifying Pitch During Conversion

To systematically alter the pitch of all samples in an instrument, you can modify the `sample_header_temp` data before conversion:

```cpp
// Example: Transpose entire instrument up by 2 semitones
SF22ASWT::instrument_data_temp inst_temp;
sf22aswt.Load_instrument_data(index, inst_temp);

// Modify pitch for all samples
for (int i = 0; i < inst_temp.sample_count; i++) {
    inst_temp.samples[i].CENTS_OFFSET += 200;  // +2 semitones
    // Or modify the root key:
    // inst_temp.samples[i].SAMPLE_NOTE += 2;
}

// Convert to final format with modified pitch
AudioSynthWavetable::instrument_data final_inst = 
    SF22ASWT::converter::to_AudioSynthWavetable_instrument_data(inst_temp);
```

### 4. Runtime Pitch Control via AudioSynthWavetable

Once the instrument is loaded into AudioSynthWavetable, you can control pitch during playback:

```cpp
// Play notes at different pitches
wavetable.playNote(60);        // Middle C
wavetable.playNote(67);        // G above middle C
wavetable.playFrequency(440);  // Play specific frequency (A4)

// Pitch bend (if supported by your AudioSynthWavetable version)
wavetable.pitchBend(8192);     // Center position
wavetable.pitchBend(10240);    // Bend up
wavetable.pitchBend(6144);     // Bend down
```

### 5. Global Tuning Modifications

For global tuning changes (like A=432Hz instead of A=440Hz), modify the conversion process:

```cpp
// Custom converter function with altered tuning
AudioSynthWavetable::instrument_data custom_converter(
    SF22ASWT::instrument_data_temp &data, 
    float tuning_ratio = 432.0f/440.0f) 
{
    // Process each sample with modified tuning
    for (int i = 0; i < data.sample_count; i++) {
        // Adjust the phase increment for alternate tuning
        float original_phase_inc = /* calculated value */;
        data.samples[i]./* modify phase increment */ *= tuning_ratio;
    }
    return SF22ASWT::converter::to_AudioSynthWavetable_instrument_data(data);
}
```

### 6. Key Mapping and Velocity Curves

The library also supports modifying how MIDI keys map to samples:

```cpp
// Access and modify note ranges after loading
for (int i = 0; i < inst_temp.sample_count; i++) {
    // Modify which MIDI notes trigger this sample
    inst_temp.sample_note_ranges[i] = new_note_range;
}
```

### 7. Microtonal and Alternate Tuning Systems

For microtonal music or alternate tuning systems:

```cpp
// Example: 31-tone equal temperament
float cents_per_step = 1200.0f / 31.0f;  // ~38.7 cents per step

// Modify each sample's tuning for 31-TET
for (int i = 0; i < inst_temp.sample_count; i++) {
    int tet31_note = /* convert MIDI note to 31-TET */;
    float cent_adjustment = (tet31_note * cents_per_step) - 
                           (inst_temp.samples[i].SAMPLE_NOTE * 100.0f);
    inst_temp.samples[i].CENTS_OFFSET += cent_adjustment;
}
```

### Important Notes

- **Performance Impact**: Modifying pitch parameters during conversion has no runtime performance cost
- **Memory Usage**: Creating multiple tuning variants requires additional RAM for each variant
- **Precision**: Cent-level adjustments provide very fine pitch control (1200 cents = 1 octave)
- **Compatibility**: Always ensure modified parameters stay within valid ranges for AudioSynthWavetable

This flexibility allows the SF22ASWT library to support everything from standard equal temperament to complex microtonal and just intonation systems.

## Example Integration

For a complete implementation example, see the TeensySynth project's `SoundfontSynthesizer` class, which wraps SF22ASWT for real-time MIDI performance with dynamic instrument switching and polyphonic playback.

This architecture enables sophisticated wavetable synthesis on Teensy hardware while maintaining the flexibility and rich sound palette of professional SoundFont libraries.