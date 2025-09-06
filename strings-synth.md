# Classic 80s String Synthesizer - Design Document

## Overview
This document outlines the design for adding classic 80s string synthesizer sounds to the TeensySynth project. The goal is to recreate the lush, sweeping string pad sounds that were characteristic of 1980s music, particularly those produced by instruments like the Korg Delta, Roland Jupiter-6, and similar analog string synthesizers used in tracks like "I Ran" by Flock of Seagulls.

While the current project already has Karplus-Strong string synthesis (which produces plucked string sounds) and a drone synthesizer targeting the Korg MS-10, this design focuses on adding sustained, ensemble string pad sounds that complement the existing capabilities.

## Target Sound Characteristics
The classic 80s string synthesizer sound has these key characteristics:

### 1. **Ensemble String Timbre**
- Warm, sustained pad sounds reminiscent of a string section
- Rich harmonic content with filtered sawtooth waves
- Multiple layers/voices creating ensemble effect
- Subtle pitch variations between voices for natural ensemble feel

### 2. **Filter Characteristics**
- Low-pass filtering with gentle roll-off
- Resonance adding character without being too harsh
- Filter envelope creating dynamic brightness changes
- Typically warmer (lower cutoff) than lead synthesizer sounds

### 3. **Chorus/Ensemble Effect**
- Multiple slightly detuned voices
- Subtle modulation creating width and movement
- Stereo spread for spatial presence
- Classic "ensemble" chorus effect similar to Roland Juno chorus

### 4. **Envelope Behavior**
- Slow attack creating smooth fade-in
- Long sustain for pad functionality  
- Medium to long release for smooth transitions
- Optional string "swell" effect with filter envelope

### 5. **Modulation**
- Subtle LFO modulation on pitch and filter
- String vibrato effect
- Possible tremolo for additional movement

## Technical Implementation Strategy

### 1. New String Pad Synthesizer Class
Create a `StringPadSynthesizer` class separate from the existing Karplus-Strong strings:

```cpp
class StringPadSynthesizer {
public:
    static const int MAX_VOICES = 6;     // Polyphonic capability
    static const int ENSEMBLE_LAYERS = 3; // Multiple detuned layers per voice
    
    void noteOn(int midiNote, float velocity);
    void noteOff(int midiNote);
    void setStringType(StringType type);  // Different string presets
    void setChorusDepth(float depth);
    void setFilterCutoff(float cutoff);
    void setAttackTime(float attackMs);
    void setReleaseTime(float releaseMs);
    
    AudioStream* getLeftOutput();
    AudioStream* getRightOutput();
};
```

### 2. Simplified Synthesis Modes
Update the existing `SynthMode` enum to have three clean solo options:

```cpp
enum SynthMode {
    PLUCKED_STRINGS,    // Existing Karplus-Strong plucked strings (renamed)
    DRONE,              // Existing analog drone synthesizer
    STRING_PADS         // New: Classic 80s string pads
};
```

### 3. Audio Architecture

#### Per-Voice Architecture:
Each voice consists of 3 ensemble layers:
```
┌─ Osc1 (Saw, +0 cents) ─┐
├─ Osc2 (Saw, +7 cents) ─┤→ Mixer → Filter → Envelope → Voice Output
└─ Osc3 (Saw, -5 cents) ─┘
```

#### System-Level Architecture:
```
Voice 1 ┐
Voice 2 ├─ Chorus/Ensemble Effect ─ Stereo Spread → Left/Right Outputs
...     │
Voice 6 ┘
```

#### Teensy Audio Library Components:
- **Oscillators**: `AudioSynthWaveform` (sawtooth waves)
- **Filtering**: `AudioFilterStateVariable` (lowpass mode)
- **Envelopes**: `AudioEffectEnvelope` for amplitude and filter
- **Chorus**: `AudioEffectChorus` or custom ensemble implementation
- **Mixing**: `AudioMixer4` for voice combining and stereo placement

### 4. Memory and CPU Considerations

#### Resource Usage:
- 6 voices × 3 layers = 18 oscillators
- 6 filter objects
- 6 envelope generators  
- Multiple mixer objects for routing
- Chorus/ensemble processing

#### Optimization Strategies:
1. **Dynamic Voice Allocation**: Only activate voices when needed
2. **Shared LFO**: Single LFO modulating multiple parameters
3. **Efficient Mixing**: Use hierarchical mixer structure
4. **Filter Optimization**: Share filter parameters where possible

### 5. User Interface Integration

