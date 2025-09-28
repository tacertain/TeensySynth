#include "StringPadSynthesizer.h"
#include <Arduino.h>

// Preset definitions for classic 80s string pad sounds
const StringPadSynthesizer::PresetData StringPadSynthesizer::presetData[PRESET_COUNT] = {
    // PRESET_LUSH_PADS - "I Ran" style lush string pads
    {
        .filterCutoff = 0.4f,       // Warm, not too bright
        .filterResonance = 0.3f,    // Gentle resonance
        .detuneAmount = 0.7f,       // Rich ensemble effect
        .attackTime = 200.0f,       // Smooth attack
        .releaseTime = 800.0f       // Long, sustaining release
    },
    // PRESET_BRIGHT_STRINGS - Brighter, more aggressive strings
    {
        .filterCutoff = 1.0f,       // Brighter sound
        .filterResonance = 0.3f,    // More character
        .detuneAmount = 0.68f,       // Moderate ensemble
        .attackTime = 150.0f,       // Quicker attack
        .releaseTime = 600.0f       // Medium release
    },
    // PRESET_SOFT_ENSEMBLE - Soft, subtle ensemble strings  
    {
        .filterCutoff = 0.3f,       // Very warm
        .filterResonance = 0.2f,    // Subtle resonance
        .detuneAmount = 0.4f,       // Gentle ensemble
        .attackTime = 300.0f,       // Slow, gentle attack
        .releaseTime = 1200.0f      // Very long release
    },
    // PRESET_ANALOG_WARMTH - Warm analog-style pads
    {
        .filterCutoff = 0.35f,      // Warm analog sound
        .filterResonance = 0.5f,    // Analog-style resonance
        .detuneAmount = 0.8f,       // Heavy ensemble for analog feel
        .attackTime = 250.0f,       // Classic analog attack
        .releaseTime = 1000.0f      // Standard analog release
    },
    // PRESET_SHIMMER - Shimmery, ethereal strings
    {
        .filterCutoff = 0.6f,       // Bright and airy
        .filterResonance = 0.6f,    // Emphasize harmonics
        .detuneAmount = 0.9f,       // Maximum ensemble shimmer
        .attackTime = 400.0f,       // Slow, building attack
        .releaseTime = 1500.0f      // Long, shimmering decay
    }
};

// StringVoice implementation
StringPadSynthesizer::StringVoice::StringVoice() 
    : active(false)
    , midiNote(-1)
    , baseFrequency(0.0f)
    , velocity(0.0f)
    , noteOnTime(0)
    , noteOffTime(0)
    , releasing(false)
    , voiceId(-1)
    , currentGain(0.0f)
    , attackTimeMs(200.0f)            // Default 200ms attack
    , releaseTimeMs(1000.0f)          // Default 1000ms release
    , patchCord1(nullptr)
    , patchCord2(nullptr)
    , patchCord3(nullptr)
    , patchCord4(nullptr)
    , patchCord5(nullptr)
    , patchCord6(nullptr)
{
}

StringPadSynthesizer::StringVoice::~StringVoice() {
    cleanup();
}

