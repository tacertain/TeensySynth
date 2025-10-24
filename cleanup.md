# TeensySynth Code Cleanup and Refactoring Recommendations

This document outlines recommended cleanup and refactoring tasks to improve code quality, maintainability, and organization in the TeensySynth project.

---

## 1. File Organization and Naming ✅ COMPLETED

### 1.1 Fix Filename Typo ✅
- **Issue**: `karplus_stong_string_synth.cpp` has a typo ("stong" instead of "strong")
- **Files affected**: `src/karplus_stong_string_synth.cpp`
- **Action**: Rename to `karplus_strong_string_synth.cpp` to match the header file
- **Impact**: Low complexity, prevents confusion
- **Status**: ✅ **COMPLETED** - File renamed successfully

### 1.2 Remove or Archive Ignored Files ✅
- **Issue**: Multiple `.ignore` files in src directory
- **Files affected**: 
  - `src/blink.cpp.ignore`
  - `src/DisplayManager.cpp.ignore`
  - `src/DisplayManager.h.ignore`
  - `src/Guitar.cpp.ignore`
- **Action**: Either delete these files or move to an `archive/` or `deprecated/` directory
- **Rationale**: Clutters workspace, unclear why they're being kept
- **Status**: ✅ **COMPLETED** - All .ignore files moved to `archive/` directory

### 1.3 Empty File Cleanup ✅
- **Issue**: `DisplayManager.h` exists but is completely empty
- **Files affected**: `src/DisplayManager.h`, `src/DisplayManager.cpp`
- **Action**: Either implement or remove entirely (appears unused)
- **Impact**: Reduces confusion about available interfaces
- **Status**: ✅ **COMPLETED** - Both empty DisplayManager files removed

### 1.4 Consolidate Documentation Files ✅
- **Issue**: Scattered markdown files in root directory
- **Files affected**: 
  - `drone-synth.md`
  - `keyboard-controls.md`
  - `strings-synth.md`
- **Action**: Create a `docs/` directory and move all documentation there
- **Benefit**: Cleaner project root, better organization
- **Status**: ✅ **COMPLETED** - Created `docs/` directory and moved all documentation files

---

## 2. Code Structure and Architecture

### 2.1 Extract Hardware Pin Definitions
- **Issue**: Pin definitions scattered throughout `main.cpp`
- **Current location**: Lines 30-38 in `main.cpp`
- **Action**: Create `include/hardware_config.h` with all pin definitions
- **Benefit**: Centralized hardware configuration, easier to modify for different boards

### 2.2 Separate MIDI Handler Functions
- **Issue**: MIDI callback functions are in `main.cpp` as free functions
- **Functions**: `OnNoteOn`, `OnNoteOff`, `OnControlChange`, `OnPitchChange`, `queryUSBDeviceInfo`
- **Action**: Create `MIDIController` class to encapsulate MIDI handling logic
- **Benefit**: Better separation of concerns, easier testing, clearer responsibility

### 2.3 Extract Display Logic
- **Issue**: TFT display initialization and update logic mixed into main loop
- **Current location**: `main.cpp` setup() and loop()
- **Action**: Create `DisplayController` class (reusing empty DisplayManager files)
- **Benefit**: Cleaner main loop, testable display logic

### 2.4 Consolidate Global Variables
- **Issue**: Multiple globals in `main.cpp`
- **Variables**: `count`, `fb`, `fb_internal`, `diff1`, `diff2`, `gfx`, `tft`, `tftAvailable`
- **Action**: Encapsulate in appropriate classes (DisplayController, Application)
- **Benefit**: Reduced global state, clearer ownership

---

## 3. Header File Issues

### 3.1 Add Include Guards Consistency
- **Issue**: Mix of `#pragma once` and traditional include guards
- **Files**: Most use `#pragma once`, but `karplus_strong_string_synth.h` uses `#ifndef`
- **Action**: Standardize on `#pragma once` throughout (more concise, supported on Teensy)
- **Benefit**: Consistency, slightly faster compilation

### 3.2 Minimize Header Dependencies
- **Issue**: Some headers include unnecessary dependencies
- **Example**: `karplus_strong_string_synth.h` includes `<ILI9341_t4.h>` and `<SPI.h>` when only pointer is needed
- **Action**: Use forward declarations where possible, move includes to .cpp files
- **Benefit**: Faster compilation, clearer dependencies

### 3.3 Organize Header Includes
- **Issue**: Inconsistent include ordering
- **Action**: Standardize include order:
  1. Standard library headers
  2. Third-party library headers
  3. Project headers
  4. Add blank lines between groups
- **Benefit**: Readability, easier to spot missing includes

---

## 4. Class Design Improvements

