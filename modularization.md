# TeensySynth Compile-Time Modularization Plan

This document outlines a plan to enable compile-time configuration of features, allowing users to include only the functionality they need, reducing memory footprint and binary size.

---

## Goals

1. **Conditional Display Support**: Enable/disable TFT display at compile time
2. **Selective Synthesizer Modes**: Include only desired synthesizer types (Karplus-Strong, Drone, String Pads)
3. **Memory Optimization**: Reduce RAM and Flash usage by excluding unused features
4. **Maintainability**: Keep configuration simple and centralized
5. **Backward Compatibility**: Default configuration should enable all features

---

## Current State Analysis

### Existing Issues
- `TFT_DISPLAY` flag exists but is inconsistently used
- No mechanism to disable individual synthesizer types
- `HybridSynthesizer` always instantiates all three synthesizer types
- Audio connections are always created even if unused
- No compile-time checks for configuration conflicts

### Memory Footprint (Approximate)
- **Karplus-Strong Strings**: ~20KB RAM (8 voices × ~2.5KB each)
- **Drone Synthesizer**: ~2KB RAM (single voice with 3 oscillators)
- **String Pad Synthesizer**: ~8KB RAM (6 voices)
- **TFT Display**: ~300KB RAM (framebuffers in DMAMEM)
- **Total Audio Objects**: Variable based on connections

---

## Proposed Architecture

### 1. Centralized Feature Configuration

**Create `include/feature_config.h`**

```cpp
#pragma once

// ============================================================================
// DISPLAY CONFIGURATION
// ============================================================================

// Uncomment to enable TFT display support
// Requires: ILI9341_t4, Adafruit_GFX libraries
// Memory cost: ~300KB DMAMEM for framebuffers
#define FEATURE_TFT_DISPLAY

// ============================================================================
// SYNTHESIZER MODE CONFIGURATION
// ============================================================================

// Karplus-Strong Plucked String Synthesis
// Memory cost: ~20KB RAM (8 voices)
#define FEATURE_SYNTH_KARPLUS_STRONG

// Analog-Style Drone Synthesizer (monophonic)
// Memory cost: ~2KB RAM (1 voice, 3 oscillators)
#define FEATURE_SYNTH_DRONE

// 80s String Pad Synthesizer (polyphonic)
// Memory cost: ~8KB RAM (6 voices, 3 oscillators each)
#define FEATURE_SYNTH_STRING_PAD

// ============================================================================
// HYBRID SYNTHESIZER MODE CONFIGURATION
// ============================================================================

// Enable split keyboard mode (requires DRONE + STRING_PAD)
// Automatically disabled if required synths not available
#if defined(FEATURE_SYNTH_DRONE) && defined(FEATURE_SYNTH_STRING_PAD)
    #define FEATURE_SYNTH_SPLIT_MODE
#endif

// Enable I Ran mode (requires all three synths)
// Automatically disabled if required synths not available
#if defined(FEATURE_SYNTH_KARPLUS_STRONG) && \
    defined(FEATURE_SYNTH_DRONE) && \
    defined(FEATURE_SYNTH_STRING_PAD)
    #define FEATURE_SYNTH_IRAN_MODE
#endif

// ============================================================================
// AUDIO CONFIGURATION
// ============================================================================

// Audio memory blocks (adjust based on enabled features)
#if defined(FEATURE_SYNTH_KARPLUS_STRONG) && \
    defined(FEATURE_SYNTH_DRONE) && \
    defined(FEATURE_SYNTH_STRING_PAD)
    #define AUDIO_MEMORY_BLOCKS 20  // All features enabled
#elif defined(FEATURE_SYNTH_KARPLUS_STRONG) || \
      (defined(FEATURE_SYNTH_DRONE) && defined(FEATURE_SYNTH_STRING_PAD))
    #define AUDIO_MEMORY_BLOCKS 15  // Two synth types
#else
    #define AUDIO_MEMORY_BLOCKS 10  // Single synth type
#endif

// ============================================================================
// VALIDATION
// ============================================================================

// Ensure at least one synthesizer is enabled
#if !defined(FEATURE_SYNTH_KARPLUS_STRONG) && \
    !defined(FEATURE_SYNTH_DRONE) && \
    !defined(FEATURE_SYNTH_STRING_PAD)
    #error "At least one synthesizer type must be enabled"
#endif

// ============================================================================
// DEBUG OPTIONS
// ============================================================================

// Uncomment to enable verbose logging during startup
// #define FEATURE_DEBUG_LOGGING

// Uncomment to print memory usage statistics
// #define FEATURE_MEMORY_STATS
```