void StringPadSynthesizer::StringVoice::initialize(int id) {
    voiceId = id;
    // Set up oscillator waveforms - all sawtooth for classic string sound
    osc1.begin(1.0, 440.0, WAVEFORM_SAWTOOTH);
    osc2.begin(1.0, 440.0, WAVEFORM_SAWTOOTH);
    osc3.begin(1.0, 440.0, WAVEFORM_SAWTOOTH);

    // Set ensemble mixer gains for balanced ensemble sound
    ensembleMixer.gain(0, 0.35f);  // osc1 (center)
    ensembleMixer.gain(1, 0.33f);  // osc2 (sharp)
    ensembleMixer.gain(2, 0.32f);  // osc3 (flat)
    ensembleMixer.gain(3, 0.0f);   // unused
    
    // Initialize lowpass filter for warm string pad sound
    filter.frequency(800.0f);      // Warm initial cutoff
    filter.resonance(0.3f);        // Gentle resonance
    filter.octaveControl(7.0f);    // Full range control
    
    // Initialize highpass filter to remove sub-harmonics and DC offset
    highpassFilter.frequency(80.0f);   // Default frequency, will be updated per note
    highpassFilter.resonance(0.7f);    // Minimal resonance for clean filtering
    highpassFilter.octaveControl(7.0f); // Full range control
    
    // Create audio connections: oscillators -> mixer -> lowpass -> highpass -> envelope
    patchCord1 = new AudioConnection(osc1, 0, ensembleMixer, 0);
    patchCord2 = new AudioConnection(osc2, 0, ensembleMixer, 1);
    patchCord3 = new AudioConnection(osc3, 0, ensembleMixer, 2);
    patchCord4 = new AudioConnection(ensembleMixer, 0, filter, 0);
    patchCord5 = new AudioConnection(filter, 0, highpassFilter, 0);
    patchCord6 = new AudioConnection(highpassFilter, 2, envAmp, 0);
    
    // Initialize envelope amplifier
    envAmp.gain(0.0f);
    
    currentGain = 0.0f;
}

void StringPadSynthesizer::StringVoice::cleanup() {
    delete patchCord1;
    delete patchCord2;
    delete patchCord3;
    delete patchCord4;
    delete patchCord5;
    delete patchCord6;
    patchCord1 = patchCord2 = patchCord3 = patchCord4 = patchCord5 = patchCord6 = nullptr;
}

void StringPadSynthesizer::StringVoice::startNote(int note, float freq, float vel) {
    active = true;
    midiNote = note;
    baseFrequency = freq;
    velocity = vel;
    noteOnTime = millis();
    noteOffTime = 0;
    releasing = false;
    currentGain = 0.0f;  // Start from silence for attack
    
    // Set base frequencies - will be updated with detuning by parent class
    osc1.frequency(baseFrequency);
    osc2.frequency(baseFrequency);
    osc3.frequency(baseFrequency);

    // Set highpass filter - will be updated with current multiplier by parent class
    float highpassFreq = baseFrequency * 0.8f;
    if (highpassFreq < 40.0f) highpassFreq = 40.0f;
    highpassFilter.frequency(highpassFreq);
    
    // Start with zero gain - envelope will ramp up
    envAmp.gain(0.0f);
}

void StringPadSynthesizer::StringVoice::stopNote() {
    if (active && !releasing) {
        releasing = true;
        noteOffTime = millis();
    }
}

void StringPadSynthesizer::StringVoice::updateEnvelope() {
    if (!active) return;
    
    unsigned long currentTime = millis();
    
    if (!releasing) {
        // Attack phase - ramp up from 0 to velocity level
        unsigned long attackTime = currentTime - noteOnTime;
        
        // Use configurable attack time instead of hardcoded value
        if (attackTime < (unsigned long)attackTimeMs) {
            float attackProgress = (float)attackTime / attackTimeMs;
            currentGain = velocity * attackProgress;
        } else {
            // Sustain phase
            currentGain = velocity;
        }
        
        envAmp.gain(currentGain);
        
    } else {
        // Release phase - ramp down to 0
        if (noteOffTime == 0) {
            noteOffTime = currentTime;
        }
        
        unsigned long releaseTime = currentTime - noteOffTime;
        
        // Use configurable release time instead of hardcoded value
        if (releaseTime < (unsigned long)releaseTimeMs) {
            float releaseProgress = (float)releaseTime / releaseTimeMs;
            currentGain = velocity * (1.0f - releaseProgress);
            envAmp.gain(currentGain);
        } else {
            // Note finished
            currentGain = 0.0f;
            envAmp.gain(0.0f);
            active = false;
        }
    }
}