### 4.1 Encapsulate `KarplusStrongStringSynth` Display Members
- **Issue**: Public member variables for thread/display functionality
- **Location**: `karplus_strong_string_synth.h` lines 72-87
- **Variables**: `displayBufferIndex`, `displayThreadId`, `tftInitialized`, `gfx`, `state`, `bufferLen`, `buffers`, `bufferGeneration`, `frequency`
- **Action**: Make private with accessor methods or friend functions
- **Benefit**: Better encapsulation, controlled access

### 4.2 Improve Voice Management Architecture
- **Issue**: Similar voice management code duplicated across `DroneSynthesizer` and `StringPadSynthesizer`
- **Action**: Create base `VoiceManager` template or abstract class
- **Benefit**: DRY principle, consistent behavior, easier to add new synth types

### 4.3 Standardize Parameter Ranges
- **Issue**: Inconsistent parameter ranges across synthesizers
- **Examples**: 
  - Some use 0.0-1.0 normalized
  - Some use actual units (Hz, ms)
  - MIDI CC handlers do conversion inline
- **Action**: Define clear parameter interfaces with documentation
- **Benefit**: Predictable behavior, easier to understand

### 4.4 Add Parameter Validation
- **Issue**: Limited bounds checking on parameter setters
- **Action**: Add validation in all setter methods with documented ranges
- **Benefit**: Prevents invalid states, easier debugging

---

## 5. Memory Management

### 5.1 Document AudioConnection Ownership
- **Issue**: Raw pointers to `AudioConnection` objects managed manually
- **Locations**: All synthesizer classes
- **Action**: Add comments explaining ownership, consider smart pointers or RAII wrappers
- **Benefit**: Clearer lifecycle management, prevent memory leaks

### 5.2 Review Dynamic Allocation in Voice Structures
- **Issue**: Voice structures use `new`/`delete` for audio connections
- **Action**: Consider pre-allocated connection pools or static allocation
- **Benefit**: Avoid heap fragmentation, more predictable performance

### 5.3 Optimize Buffer Sizes
- **Issue**: Large static buffers (e.g., `DMAMEM uint16_t fb_internal[240 * 320]`)
- **Action**: Review memory usage, ensure buffers are necessary size
- **Benefit**: More memory for audio processing

---

## 6. Audio Architecture

### 6.1 Centralize Audio Memory Allocation
- **Issue**: `AudioMemory(20)` called in `main.cpp` - unclear if sufficient
- **Location**: `main.cpp` line 73
- **Action**: Calculate required blocks based on active voices/effects, document reasoning
- **Benefit**: Prevents audio glitches, clearer resource planning

### 6.2 Standardize Mixer Gain Levels
- **Issue**: Magic numbers for mixer gains throughout code
- **Examples**: 
  - `oscMixer.gain(0, 0.4f)` in DroneSynthesizer
  - `ensembleMixer.gain(0, 0.35f)` in StringPadSynthesizer
- **Action**: Define named constants with comments explaining choices
- **Benefit**: Easier to tune, understand mixing ratios

### 6.3 Add Audio Performance Monitoring
- **Issue**: `AudioPeakMonitor` exists but limited usage
- **Action**: Expand to track CPU usage, memory usage, provide diagnostics
- **Benefit**: Debug audio dropouts, optimize performance

---

## 7. Configuration Management

### 7.1 Remove Commented Code
- **Issue**: Commented conditional compilation (`#define DISABLE_TFT`, `#ifdef TFT_DISPLAY`)
- **Location**: `main.cpp` lines 12, 85
- **Action**: Either implement proper feature flags or remove dead code
- **Benefit**: Cleaner code, working feature toggles

### 7.2 Create Configuration File
- **Issue**: Default parameters scattered throughout code
- **Action**: Create `config.h` with all tunable parameters
- **Examples**:
  - Default volumes
  - Envelope times
  - Filter ranges
  - Split point for SPLIT mode
- **Benefit**: Easy experimentation, clear defaults

### 7.3 Consolidate Mode Definitions
- **Issue**: `SynthMode` enum in `HybridSynthesizer.h` duplicated in string form in comments
- **Action**: Add string conversion methods, use enum consistently
- **Benefit**: Type safety, easier debugging

---

## 8. Control Flow and Logic

### 8.1 Simplify MIDI CC Handler
- **Issue**: Massive `OnControlChange` function with nested if statements
- **Location**: `main.cpp` lines 185-368
- **Action**: Use map/table of CC numbers to handler functions or switch statement
- **Benefit**: Much more readable, easier to add new controls

### 8.2 Extract Mode Switching Logic
- **Issue**: Mode switching CC handlers (51-58) contain duplicate Serial.println patterns
- **Action**: Create helper function for mode switching with logging
- **Benefit**: DRY principle, consistent logging

