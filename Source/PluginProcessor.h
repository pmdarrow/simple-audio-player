#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>
#include <memory>

// AU instrument that plays a single embedded WAV on button press.
//
// Threading model:
//   - UI thread owns the editor; writes `playing` and `resetRequested`.
//   - Audio thread reads both flags in processBlock and never blocks or allocates.
//   - No locks; synchronisation is via std::atomic with acquire/release ordering.
class TrackPlayerProcessor final : public juce::AudioProcessor {
 public:
  TrackPlayerProcessor();
  ~TrackPlayerProcessor() override;

  void prepareToPlay(double sampleRate, int samplesPerBlock) override;
  void releaseResources() override;
  bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
  void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

  juce::AudioProcessorEditor* createEditor() override;
  bool hasEditor() const override { return true; }

  const juce::String getName() const override { return "Track Player"; }
  // AU Music Devices must advertise MIDI input (IS_SYNTH=TRUE). MIDI is accepted
  // but ignored — playback is button-triggered from the UI.
  bool acceptsMidi() const override { return true; }
  bool producesMidi() const override { return false; }
  bool isMidiEffect() const override { return false; }
  double getTailLengthSeconds() const override { return 0.0; }

  int getNumPrograms() override { return 1; }
  int getCurrentProgram() override { return 0; }
  void setCurrentProgram(int) override {}
  const juce::String getProgramName(int) override { return {}; }
  void changeProgramName(int, const juce::String&) override {}

  void getStateInformation(juce::MemoryBlock& destData) override;
  void setStateInformation(const void* data, int sizeInBytes) override;

  // UI-thread API. Safe to call from the message thread at any time.
  void startPlayback();
  void stopPlayback();
  bool isPlaying() const noexcept { return playing.load(std::memory_order_acquire); }
  bool hasLoadedAudio() const noexcept { return fileBuffer.getNumSamples() > 0; }
  double getPlaybackPositionSeconds() const noexcept;
  double getTotalLengthSeconds() const noexcept;

 private:
  void loadAudioFile();

  juce::AudioFormatManager formatManager;

  // Decoded PCM at the file's native sample rate. ResamplingAudioSource handles
  // the conversion to the host rate at playback time, so we never re-decode or
  // re-resample during processBlock.
  juce::AudioBuffer<float> fileBuffer;
  double fileSampleRate{0.0};

  std::unique_ptr<juce::MemoryAudioSource> memorySource;
  std::unique_ptr<juce::ResamplingAudioSource> resamplingSource;

  double hostSampleRate{0.0};

  // `playing`        — true while the audio thread should render samples.
  // `resetRequested` — one-shot flag that causes the next audio block to seek
  //                    to 0 and flush the resampler. Set by Start/Stop on the
  //                    UI thread, and by the audio thread itself on EOF so the
  //                    next Play press starts cleanly from the beginning.
  std::atomic<bool> playing{false};
  std::atomic<bool> resetRequested{false};

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrackPlayerProcessor)
};