void StringPadSynthesizer::StringVoice::updateOscillatorFrequencies(float baseFreq, float detuneAmount) {
    if (!active) return;
    
    // Convert detune amount (0.0-1.0) to cents (0-15 cents)
    float maxDetuneCents = 15.0f;
    float detuneCents = detuneAmount * maxDetuneCents;
    
    // Convert cents to frequency multipliers
    float sharpMultiplier = pow(2.0f, (detuneCents * 0.7f) / 1200.0f);  // +7 cents at max
    float flatMultiplier = pow(2.0f, (-detuneCents * 0.5f) / 1200.0f);  // -5 cents at max
    
    osc1.frequency(baseFreq);                    // Center frequency
    osc2.frequency(baseFreq * sharpMultiplier);  // Slightly sharp
    osc3.frequency(baseFreq * flatMultiplier);   // Slightly flat
}

void StringPadSynthesizer::StringVoice::updateFilter(float cutoff, float resonance) {
    // Map cutoff (0.0-1.0) to frequency range (200-2000 Hz)
    float filterFreq = 200.0f + (cutoff * 1800.0f);
    
    filter.frequency(filterFreq);
    filter.resonance(resonance * 4.0f); // Scale for reasonable resonance range
}

void StringPadSynthesizer::StringVoice::updateHighpassFilter(float multiplier) {
    if (!active || baseFrequency <= 0.0f) return;
    
    // Calculate highpass frequency: base frequency * 0.8 * multiplier
    float highpassFreq = baseFrequency * 0.8f * multiplier;
    
    // Ensure minimum frequency to avoid very low frequency rumble
    if (highpassFreq < 40.0f) highpassFreq = 40.0f;
    
    // Ensure maximum frequency doesn't go too high
    if (highpassFreq > 8000.0f) highpassFreq = 8000.0f;
    
    highpassFilter.frequency(highpassFreq);
}

void StringPadSynthesizer::StringVoice::updateEnvelopeParameters(float attackMs, float releaseMs) {
    attackTimeMs = constrain(attackMs, 10.0f, 5000.0f);    // 10ms to 5s
    releaseTimeMs = constrain(releaseMs, 10.0f, 10000.0f); // 10ms to 10s
}