### 8.3 Refactor Preset Loading
- **Issue**: CC handlers 53-57 all follow same pattern
- **Action**: Use array mapping CC to preset, single handler
- **Benefit**: Reduced code duplication

### 8.4 Improve Loop Function Structure
- **Issue**: `loop()` function mixes USB polling, MIDI reading, synth updates, and display
- **Action**: Extract into clearly named functions: `processInput()`, `updateSynthesis()`, `updateDisplay()`
- **Benefit**: Clearer responsibility, easier to understand timing

---

## 9. Error Handling and Validation

### 9.1 Add TFT Initialization Error Handling
- **Issue**: TFT initialization failure only prints "failed"
- **Location**: `main.cpp` line 86
- **Action**: Provide more context, continue gracefully, offer diagnostic information
- **Benefit**: Better user experience, easier troubleshooting

### 9.2 Validate MIDI Input Ranges
- **Issue**: No bounds checking on MIDI values before use
- **Action**: Add validation in all MIDI handlers
- **Benefit**: Prevent out-of-bounds errors, safer operation

### 9.3 Add Voice Allocation Failure Handling
- **Issue**: Silent failure when no voices available
- **Action**: Log when voice allocation fails, potentially implement voice stealing
- **Benefit**: Understand polyphony limitations, better user feedback

---

## 10. Code Quality and Style

### 10.1 Standardize Naming Conventions
- **Issue**: Mix of naming styles
- **Examples**:
  - `updateSamples` vs `UpdateEnvelope`
  - `noteOn` vs `NoteOn`
  - `fb` vs `framebuffer`
- **Action**: Choose consistent style (suggest: camelCase for methods, descriptive names)
- **Benefit**: Professional appearance, easier code navigation

### 10.2 Add Function Documentation
- **Issue**: Most functions lack documentation
- **Action**: Add Doxygen-style comments for all public methods
- **Include**: Purpose, parameters, return values, side effects
- **Benefit**: Self-documenting code, easier for others to use

### 10.3 Improve Variable Names
- **Issue**: Single-letter and abbreviated names
- **Examples**: 
  - `fb`, `gfx`, `vel`, `freq`
  - Loop variables: `i`, `t`
- **Action**: Use descriptive names except in tight loops or math formulas
- **Benefit**: Self-documenting, easier to understand

### 10.4 Consistent Brace Style
- **Issue**: Mix of brace placement styles
- **Action**: Standardize on K&R or Allman style throughout
- **Benefit**: Professional consistency

---

## 11. Serial Communication and Debugging

### 11.1 Implement Logging Levels
- **Issue**: `Serial.print` statements everywhere, no way to control verbosity
- **Action**: Create simple logging macro with levels (DEBUG, INFO, WARNING, ERROR)
- **Benefit**: Reduce serial spam, focused debugging

### 11.2 Standardize Serial Output Format
- **Issue**: Inconsistent logging format
- **Examples**: Mix of "Note On, ch=..." and "LFO rate: ... Hz"
- **Action**: Define standard format: `[LEVEL] Subsystem: Message`
- **Benefit**: Easier to parse, professional appearance

### 11.3 Add Optional Serial Disable
- **Issue**: Serial always active, adds overhead
- **Action**: Add compile-time flag to disable all serial output
- **Benefit**: Potentially better performance in production

### 11.4 Extract USB Device Query Function
- **Issue**: Large `queryUSBDeviceInfo()` function could be separate module
- **Location**: `main.cpp` lines 385-470
- **Action**: Move to `USBDeviceInfo` class or file
- **Benefit**: Cleaner main.cpp, reusable component

---

## 12. Mathematical and DSP Code

### 12.1 Document Karplus-Strong Algorithm
- **Issue**: Complex algorithm lacks explanation
- **Location**: `karplus_stong_string_synth.cpp`
- **Action**: Add detailed comments explaining the physics and math
- **Benefit**: Maintainable by others, educational value

### 12.2 Extract Magic Numbers
- **Issue**: Unexplained constants in DSP code
- **Examples**: 
  - `16807` in pseudorand
  - `0.0833333f` in frequency calculation
  - `7.0f / frequency` in attenuation
- **Action**: Define as named constants with comments
- **Benefit**: Understandable DSP code

### 12.3 Optimize Fixed-Point Calculations
- **Issue**: 16.16 fixed-point used but could be clearer
- **Location**: `karplus_strong_string_synth.cpp`
- **Action**: Create fixed-point helper functions/macros with documentation
- **Benefit**: Self-documenting math, easier to modify precision

---

## 13. Platform-Specific Code