#### Control Mapping:
Integrate with existing USB keyboard controls:
- Pitch bend affecting all active voices
- Modulation wheel controlling filter cutoff or chorus depth
- Velocity sensitivity affecting filter brightness
- Mode switching via dedicated key combinations or MIDI program change

Note: TFT display integration is deferred to focus on core audio functionality first.

## Implementation Phases

### Phase 1: Core String Pad Engine
1. Create `StringPadSynthesizer` class
2. Implement single voice with 3-layer ensemble
3. Add basic filtering and envelope shaping
4. Test with single note playability

### Phase 2: Polyphonic Capabilities  
1. Implement voice allocation system
2. Add polyphonic note on/off handling
3. Test chord playing and voice management
4. Optimize for 6-voice polyphony

### Phase 3: Ensemble Effects
1. Implement chorus/ensemble effect
2. Add stereo spread and spatial positioning  
3. Tune detuning amounts for authentic ensemble sound
4. Add subtle pitch and filter modulation

### Phase 4: Integration and Presets
1. Integrate into `HybridSynthesizer` class with simplified mode switching
2. Add new STRING_PADS synthesis mode
3. Create string pad presets mimicking classic 80s sounds
4. Implement mode switching via USB keyboard or MIDI

### Phase 5: Advanced Features
1. Filter envelope modulation
2. String "swell" effects
3. Mode switching refinements
4. Performance optimizations

## Sound Design Presets

### Preset 1: "Classic Pad" 
- Filter: Cutoff ~800Hz, Low resonance
- Chorus: Medium depth
- Attack: 200ms, Release: 1000ms
- Detuning: ±5-7 cents between layers

### Preset 2: "Warm Ensemble"
- Filter: Cutoff ~600Hz, Higher resonance  
- Chorus: High depth with slower rate
- Attack: 300ms, Release: 1500ms
- Detuning: ±8-10 cents for wider ensemble

### Preset 3: "Bright Strings"
- Filter: Cutoff ~1200Hz, Low resonance
- Chorus: Light depth
- Attack: 100ms, Release: 800ms
- Detuning: ±3-5 cents for tighter ensemble

### Preset 4: "Flock of Seagulls Strings"
- Filter: Cutoff ~700Hz with envelope modulation
- Chorus: Medium depth with stereo spread
- Attack: 250ms, Release: 1200ms
- Subtle LFO on filter cutoff for movement

## Hardware Requirements

### Current Platform Compatibility:
- **Teensy 4.1**: Sufficient CPU and memory for implementation
- **Audio Library**: All required components available
- **I2S Audio Output**: Stereo capability for ensemble effects
- **USB Host**: Existing keyboard input support

### Memory Footprint Estimate:
- Code: ~15-20KB additional
- RAM: ~8-12KB for voice management and audio buffers
- Audio Processing: ~15-20% additional CPU usage

## Testing and Validation

### Technical Testing:
1. **Polyphonic Performance**: Test 6-voice chord stability
2. **Audio Quality**: Measure THD and frequency response  
3. **CPU Usage**: Monitor for real-time performance
4. **Memory Usage**: Ensure no memory leaks in voice allocation

### Musical Testing:
1. **Chord Progressions**: Test classic 80s progressions
2. **Filter Sweeps**: Validate smooth parameter changes
3. **Ensemble Width**: Verify stereo imaging and chorus depth
4. **Integration**: Test layering with existing modes

## Risks and Mitigations

### Risk 1: CPU/Memory Limitations
- **Mitigation**: Implement voice limiting and efficient audio routing
- **Fallback**: Reduce polyphony or ensemble layers if needed

### Risk 2: Audio Quality vs Performance Trade-off
- **Mitigation**: Profile different implementation approaches
- **Fallback**: Offer quality vs performance settings

### Risk 3: Integration Complexity  
- **Mitigation**: Start with clean three-mode architecture
- **Fallback**: Implement as separate mode initially before full integration

## Success Criteria
1. **Authenticity**: Sounds recognizably similar to 80s string synthesizers
2. **Playability**: Responsive polyphonic performance with USB keyboard
3. **Integration**: Clean three-mode system (PLUCKED_STRINGS, DRONE, STRING_PADS)
4. **Performance**: Maintains real-time audio without dropouts
5. **Mode Switching**: Simple and reliable mode changes during performance

## Future Enhancements
1. **String Sections**: Orchestra-style string arrangements
2. **Advanced Modulation**: Complex LFO routing and modulation matrix
3. **Effects Chain**: Reverb, delay, and additional time-based effects
4. **MIDI Implementation**: Full MIDI CC control for all parameters
5. **Preset Management**: Save/load user presets to SD card