// StringPadSynthesizer implementation
StringPadSynthesizer::StringPadSynthesizer()
    : filterCutoff(0.4f)        // Start with warm sound
    , filterResonance(0.3f)     // Gentle resonance
    , chorusDepth(0.5f)         // Medium chorus depth (for future use)
    , detuneAmount(0.6f)        // Medium detuning for ensemble effect
    , attackTime(200.0f)        // 200ms attack
    , releaseTime(1000.0f)      // 1000ms release
    , highpassMultiplier(1.0f)  // Default highpass multiplier (CC 64 = 1.0)
    , chordMode(CHORD_MODE_MAJOR)  // Default to chord mode
    , currentPreset(PRESET_BRIGHT_STRINGS)  // Default to bright strings preset
{
    // Initialize all voices
    for (int i = 0; i < MAX_VOICES; i++) {
        voices[i].initialize(i);
        voiceConnections[i * 2] = nullptr;     // Left connection
        voiceConnections[i * 2 + 1] = nullptr; // Right connection
    }
    
    // Set up voice mixing for polyphony
    // Mixer 1: Voices 0-3 (4 inputs each)
    voiceMixerL1.gain(0, 0.5);  // Voice 0
    voiceMixerL1.gain(1, 0.5f);  // Voice 1
    voiceMixerL1.gain(2, 0.5f);  // Voice 2
    voiceMixerL1.gain(3, 0.5f);  // Voice 3
    
    voiceMixerR1.gain(0, 0.5f);  // Voice 0
    voiceMixerR1.gain(1, 0.5f);  // Voice 1
    voiceMixerR1.gain(2, 0.5f);  // Voice 2
    voiceMixerR1.gain(3, 0.5f);  // Voice 3
    
    // Mixer 2: Voices 4-5 + mix from mixer1 (3 inputs used)
    voiceMixerL2.gain(0, 1.0f);   // Mix from voiceMixerL1
    voiceMixerL2.gain(1, 0.5f);  // Voice 4
    voiceMixerL2.gain(2, 0.5f);  // Voice 5
    voiceMixerL2.gain(3, 0.0f);   // Unused
    
    voiceMixerR2.gain(0, 1.0f);   // Mix from voiceMixerR1
    voiceMixerR2.gain(1, 0.5f);  // Voice 4
    voiceMixerR2.gain(2, 0.5f);  // Voice 5
    voiceMixerR2.gain(3, 0.0f);   // Unused
    
    // Connect voices to mixers
    voiceConnections[0] = new AudioConnection(voices[0].envAmp, 0, voiceMixerL1, 0);  // Voice 0 -> L1
    voiceConnections[1] = new AudioConnection(voices[0].envAmp, 0, voiceMixerR1, 0);  // Voice 0 -> R1
    voiceConnections[2] = new AudioConnection(voices[1].envAmp, 0, voiceMixerL1, 1);  // Voice 1 -> L1
    voiceConnections[3] = new AudioConnection(voices[1].envAmp, 0, voiceMixerR1, 1);  // Voice 1 -> R1
    voiceConnections[4] = new AudioConnection(voices[2].envAmp, 0, voiceMixerL1, 2);  // Voice 2 -> L1
    voiceConnections[5] = new AudioConnection(voices[2].envAmp, 0, voiceMixerR1, 2);  // Voice 2 -> R1
    voiceConnections[6] = new AudioConnection(voices[3].envAmp, 0, voiceMixerL1, 3);  // Voice 3 -> L1
    voiceConnections[7] = new AudioConnection(voices[3].envAmp, 0, voiceMixerR1, 3);  // Voice 3 -> R1
    
    voiceConnections[8] = new AudioConnection(voices[4].envAmp, 0, voiceMixerL2, 1);  // Voice 4 -> L2
    voiceConnections[9] = new AudioConnection(voices[4].envAmp, 0, voiceMixerR2, 1);  // Voice 4 -> R2
    voiceConnections[10] = new AudioConnection(voices[5].envAmp, 0, voiceMixerL2, 2); // Voice 5 -> L2
    voiceConnections[11] = new AudioConnection(voices[5].envAmp, 0, voiceMixerR2, 2); // Voice 5 -> R2
    
    // Connect mixer1 to mixer2
    new AudioConnection(voiceMixerL1, 0, voiceMixerL2, 0);
    new AudioConnection(voiceMixerR1, 0, voiceMixerR2, 0);
    
    // Add final anti-aliasing filter after voice mixing
    finalFilterConnection = new AudioConnection(voiceMixerL2, 0, finalFilter, 0);
    
    // Set up final filter to remove high frequencies that could alias
    finalFilter.frequency(8000.0f);    // Low-pass at 8kHz to prevent aliasing
    finalFilter.resonance(0.7f);       // Minimal resonance for clean filtering
    finalFilter.octaveControl(7.0f);   // Full range control
}

StringPadSynthesizer::~StringPadSynthesizer() {
    // Clean up voice connections
    for (int i = 0; i < MAX_VOICES * 2; i++) {
        delete voiceConnections[i];
    }
    // Clean up final filter connection
    delete finalFilterConnection;
}