### 13.1 Abstract Teensy-Specific Features
- **Issue**: Direct use of `TeensyThreads`, `DMAMEM` macros
- **Action**: Consider abstraction layer or clear documentation
- **Benefit**: Potentially port to other platforms, clearer dependencies

### 13.2 Document Hardware Requirements
- **Issue**: Pin usage documented in comment block but not validated
- **Action**: Add compile-time or runtime checks for proper configuration
- **Benefit**: Catch wiring errors early

---

## 14. Testing and Validation

### 14.1 Add Unit Tests
- **Issue**: No tests present
- **Action**: Add tests for:
  - Parameter validation
  - Voice allocation logic
  - Mode switching
  - MIDI conversion functions
- **Benefit**: Confidence in changes, prevent regressions

### 14.2 Add Integration Tests
- **Issue**: No way to test without hardware
- **Action**: Create mock audio/MIDI layer for simulation
- **Benefit**: Develop without physical Teensy

### 14.3 Add Performance Benchmarks
- **Issue**: No measurement of audio processing time
- **Action**: Add timing measurements for critical paths
- **Benefit**: Optimize bottlenecks, ensure real-time performance

---

## 15. Build System and Dependencies

### 15.1 Document Library Versions
- **Issue**: `platformio.ini` doesn't specify versions
- **Current**: `lib_deps = USBHost_t36, ILI9341_t4, adafruit/Adafruit GFX Library@^1.12.1`
- **Action**: Pin all dependencies to specific versions
- **Benefit**: Reproducible builds, avoid breakage

### 15.2 Add Build Configuration Options
- **Issue**: No easy way to build with/without TFT, different features
- **Action**: Add build flags in platformio.ini for feature toggles
- **Benefit**: Flexible builds, test configurations

### 15.3 Consider Adding CI
- **Issue**: No automated build verification
- **Action**: Add GitHub Actions to build on commit
- **Benefit**: Catch compile errors early

---

## 16. Unused Code Review

### 16.1 Review Synthesizer.h Usage
- **Issue**: `Synthesizer.h` appears to be superseded by `HybridSynthesizer`
- **Action**: Determine if still needed, remove or mark deprecated
- **Benefit**: Reduce confusion about which class to use

### 16.2 Review chords.h
- **Issue**: `chords.h` present but usage unclear
- **Action**: Document purpose or remove if unused
- **Benefit**: Clear codebase

### 16.3 Review example_tft_usage.cpp
- **Issue**: Example file in root, unclear if maintained
- **Action**: Move to `examples/` directory or remove if outdated
- **Benefit**: Cleaner project structure

---

## 17. Specific Bug Risks

### 17.1 Fix Potential Memory Leak in Synthesizer.h
- **Issue**: `patchCordSumL` assigned twice in constructor (lines 47-48)
- **Location**: `Synthesizer.h` lines 47-48
- **Action**: Create separate `patchCordSumL2` variable
- **Impact**: **HIGH** - Memory leak on each instance

### 17.2 Review Uninitialized Variables
- **Issue**: Some member variables may not be initialized in all constructors
- **Action**: Use member initializer lists consistently
- **Benefit**: Prevent undefined behavior

### 17.3 Add Overflow Protection
- **Issue**: Integer overflow possible in time calculations
- **Example**: `millis()` wraps after 49 days
- **Action**: Use proper wraparound-safe comparison
- **Benefit**: Long-term stability

---

## Priority Recommendations

### High Priority (Fix First)
1. **Fix memory leak in Synthesizer.h** (17.1)
2. **Rename karplus_stong_string_synth.cpp** (1.1)
3. **Remove or organize .ignore files** (1.2)
4. **Add parameter validation** (4.4)
5. **Simplify MIDI CC handler** (8.1)

### Medium Priority (Architecture Improvements)
6. **Extract hardware pin definitions** (2.1)
7. **Create MIDIController class** (2.2)
8. **Standardize naming conventions** (10.1)
9. **Add function documentation** (10.2)
10. **Implement logging levels** (11.1)

### Low Priority (Polish)
11. **Organize documentation files** (1.4)
12. **Add unit tests** (14.1)
13. **Improve variable names** (10.3)
14. **Document DSP algorithms** (12.1)

---

## Conclusion

This document represents a comprehensive review of the TeensySynth codebase. Not all recommendations need to be implemented immediately - prioritize based on:

- **Safety**: Fix bugs and memory issues first
- **Maintainability**: Improve code organization and documentation
- **Extensibility**: Refactor architecture to support future features
- **Performance**: Optimize only after measuring

The codebase shows good functional implementation with room for structural improvements. Focus on incremental refactoring while maintaining working functionality.
