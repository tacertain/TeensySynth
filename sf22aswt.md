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

### 3. Parameter Conversion Details

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

## Example Integration

For a complete implementation example, see the TeensySynth project's `SoundfontSynthesizer` class, which wraps SF22ASWT for real-time MIDI performance with dynamic instrument switching and polyphonic playback.

This architecture enables sophisticated wavetable synthesis on Teensy hardware while maintaining the flexibility and rich sound palette of professional SoundFont libraries.