void StringPadSynthesizer::noteOn(int midiNote, float velocity, ChordMode chordMode) {
    if (velocity <= 0.0f || velocity > 1.0f) {
        return;
    }
    
    switch (chordMode) {
        case CHORD_MODE_MAJOR:
            // Play major chord instead of single note
            playMajorChord(midiNote, velocity);
            break;
        case CHORD_MODE_OCTAVE:
            // Play single note plus octave
            playOctaveNote(midiNote, velocity);
            break;
        default: // CHORD_MODE_OFF
            // Original single note behavior
            // Check if this note is already playing
            int existingVoice = findVoicePlayingNote(midiNote);
            if (existingVoice >= 0) {
                // Retrigger existing note - reuse the same voice
                float frequency = midiNoteToFrequency(midiNote);
                voices[existingVoice].startNote(midiNote, frequency, velocity);
                voices[existingVoice].updateOscillatorFrequencies(frequency, detuneAmount);
                voices[existingVoice].updateFilter(filterCutoff, filterResonance);
                voices[existingVoice].updateHighpassFilter(highpassMultiplier);
                break; // Exit here since we reused the existing voice
            }
            
            // Find an available voice
            int voiceIndex = findAvailableVoice();
            if (voiceIndex < 0) {
                // No available voices, steal the oldest one
                voiceIndex = findOldestVoice();
            }
            
            if (voiceIndex >= 0) {
                float frequency = midiNoteToFrequency(midiNote);
                voices[voiceIndex].startNote(midiNote, frequency, velocity);
                voices[voiceIndex].updateOscillatorFrequencies(frequency, detuneAmount);
                voices[voiceIndex].updateFilter(filterCutoff, filterResonance);
                voices[voiceIndex].updateHighpassFilter(highpassMultiplier);
            }
            break;
    }
}

void StringPadSynthesizer::noteOff(int midiNote, ChordMode chordMode) {
    switch (chordMode) {
        case CHORD_MODE_MAJOR:
            // Stop major chord
            stopMajorChord(midiNote);
            break;
        case CHORD_MODE_OCTAVE:
            // Stop octave notes
            stopOctaveNote(midiNote);
            break;
        default: // CHORD_MODE_OFF
            // Original single note behavior
            // Find the voice playing this note
            int voiceIndex = findVoicePlayingNote(midiNote);
            if (voiceIndex >= 0) {
                voices[voiceIndex].stopNote();
            }
            break;
    }
}

void StringPadSynthesizer::allNotesOff() {
    for (int i = 0; i < MAX_VOICES; i++) {
        if (voices[i].active) {
            voices[i].stopNote();
        }
    }
}

int StringPadSynthesizer::getActiveVoiceCount() const {
    int count = 0;
    for (int i = 0; i < MAX_VOICES; i++) {
        if (voices[i].active) {
            count++;
        }
    }
    return count;
}

void StringPadSynthesizer::setFilterCutoff(float cutoff) {
    filterCutoff = constrain(cutoff, 0.0f, 1.0f);
    updateVoiceParameters();
}

void StringPadSynthesizer::setFilterResonance(float resonance) {
    filterResonance = constrain(resonance, 0.0f, 1.0f);
    updateVoiceParameters();
}

void StringPadSynthesizer::setChorusDepth(float depth) {
    chorusDepth = constrain(depth, 0.0f, 1.0f);
    // Not implemented in Phase 1
}

void StringPadSynthesizer::setDetuneAmount(float detune) {
    detuneAmount = constrain(detune, 0.0f, 1.0f);
    updateVoiceParameters();
}

void StringPadSynthesizer::setHighpassMultiplier(float multiplier) {
    highpassMultiplier = constrain(multiplier, 0.05f, 4.0f);
    updateVoiceParameters();
}

void StringPadSynthesizer::setAttackTime(float attackMs) {
    attackTime = constrain(attackMs, 50.0f, 2000.0f);
    updateVoiceParameters();
}

void StringPadSynthesizer::setReleaseTime(float releaseMs) {
    releaseTime = constrain(releaseMs, 100.0f, 5000.0f);
    updateVoiceParameters();
}

AudioStream* StringPadSynthesizer::getOutput() {
    return &finalFilter;  // Return filtered output to prevent aliasing
}

void StringPadSynthesizer::processEnvelope() {
    for (int i = 0; i < MAX_VOICES; i++) {
        voices[i].updateEnvelope();
    }
}