---

## Implementation Plan

### Phase 1: Configuration Infrastructure

#### 1.1 Create Feature Configuration File
- **File**: `include/feature_config.h`
- **Content**: As shown above
- **Dependencies**: None
- **Effort**: 1 hour

#### 1.2 Update Main Include
- **File**: `src/main.cpp`
- **Action**: Include `feature_config.h` before other project headers
- **Validation**: Ensure it compiles with all features enabled
- **Effort**: 15 minutes

### Phase 2: Display Modularization

#### 2.1 Conditional Display Compilation
- **Files**: `src/main.cpp`, `src/DisplayController.h`, `src/DisplayController.cpp`
- **Changes**:
  - Wrap DisplayController class with `#ifdef FEATURE_TFT_DISPLAY`
  - Provide no-op stub when disabled
  - Update main.cpp to conditionally instantiate display
- **Effort**: 1 hour

#### 2.2 Display-Related Headers
- **Files**: `src/karplus_strong_string_synth.h`, `src/karplus_strong_string_synth.cpp`
- **Changes**:
  - Make display integration optional
  - Forward declare display types when feature is enabled
  - Stub out display methods when disabled
- **Effort**: 1 hour

### Phase 3: HybridSynthesizer Refactoring

#### 3.1 Conditional Synthesizer Instantiation
- **File**: `src/HybridSynthesizer.h`
- **Changes**:
  ```cpp
  class HybridSynthesizer {
  private:
      #ifdef FEATURE_SYNTH_KARPLUS_STRONG
      KarplusStrongStringSynth strings[8];
      AudioMixer4 stringMixerL1, stringMixerL2;
      AudioMixer4 stringMixerR1, stringMixerR2;
      AudioConnection* stringPatchCords[16];
      #endif
      
      #ifdef FEATURE_SYNTH_DRONE
      DroneSynthesizer drone;
      AudioMixer4 droneMixerL, droneMixerR;
      AudioConnection* dronePatchCordL;
      AudioConnection* dronePatchCordR;
      #endif
      
      #ifdef FEATURE_SYNTH_STRING_PAD
      StringPadSynthesizer stringPad;
      AudioMixer4 stringPadMixerL, stringPadMixerR;
      AudioConnection* stringPadPatchCordL;
      AudioConnection* stringPadPatchCordR;
      #endif
      
      // Always present
      AudioMixer4 sumL, sumR;
      AudioOutputI2S i2s1;
  };
  ```
- **Effort**: 2 hours

#### 3.2 Conditional Method Implementation
- **File**: `src/HybridSynthesizer.h` (inline methods)
- **Changes**:
  - Wrap mode-specific methods with `#ifdef` guards
  - Provide compile-time errors for disabled features
  - Example:
  ```cpp
  #ifdef FEATURE_SYNTH_DRONE
  DroneSynthesizer& getDrone() { return drone; }
  void setDroneVolume(float volume);
  #else
  DroneSynthesizer& getDrone() {
      static_assert(false, "Drone synthesizer not enabled");
  }
  #endif
  ```
- **Effort**: 2 hours

