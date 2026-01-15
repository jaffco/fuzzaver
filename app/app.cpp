#include "app.h"

class Phasor {
private:
  float phase = 0.f;
  float frequency = 220.f; // Default to A3
  const float sampleRate = 48000.f;
  float phaseInc = 0.f;

public:
  void setFrequency(float freq) {
    frequency = freq;
    phaseInc = frequency / sampleRate;
  }  

  float process() {
    phase += phaseInc;
    if (phase >= 1.f)
      phase -= 1.f;
    return phase;
  }

};

// Process one audio sample
// This is exported to the host and called for each audio sample
float process(float input) {
  static Phasor phasor;
  static bool initialized = false;     
  if (!initialized) {
    phasor.setFrequency(220.f);
    initialized = true;
  }   
  float output = phasor.process();
  return output;
}
