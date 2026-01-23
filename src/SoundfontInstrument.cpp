#include "SoundfontInstrument.h"
#include <Arduino.h>

SoundfontInstrument::SoundfontInstrument()
    : instrumentData(nullptr)
    , centsOffsets(nullptr)
    , sampleCount(0)
    , loaded(false)
    , instrumentIndex(-1)
    , volume(1.0f)
    , attackMs(5.0f)
    , decayMs(200.0f)
    , sustainLevel(0.4f)
    , releaseMs(300.0f)
    , filterMultiplier(5.0f)
    , filterResonance(1.0f)
    , crossfadeDurationMs(100.0f)
{
    name[0] = '\0';
    filename[0] = '\0';
    
    // Initialize voice states
    for (int i = 0; i < VOICES_PER_INSTRUMENT; ++i) {
        voiceStates[i].active = false;
        voiceStates[i].midiNote = -1;
        voiceStates[i].noteOnTime = 0;
        voiceConnections[i] = nullptr;
        envelopeToFilterConnections[i] = nullptr;
        envelopeToUnfilteredConnections[i] = nullptr;
        filterToFaderConnections[i] = nullptr;
        filteredToMixerConnections[i] = nullptr;
        unfilteredToMixerConnections[i] = nullptr;
        crossfadeToFinalConnections[i] = nullptr;
    }
    
    // Connect audio routing:
    // voice → envelope → filter → filteredFader → crossfadeMixer[0]
    //                  → unfilteredFader → crossfadeMixer[1]
    //                     crossfadeMixer → finalMixer
    for (int i = 0; i < VOICES_PER_INSTRUMENT; ++i) {
        voiceConnections[i] = new AudioConnection(voices[i], 0, envelopes[i], 0);
        
        // Filtered path
        envelopeToFilterConnections[i] = new AudioConnection(envelopes[i], 0, filters[i], 0);
        filterToFaderConnections[i] = new AudioConnection(filters[i], 0, filteredFaders[i], 0);
        filteredToMixerConnections[i] = new AudioConnection(filteredFaders[i], 0, crossfadeMixers[i], 0);
        
        // Unfiltered path
        envelopeToUnfilteredConnections[i] = new AudioConnection(envelopes[i], 0, unfilteredFaders[i], 0);
        unfilteredToMixerConnections[i] = new AudioConnection(unfilteredFaders[i], 0, crossfadeMixers[i], 1);
        
        // Crossfade mixer to final mixer
        crossfadeToFinalConnections[i] = new AudioConnection(crossfadeMixers[i], 0, mixer, i);
        
        // Set default ADSR parameters
        envelopes[i].attack(attackMs);
        envelopes[i].decay(decayMs);
        envelopes[i].sustain(sustainLevel);
        envelopes[i].release(releaseMs);
        
        // Set default filter parameters (frequency will be set per-note)
        filters[i].resonance(filterResonance);
        
        // Set crossfade mixer gains (equal mix initially)
        crossfadeMixers[i].gain(0, 1.0f);  // Filtered
        crossfadeMixers[i].gain(1, 1.0f);  // Unfiltered
        crossfadeMixers[i].gain(2, 0.0f);  // Unused
        crossfadeMixers[i].gain(3, 0.0f);  // Unused
    }
    
    // Initialize fader delays based on attack + decay
    updateFaderDelays();
    
    // Set initial mixer gains
    updateMixerGains();
}

SoundfontInstrument::~SoundfontInstrument() {
    // Clean up audio connections
    for (int i = 0; i < VOICES_PER_INSTRUMENT; ++i) {
        delete voiceConnections[i];
        delete envelopeToFilterConnections[i];
        delete envelopeToUnfilteredConnections[i];
        delete filterToFaderConnections[i];
        delete filteredToMixerConnections[i];
        delete unfilteredToMixerConnections[i];
        delete crossfadeToFinalConnections[i];
    }
    
    // Clean up instrument data
    if (instrumentData != nullptr) {
        delete instrumentData;
        instrumentData = nullptr;
    }
    
    // Clean up cents offsets array
    if (centsOffsets != nullptr) {
        delete[] centsOffsets;
        centsOffsets = nullptr;
    }
}