float StringPadSynthesizer::midiNoteToFrequency(int midiNote) {
    // Standard MIDI note to frequency conversion
    // A4 (MIDI note 69) = 440 Hz
    return 440.0f * pow(2.0f, (midiNote - 69) / 12.0f);
}

float StringPadSynthesizer::mapCutoffToFrequency(float cutoff01) {
    // Map 0.0-1.0 to 200-2000 Hz with slight exponential curve for more musical response
    float linear = 200.0f + (cutoff01 * 1800.0f);
    return linear;
}

float StringPadSynthesizer::mapDetuneToSemitones(float detune01) {
    // Map 0.0-1.0 to 0-15 cents
    return detune01 * 0.15f; // 15 cents = 0.15 semitones
}

void StringPadSynthesizer::updateVoiceParameters() {
    updateAllVoiceParameters();
}

void StringPadSynthesizer::updateAllVoiceParameters() {
    for (int i = 0; i < MAX_VOICES; i++) {
        voices[i].updateFilter(filterCutoff, filterResonance);
        voices[i].updateHighpassFilter(highpassMultiplier);
        voices[i].updateEnvelopeParameters(attackTime, releaseTime);
        if (voices[i].active) {
            voices[i].updateOscillatorFrequencies(voices[i].baseFrequency, detuneAmount);
        }
    }
}

// Voice allocation methods
int StringPadSynthesizer::findAvailableVoice() {
    for (int i = 0; i < MAX_VOICES; i++) {
        if (!voices[i].active) {
            return i;
        }
    }
    return -1; // No available voices
}

int StringPadSynthesizer::findVoicePlayingNote(int midiNote) {
    for (int i = 0; i < MAX_VOICES; i++) {
        if (voices[i].active && voices[i].midiNote == midiNote) {
            return i;
        }
    }
    return -1; // Note not found
}

int StringPadSynthesizer::findOldestVoice() {
    int oldestVoice = 0;
    unsigned long oldestTime = voices[0].noteOnTime;
    
    for (int i = 1; i < MAX_VOICES; i++) {
        if (voices[i].noteOnTime < oldestTime) {
            oldestTime = voices[i].noteOnTime;
            oldestVoice = i;
        }
    }
    
    return oldestVoice;
}

// Preset system methods (Phase 3)
void StringPadSynthesizer::loadPreset(StringPadPreset preset) {
    if (preset >= PRESET_COUNT) return;
    
    currentPreset = preset;
    const PresetData& data = presetData[preset];
    
    // Load preset parameters
    setFilterCutoff(data.filterCutoff);
    setFilterResonance(data.filterResonance);
    setDetuneAmount(data.detuneAmount);
    setAttackTime(data.attackTime);
    setReleaseTime(data.releaseTime);
}

StringPadSynthesizer::StringPadPreset StringPadSynthesizer::getCurrentPreset() const {
    return currentPreset;
}

const char* StringPadSynthesizer::getPresetName(StringPadPreset preset) const {
    switch (preset) {
        case PRESET_LUSH_PADS:      return "Lush Pads";
        case PRESET_BRIGHT_STRINGS: return "Bright Strings";
        case PRESET_SOFT_ENSEMBLE:  return "Soft Ensemble";
        case PRESET_ANALOG_WARMTH:  return "Analog Warmth";
        case PRESET_SHIMMER:        return "Shimmer";
        default:                    return "Unknown";
    }
}

// Chord mode methods
void StringPadSynthesizer::setChordMode(ChordMode mode) {
    chordMode = mode;
}

StringPadSynthesizer::ChordMode StringPadSynthesizer::getChordMode() const {
    return chordMode;
}

