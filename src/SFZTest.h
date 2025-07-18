#pragma once

// Test include to verify SFZPlayer library is accessible
#include <SFZSynth.h>
#include <SFZSound.h>
#include <SFZReader.h>
#include <OutBuffer.h>

// Simple test class to verify linking works
class SFZTest {
public:
    SFZTest() {
        // This should compile if the library is properly linked
        synth = new SFZSynth(4);
    }
    
    ~SFZTest() {
        delete synth;
    }
    
private:
    SFZSynth* synth;
};
