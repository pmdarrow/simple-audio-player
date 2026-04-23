#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"

// The editor lives entirely on the message thread. It drives the processor
// via atomic flags (startPlayback/stopPlayback) and polls playback state at a
// low rate for the position readout — no direct access to audio-thread state.
class TrackPlayerEditor final : public juce::AudioProcessorEditor, private juce::Timer {
 public:
  explicit TrackPlayerEditor(TrackPlayerProcessor&);
  ~TrackPlayerEditor() override;

  void paint(juce::Graphics&) override;
  void resized() override;

 private:
  void timerCallback() override;
  void refresh();

  TrackPlayerProcessor& processor;

  juce::Label titleLabel;
  juce::Label subtitleLabel;
  juce::TextButton playButton;
  juce::Label statusLabel;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrackPlayerEditor)
};