bool SoundfontInstrument::loadInstrument(const char* filename, int instrumentIndex) {
    Serial.print("SoundfontInstrument: Loading ");
    Serial.print(filename);
    Serial.print("[");
    Serial.print(instrumentIndex);
    Serial.println("]");
    
    // Ensure filename starts with "/" for SD library compatibility
    char fullPath[256];
    if (filename[0] != '/') {
        snprintf(fullPath, sizeof(fullPath), "/%s", filename);
    } else {
        strncpy(fullPath, filename, sizeof(fullPath) - 1);
        fullPath[sizeof(fullPath) - 1] = '\0';
    }
    
    // Check if file exists
    if (!SD.exists(fullPath)) {
        Serial.print("SoundfontInstrument: File not found: ");
        Serial.println(fullPath);
        return false;
    }
    
    // Store old instrument data to delete after switching
    AudioSynthWavetable::instrument_data* oldInstrument = instrumentData;
    int* oldCentsOffsets = centsOffsets;
    
    // Load instrument data (temp format with CENTS_OFFSET)
    SF22ASWT::instrument_data_temp inst_temp;
    bool success = false;
    
    if (sf2Reader.ReadFile(fullPath)) {
        if (sf2Reader.Load_instrument_data(instrumentIndex, inst_temp)) {
            // Store cents offsets before conversion
            sampleCount = inst_temp.sample_count;
            centsOffsets = new int[sampleCount];
            for (int i = 0; i < sampleCount; ++i) {
                centsOffsets[i] = inst_temp.samples[i].CENTS_OFFSET;
            }
            
            // Load sample data and convert to AudioSynthWavetable format
            if (sf2Reader.ReadSampleDataFromFile(inst_temp)) {
                instrumentData = new AudioSynthWavetable::instrument_data(
                    SF22ASWT::converter::to_AudioSynthWavetable_instrument_data(inst_temp)
                );
                success = true;
            }
        }
    } else {
        Serial.println("SoundfontInstrument: Failed to read SF2 file");
    }
    
    if (!success) {
        Serial.println("SoundfontInstrument: Failed to load instrument");
        instrumentData = oldInstrument;  // Restore old data
        centsOffsets = oldCentsOffsets;
        return false;
    }
    
    // Clean up old instrument data
    if (oldInstrument != nullptr) {
        delete oldInstrument;
    }
    if (oldCentsOffsets != nullptr) {
        delete[] oldCentsOffsets;
    }
    
    // Store instrument metadata
    snprintf(name, sizeof(name), "%s[%d]", filename, instrumentIndex);
    strncpy(this->filename, fullPath, sizeof(this->filename) - 1);
    this->filename[sizeof(this->filename) - 1] = '\0';
    this->instrumentIndex = instrumentIndex;
    loaded = true;
    
    Serial.println("SoundfontInstrument: Instrument loaded successfully");
    return true;
}

bool SoundfontInstrument::isLoaded() const {
    return loaded;
}

const char* SoundfontInstrument::getName() const {
    return name;
}

void SoundfontInstrument::unload() {
    Serial.println("SoundfontInstrument: Unloading instrument");
    
    // Stop all voices
    allNotesOff();
    
    // Clean up instrument data
    if (instrumentData != nullptr) {
        delete instrumentData;
        instrumentData = nullptr;
    }
    
    // Clean up cents offsets
    if (centsOffsets != nullptr) {
        delete[] centsOffsets;
        centsOffsets = nullptr;
    }
    sampleCount = 0;
    
    // Clear metadata
    loaded = false;
    name[0] = '\0';
    filename[0] = '\0';
    instrumentIndex = -1;
}

void SoundfontInstrument::noteOn(int midiNote, float velocity) {
    if (!loaded) {
        return;
    }
    
    // Find an available voice
    int voiceIndex = findAvailableVoice();
    if (voiceIndex == -1) {
        // No free voice, steal the oldest one
        voiceIndex = findOldestVoice();
        voices[voiceIndex].stop();
    }
    
    // Set the instrument on this voice
    voices[voiceIndex].setInstrument(*instrumentData);
    
    // Calculate note frequency and set filter cutoff
    float noteFreq = AudioSynthWavetable::noteToFreq(midiNote);
    float filterFreq = noteFreq * filterMultiplier;
    filterFreq = constrain(filterFreq, 20.0f, 20000.0f);
    filters[voiceIndex].frequency(filterFreq);
    
    // Convert velocity to 0-127 range for AudioSynthWavetable
    int vel = constrain(velocity * 127.0f, 0, 127);
    
    // Find which sample will be used for this note (same logic as AudioSynthWavetable::setState)
    int sampleIndex = 0;
    for (int i = 0; midiNote > instrumentData->sample_note_ranges[i]; i++) {
        sampleIndex = i + 1;
    }
    
    // Print tuning information with original cents offset
    Serial.printf("NoteOn: note=%d, sample=%d, centsOffset=%d\n", 
                  midiNote, sampleIndex, 
                  (sampleIndex < sampleCount) ? centsOffsets[sampleIndex] : 0);
    
    // Start the note
    voices[voiceIndex].playNote(midiNote, vel);
    
    // Trigger envelope
    envelopes[voiceIndex].noteOn();
    
    // Trigger crossfade: fade in filtered, fade out unfiltered
    uint32_t delayMs = attackMs + decayMs;
    filteredFaders[voiceIndex].fadeIn(crossfadeDurationMs, delayMs);
    unfilteredFaders[voiceIndex].fadeOut(crossfadeDurationMs, delayMs);
    
    // Update voice state
    voiceStates[voiceIndex].active = true;
    voiceStates[voiceIndex].midiNote = midiNote;
    voiceStates[voiceIndex].noteOnTime = millis();
}