#### 3.3 SynthMode Enum Refactoring
- **File**: `src/HybridSynthesizer.h`
- **Changes**:
  ```cpp
  enum SynthMode {
      #ifdef FEATURE_SYNTH_KARPLUS_STRONG
      PLUCKED_STRINGS,
      #endif
      #ifdef FEATURE_SYNTH_DRONE
      DRONE,
      #endif
      #ifdef FEATURE_SYNTH_STRING_PAD
      STRING_PADS,
      #endif
      #ifdef FEATURE_SYNTH_SPLIT_MODE
      SPLIT,
      #endif
      #ifdef FEATURE_SYNTH_IRAN_MODE
      IRAN,
      #endif
      MODE_COUNT  // Always last
  };
  ```
- **Effort**: 1 hour

### Phase 4: MIDI Controller Updates

#### 4.1 Conditional CC Handler Routes
- **File**: `src/MIDIController.cpp`
- **Changes**:
  - Wrap synthesizer-specific CC handlers with `#ifdef`
  - Example:
  ```cpp
  #ifdef FEATURE_SYNTH_DRONE
  if (channel == 1 && control == 23) {
      synth.setDroneVolume((float)value / 127.0f);
  }
  #endif
  ```
- **Effort**: 1 hour

#### 4.2 Mode Switching Logic
- **File**: `src/MIDIController.cpp`
- **Changes**:
  - Only allow switching to compiled-in modes
  - Provide feedback when attempting to use disabled mode
- **Effort**: 30 minutes

### Phase 5: Constructor/Destructor Updates

#### 5.1 Conditional Audio Routing
- **File**: `src/HybridSynthesizer.h` constructor
- **Changes**:
  - Only create audio connections for enabled synthesizers
  - Adjust mixer input assignments dynamically
  - Example:
  ```cpp
  HybridSynthesizer() : /* initializers */ {
      int sumInputIndex = 0;
      
      #ifdef FEATURE_SYNTH_KARPLUS_STRONG
      // Connect string synths
      for (int i = 0; i < 8; ++i) { /* ... */ }
      patchCordSumL[sumInputIndex] = new AudioConnection(stringMixerL1, 0, sumL, sumInputIndex);
      sumInputIndex++;
      #endif
      
      #ifdef FEATURE_SYNTH_DRONE
      // Connect drone
      dronePatchCordL = new AudioConnection(drone.getLeftOutput(), 0, sumL, sumInputIndex);
      sumInputIndex++;
      #endif
      
      // And so on...
  }
  ```
- **Effort**: 2 hours

#### 5.2 Destructor Cleanup
- **File**: `src/HybridSynthesizer.h` destructor
- **Changes**: Conditionally delete connections
- **Effort**: 30 minutes

### Phase 6: Documentation and Examples

#### 6.1 Configuration Examples
- **File**: `docs/configuration-examples.md`
- **Content**:
  - Minimal configuration (single synth, no display)
  - Performance configuration (specific synths for live use)
  - Full-featured configuration (all options enabled)
  - Memory comparison table
- **Effort**: 1 hour

#### 6.2 Update Build Instructions
- **File**: `README.md` or `docs/building.md`
- **Content**: How to customize feature_config.h
- **Effort**: 30 minutes

#### 6.3 Migration Guide
- **File**: `docs/migration-guide.md`
- **Content**: How existing code needs to be updated
- **Effort**: 30 minutes

---

## Configuration Presets

### Preset 1: Full Featured (Default)
```cpp
#define FEATURE_TFT_DISPLAY
#define FEATURE_SYNTH_KARPLUS_STRONG
#define FEATURE_SYNTH_DRONE
#define FEATURE_SYNTH_STRING_PAD
// Auto-enabled: SPLIT_MODE, IRAN_MODE
```
**Memory**: ~330KB RAM, ~140KB Flash

### Preset 2: Live Performance (No Display)
```cpp
// #define FEATURE_TFT_DISPLAY  // Disabled
#define FEATURE_SYNTH_KARPLUS_STRONG
#define FEATURE_SYNTH_DRONE
#define FEATURE_SYNTH_STRING_PAD
```
**Memory**: ~30KB RAM, ~130KB Flash

