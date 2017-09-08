#include "AnalogHiRes.h"
#include "MIDI_Controller.h"

using namespace ExtIO;

AnalogHiRes::AnalogHiRes(pin_t analogPin, uint8_t channel) // Constructor
{
  this->analogPin = analogPin;
  this->channel = channel;
}

AnalogHiRes::~AnalogHiRes() // Destructor
{
  free(avValues); // free the sample buffer malloc'ed in Analog::average
}

void AnalogHiRes::average(size_t length) // use the average of multiple samples of analog readings
{
  if (length == 0 || length == 1 || avLen) // the average of 0 or 1 samples is meaningless, if "av" already exists, don't allocate new memory
    return;
  avValues = (unsigned int *)malloc(length * sizeof(unsigned int)); // allocate memory for the sample buffer
  memset(avValues, 0, length * sizeof(unsigned int));               // set all values in the buffer to zero
  avLen = length;
}

void AnalogHiRes::refresh() // read the analog value, update the average, map it to a MIDI value, check if it changed since last time, if so, send Control Change message over MIDI
{
  analogRead(analogPin); // throw away first analog reading
  analog_t input = analogRead(analogPin);
  uint16_t value = analogMap(input) << 4; // apply the analogMap function to the value (identity function f(x) = x by default) and make it a 14-bit number (pad with 4 zeros)

  if (avLen)                       // if averaging is enabled
    value = runningAverage(value); // update the running average with the new value

  if (value != oldVal) // if the value changed since last time
  {
    MIDI_Controller.MIDI()->send(PITCH_BEND, channel + channelOffset * channelsPerBank, value, value >> 7); // send a Pitch Bend MIDI event
    oldVal = value;
  }
}

void AnalogHiRes::map(int (*fn)(int)) // change the function pointer for analogMap to a new function. It will be applied to the raw analog input value in Analog::refresh()
{
  analogMap = fn;
}

unsigned int AnalogHiRes::runningAverage(unsigned int value) // http://playground.arduino.cc/Main/RunningAverage
{
  avSum -= avValues[avIndex];
  avValues[avIndex] = value;
  avSum += avValues[avIndex];
  avIndex++;
  avIndex = avIndex % avLen;
  if (avCount < avLen)
    avCount++;

  return avSum / avCount;
}