void StringPadSynthesizer::playMajorChord(int rootNote, float velocity) {
    // Major chord intervals: Root, Major 3rd (4 semitones), Perfect 5th (7 semitones)
    // Play both the original chord and one octave higher (6 notes total)
    int chordNotes[6] = {
        rootNote,       // Root
        rootNote + 4,   // Major third
        rootNote + 7,   // Perfect fifth
        rootNote + 12,  // Root (octave up)
        rootNote + 16,  // Major third (octave up)
        rootNote + 19   // Perfect fifth (octave up)
    };
    
    // Play each note of the chord
    for (int i = 0; i < 6; i++) {
        // Check if this note is already playing
        int existingVoice = findVoicePlayingNote(chordNotes[i]);
        if (existingVoice >= 0) {
            // Retrigger existing note
            voices[existingVoice].stopNote();
        }
        
        // Find an available voice
        int voiceIndex = findAvailableVoice();
        if (voiceIndex < 0) {
            // No available voices, steal the oldest one
            voiceIndex = findOldestVoice();
        }
        
        if (voiceIndex >= 0) {
            float frequency = midiNoteToFrequency(chordNotes[i]);
            voices[voiceIndex].startNote(chordNotes[i], frequency, velocity / ((i < 3) ? 2.0f : 1.0f));
            voices[voiceIndex].updateOscillatorFrequencies(frequency, detuneAmount);
            voices[voiceIndex].updateFilter(filterCutoff, filterResonance);
            voices[voiceIndex].updateHighpassFilter(highpassMultiplier);
        }
    }
}

void StringPadSynthesizer::stopMajorChord(int rootNote) {
    // Major chord intervals: Root, Major 3rd (4 semitones), Perfect 5th (7 semitones)
    // Stop both the original chord and one octave higher (6 notes total)
    int chordNotes[6] = {
        rootNote,       // Root
        rootNote + 4,   // Major third
        rootNote + 7,   // Perfect fifth
        rootNote + 12,  // Root (octave up)
        rootNote + 16,  // Major third (octave up)
        rootNote + 19   // Perfect fifth (octave up)
    };
    
    // Stop each note of the chord
    for (int i = 0; i < 6; i++) {
        int voiceIndex = findVoicePlayingNote(chordNotes[i]);
        if (voiceIndex >= 0) {
            voices[voiceIndex].stopNote();
        }
    }
}

void StringPadSynthesizer::playOctaveNote(int rootNote, float velocity) {
    // Play the root note and the note one octave higher (2 notes total)
    int octaveNotes[2] = {
        rootNote,       // Root note
        rootNote + 12   // One octave higher
    };
    
    // Play each note
    for (int i = 0; i < 2; i++) {
        // Check if this note is already playing
        int existingVoice = findVoicePlayingNote(octaveNotes[i]);
        if (existingVoice >= 0) {
            // Retrigger existing note
            voices[existingVoice].stopNote();
        }
        
        // Find an available voice
        int voiceIndex = findAvailableVoice();
        if (voiceIndex < 0) {
            // No available voices, steal the oldest one
            voiceIndex = findOldestVoice();
        }
        
        if (voiceIndex >= 0) {
            float frequency = midiNoteToFrequency(octaveNotes[i]);
            voices[voiceIndex].startNote(octaveNotes[i], frequency, velocity);
            voices[voiceIndex].updateOscillatorFrequencies(frequency, detuneAmount);
            voices[voiceIndex].updateFilter(filterCutoff, filterResonance);
            voices[voiceIndex].updateHighpassFilter(highpassMultiplier);
        }
    }
}

void StringPadSynthesizer::stopOctaveNote(int rootNote) {
    // Stop the root note and the note one octave higher (2 notes total)
    int octaveNotes[2] = {
        rootNote,       // Root note
        rootNote + 12   // One octave higher
    };
    
    // Stop each note
    for (int i = 0; i < 2; i++) {
        int voiceIndex = findVoicePlayingNote(octaveNotes[i]);
        if (voiceIndex >= 0) {
            voices[voiceIndex].stopNote();
        }
    }
}