### Preset 3: Drone Only (Minimal)
```cpp
// #define FEATURE_TFT_DISPLAY  // Disabled
// #define FEATURE_SYNTH_KARPLUS_STRONG  // Disabled
#define FEATURE_SYNTH_DRONE
// #define FEATURE_SYNTH_STRING_PAD  // Disabled
```
**Memory**: ~2KB RAM, ~60KB Flash

### Preset 4: String Machine
```cpp
// #define FEATURE_TFT_DISPLAY  // Disabled
// #define FEATURE_SYNTH_KARPLUS_STRONG  // Disabled
// #define FEATURE_SYNTH_DRONE  // Disabled
#define FEATURE_SYNTH_STRING_PAD
```
**Memory**: ~8KB RAM, ~70KB Flash

---

## Testing Strategy

### 1. Compile-Time Testing
- Test each preset configuration compiles successfully
- Verify appropriate compile errors for invalid configurations
- Check binary size reduction for each configuration

### 2. Runtime Testing
- Test each configuration on hardware
- Verify MIDI routing works with enabled features only
- Ensure disabled features produce appropriate errors/warnings

### 3. Memory Testing
- Measure actual RAM usage for each configuration
- Compare against estimated values
- Document results in configuration examples

---

## Potential Issues and Solutions

### Issue 1: Audio Connection Complexity
**Problem**: Dynamic audio routing with conditional compilation is complex
**Solution**: Use preprocessor to calculate connection indices at compile time

### Issue 2: Interface Compatibility
**Problem**: Code calling disabled features will break
**Solution**: Use `static_assert` to provide clear compile-time errors

### Issue 3: Mode Enum Values Change
**Problem**: Enum values differ between configurations
**Solution**: Use named enums consistently, never rely on integer values

### Issue 4: MIDI CC Mapping Conflicts
**Problem**: Different builds respond to different CC values
**Solution**: Document CC mapping clearly, consider config report at startup

### Issue 5: Testing Overhead
**Problem**: Many configurations to test
**Solution**: Focus on key presets, use automated build testing

---

## Benefits

1. **Memory Efficiency**: Save up to 300KB+ RAM by disabling unused features
2. **Code Size**: Reduce Flash usage by 50% or more in minimal configs
3. **Performance**: Fewer active audio objects = better CPU efficiency
4. **Clarity**: Clear understanding of what's included in each build
5. **Flexibility**: Easy to create specialized builds for specific uses

---

## Timeline Estimate

| Phase | Tasks | Effort | Dependencies |
|-------|-------|--------|--------------|
| 1 | Configuration Infrastructure | 1.25h | None |
| 2 | Display Modularization | 2h | Phase 1 |
| 3 | HybridSynthesizer Refactoring | 5h | Phase 1 |
| 4 | MIDI Controller Updates | 1.5h | Phase 3 |
| 5 | Constructor/Destructor Updates | 2.5h | Phase 3 |
| 6 | Documentation | 2h | Phase 5 |
| **Total** | **14.25 hours** | | |

---

## Future Enhancements

1. **PlatformIO Build Environments**: Create pre-configured build environments in `platformio.ini`
2. **Runtime Feature Detection**: Add API to query enabled features at runtime
3. **Web Configurator**: Build tool to generate `feature_config.h` via web interface
4. **Modular MIDI Mapping**: Allow different CC mappings per configuration
5. **Plugin Architecture**: Support dynamically loadable synthesizer modules

---

## Conclusion

This modularization plan provides a clear path to making TeensySynth more flexible and memory-efficient while maintaining code quality and ease of use. The centralized configuration approach keeps complexity manageable and makes it easy to create optimized builds for specific use cases.

**Recommended Next Steps:**
1. Review and approve this plan
2. Implement Phase 1 (Configuration Infrastructure)
3. Test with all features enabled to ensure backward compatibility
4. Proceed with phases 2-5 incrementally
5. Update documentation throughout
