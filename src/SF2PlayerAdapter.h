#pragma once

#include <AudioStream.h>
#include <SFZSynth.h>
#include <SF2Sound.h>
#include <OutBuffer.h>
#include <SdFat.h>
#include <string>
#include <functional>
#include <memory>


class SdFatStreamAdapter : public std::istream {
private:
    class SdFatStreamBuf : public std::streambuf {
    private:
        FsFile* sdFile;
        char* buffer;
        static const size_t bufferSize = 1024;
        
    public:
        SdFatStreamBuf(FsFile* file) : sdFile(file) {
            buffer = new char[bufferSize];
            setg(buffer, buffer, buffer);
        }
        
        ~SdFatStreamBuf() {
            delete[] buffer;
        }
        
    protected:
        int underflow() override {
            if (gptr() < egptr()) {
                return traits_type::to_int_type(*gptr());
            }
            
            size_t numRead = sdFile->read(buffer, bufferSize);
            if (numRead <= 0) {
                return traits_type::eof();
            }
            
            setg(buffer, buffer, buffer + numRead);
            return traits_type::to_int_type(*gptr());
        }
        
        std::streampos seekoff(std::streamoff off, std::ios_base::seekdir way, 
                              std::ios_base::openmode which = std::ios_base::in) override {
            uint64_t pos;
            if (way == std::ios_base::beg) {
                pos = off;
            } else if (way == std::ios_base::cur) {
                pos = sdFile->curPosition() + off;
            } else if (way == std::ios_base::end) {
                pos = sdFile->size() + off;
            } else {
                return -1;
            }
            
            if (sdFile->seekSet(pos)) {
                setg(buffer, buffer, buffer); // Reset buffer
                return pos;
            }
            return -1;
        }
        
        std::streampos seekpos(std::streampos sp, 
                              std::ios_base::openmode which = std::ios_base::in) override {
            return seekoff(sp, std::ios_base::beg, which);
        }
    };
    
    SdFatStreamBuf* streamBuf;
    FsFile sdFile;  // Use stack allocation instead of pointer
    
public:
    SdFatStreamAdapter(const std::string& path) : std::istream(nullptr) {
        if (sdFile.open(path.c_str(), O_RDONLY)) {
            streamBuf = new SdFatStreamBuf(&sdFile);
            rdbuf(streamBuf);
        } else {
            streamBuf = nullptr;
            setstate(std::ios::failbit);
        }
    }
    
    ~SdFatStreamAdapter() {
        if (streamBuf) delete streamBuf;
        if (sdFile.isOpen()) {
            sdFile.close();
        }
    }
    
    bool is_open() const {
        return sdFile.isOpen();
    }
    
    uint64_t size() const {
        return const_cast<FsFile&>(sdFile).isOpen() ? const_cast<FsFile&>(sdFile).size() : 0;
    }
};

// Adapter to make SF2Sound work with Teensy Audio Library
class SF2PlayerAdapter : public AudioStream {
private:
    SFZSynth* synth;
    SF2Sound* sound;
    bool initialized;
    
    // OutBuffer implementation for SFZPlayer
    class TeensyOutBuffer : public OutBuffer {
    private:
        float* leftChannel;
        float* rightChannel;
        uint32_t numChannels_;
        
    public:
        TeensyOutBuffer(float* left, float* right) 
            : leftChannel(left), rightChannel(right), numChannels_(2) {}
            
        float* samples_for_channel_32(int channel) override {
            return (channel == 0) ? leftChannel : rightChannel;
        }
        
        uint32_t num_channels() override {
            return numChannels_;
        }
    };

public:
    SF2PlayerAdapter() : AudioStream(0, nullptr), synth(nullptr), sound(nullptr), initialized(false) {
        // Create synth with 8 voices (polyphony)
        synth = new SFZSynth(8);
        synth->set_sample_rate(AUDIO_SAMPLE_RATE_EXACT);
        
        // Enable 2 output channels for stereo
        active = true;
    }
    
    ~SF2PlayerAdapter() {
        if (sound) delete sound;
        if (synth) delete synth;
    }
    
    bool loadSF2(const char* filename) {
        if (sound) {
            delete sound;
            sound = nullptr;
        }
        
        // Create file factory for SF2Sound using SdFat library
        auto file_factory = [](const std::string &path) -> std::unique_ptr<std::istream>
        {
            auto adapter = std::make_unique<SdFatStreamAdapter>(path);
            if (!adapter->is_open()) {
                return nullptr;  // File couldn't be opened
            }
            return std::move(adapter);
        };

        // Simple error handling without exceptions
        sound = new SF2Sound(file_factory, std::string(filename));
        if (!sound) {
            initialized = false;
            return false;
        }
        
        // SF2Sound handles the reading internally
        sound->load_regions();
        sound->load_samples();
        
        synth->set_sound(sound);
        initialized = true;
        return true;
    }
    
    void noteOn(int channel, int note, int velocity) {
        if (initialized && synth) {
            double vel = velocity / 127.0;
            synth->note_on(note, vel, channel, note);
        }
    }
    
    void noteOff(int channel, int note) {
        if (initialized && synth) {
            synth->note_off(note, 0.0, channel, note, true);
        }
    }
    
    void setPitchBend(int channel, float bendSemitones) {
        if (initialized && synth) {
            synth->tuning_expression_changed(bendSemitones);
        }
    }
    
    bool isLoaded() const {
        return initialized && sound != nullptr;
    }
    
    // Access to SF2-specific functionality
    int getNumPresets() const {
        return sound ? sound->num_subsounds() : 0;
    }
    
    std::string getPresetName(int presetIndex) const {
        return sound ? sound->subsound_name(presetIndex) : "";
    }
    
    void selectPreset(int presetIndex) {
        if (sound) {
            sound->use_subsound(presetIndex);
        }
    }
    
    int getSelectedPreset() const {
        return sound ? sound->selected_subsound() : 0;
    }
    
    virtual void update(void) override {
        if (!initialized || !synth) {
            return;
        }
        
        audio_block_t *blockL = allocate();
        audio_block_t *blockR = allocate();
        
        if (!blockL || !blockR) {
            if (blockL) release(blockL);
            if (blockR) release(blockR);
            return;
        }
        
        // Convert int16_t buffers to float for SFZPlayer
        float leftFloat[AUDIO_BLOCK_SAMPLES];
        float rightFloat[AUDIO_BLOCK_SAMPLES];
        
        // Clear buffers
        memset(leftFloat, 0, sizeof(leftFloat));
        memset(rightFloat, 0, sizeof(rightFloat));
        
        // Create output buffer adapter
        TeensyOutBuffer outBuffer(leftFloat, rightFloat);
        
        // Render audio from SFZPlayer
        synth->render(&outBuffer, 0, AUDIO_BLOCK_SAMPLES);
        
        // Convert float back to int16_t and scale
        for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
            // Scale and clamp
            float scaledL = leftFloat[i] * 32767.0f;
            float scaledR = rightFloat[i] * 32767.0f;
            
            if (scaledL > 32767.0f) scaledL = 32767.0f;
            if (scaledL < -32767.0f) scaledL = -32767.0f;
            if (scaledR > 32767.0f) scaledR = 32767.0f;
            if (scaledR < -32767.0f) scaledR = -32767.0f;
            
            blockL->data[i] = (int16_t)scaledL;
            blockR->data[i] = (int16_t)scaledR;
        }
        
        transmit(blockL, 0);
        transmit(blockR, 1);
        
        release(blockL);
        release(blockR);
    }
};