void SoundfontInstrument::noteOff(int midiNote) {
    // Find the voice playing this note
    int voiceIndex = findVoicePlayingNote(midiNote);
    if (voiceIndex != -1) {
        // Trigger envelope release (let it decay naturally)
        envelopes[voiceIndex].noteOff();
        
        // Mark voice as inactive (will be reused after release completes)
        voiceStates[voiceIndex].active = false;
        voiceStates[voiceIndex].midiNote = -1;
    }
}

void SoundfontInstrument::allNotesOff() {
    for (int i = 0; i < VOICES_PER_INSTRUMENT; ++i) {
        voices[i].stop();
        envelopes[i].noteOff();  // Trigger release for all envelopes
        voiceStates[i].active = false;
        voiceStates[i].midiNote = -1;
    }
}

int SoundfontInstrument::getActiveVoiceCount() const {
    int count = 0;
    for (int i = 0; i < VOICES_PER_INSTRUMENT; ++i) {
        if (voiceStates[i].active) {
            count++;
        }
    }
    return count;
}

void SoundfontInstrument::setVolume(float vol) {
    volume = constrain(vol, 0.0f, 1.0f);
    updateMixerGains();
}

float SoundfontInstrument::getVolume() const {
    return volume;
}

AudioStream* SoundfontInstrument::getOutput() {
    return &mixer;
}

void SoundfontInstrument::setAttack(float milliseconds) {
    attackMs = constrain(milliseconds, 0.0f, 11880.0f);
    for (int i = 0; i < VOICES_PER_INSTRUMENT; ++i) {
        envelopes[i].attack(attackMs);
    }
    updateFaderDelays();
}

void SoundfontInstrument::setDecay(float milliseconds) {
    decayMs = constrain(milliseconds, 0.0f, 11880.0f);
    for (int i = 0; i < VOICES_PER_INSTRUMENT; ++i) {
        envelopes[i].decay(decayMs);
    }
    updateFaderDelays();
}

void SoundfontInstrument::setSustain(float level) {
    sustainLevel = constrain(level, 0.0f, 1.0f);
    for (int i = 0; i < VOICES_PER_INSTRUMENT; ++i) {
        envelopes[i].sustain(sustainLevel);
    }
}

void SoundfontInstrument::setRelease(float milliseconds) {
    releaseMs = constrain(milliseconds, 0.0f, 11880.0f);
    for (int i = 0; i < VOICES_PER_INSTRUMENT; ++i) {
        envelopes[i].release(releaseMs);
    }
}

void SoundfontInstrument::setADSR(float attack, float decay, float sustain, float release) {
    setAttack(attack);
    setDecay(decay);
    setSustain(sustain);
    setRelease(release);
}

void SoundfontInstrument::setFilterMultiplier(float multiplier) {
    filterMultiplier = constrain(multiplier, 0.5f, 20.0f);
    
    // Update filter frequency for all active voices
    for (int i = 0; i < VOICES_PER_INSTRUMENT; ++i) {
        if (voiceStates[i].active && voiceStates[i].midiNote >= 0) {
            float noteFreq = AudioSynthWavetable::noteToFreq(voiceStates[i].midiNote);
            float filterFreq = noteFreq * filterMultiplier;
            filterFreq = constrain(filterFreq, 20.0f, 20000.0f);
            filters[i].frequency(filterFreq);
        }
    }
}

void SoundfontInstrument::setFilterResonance(float q) {
    filterResonance = constrain(q, 0.7f, 5.0f);
    for (int i = 0; i < VOICES_PER_INSTRUMENT; ++i) {
        filters[i].resonance(filterResonance);
    }
}

void SoundfontInstrument::setCrossfadeDuration(float milliseconds) {
    crossfadeDurationMs = constrain(milliseconds, 0.0f, 5000.0f);
}

void SoundfontInstrument::updateFaderDelays() {
    // Fader delays are set dynamically in noteOn() based on current attack+decay
    // This method is kept for future use if we need to update existing active fades
}

int SoundfontInstrument::findAvailableVoice() {
    for (int i = 0; i < VOICES_PER_INSTRUMENT; ++i) {
        if (!voiceStates[i].active) {
            return i;
        }
    }
    return -1;
}

int SoundfontInstrument::findVoicePlayingNote(int midiNote) {
    for (int i = 0; i < VOICES_PER_INSTRUMENT; ++i) {
        if (voiceStates[i].active && voiceStates[i].midiNote == midiNote) {
            return i;
        }
    }
    return -1;
}

int SoundfontInstrument::findOldestVoice() {
    int oldestIndex = 0;
    unsigned long oldestTime = voiceStates[0].noteOnTime;
    
    for (int i = 1; i < VOICES_PER_INSTRUMENT; ++i) {
        if (voiceStates[i].noteOnTime < oldestTime) {
            oldestTime = voiceStates[i].noteOnTime;
            oldestIndex = i;
        }
    }
    
    return oldestIndex;
}

void SoundfontInstrument::updateMixerGains() {
    // Apply volume to all mixer channels (4 voices max)
    // Use 0.25 gain per voice to prevent overflow when all 4 are active
    for (int i = 0; i < VOICES_PER_INSTRUMENT; ++i) {
        mixer.gain(i, volume * 0.25f);
    }
}
