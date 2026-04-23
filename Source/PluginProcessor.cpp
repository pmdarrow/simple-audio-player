#include "PluginProcessor.h"

#include "PluginEditor.h"

namespace {
// Temporary: read the track from a fixed path under the user's Downloads
// folder. This keeps the audio out of the repo while the project is being
// scaffolded. A future revision will load the file from a user-chosen path or
// bundle it into the plugin via juce_add_binary_data.
constexpr const char* kTrackFilename = "Luke Melville - El Monte.wav";
}  // namespace

TrackPlayerProcessor::TrackPlayerProcessor()
    : AudioProcessor(
          BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)
      ) {
  formatManager.registerBasicFormats();
  loadAudioFile();
}

TrackPlayerProcessor::~TrackPlayerProcessor() = default;

void TrackPlayerProcessor::loadAudioFile() {
  // Decode the track from disk once at construction so processBlock never
  // touches the filesystem. The whole file lives in fileBuffer at its native
  // sample rate; ResamplingAudioSource below adapts to the host rate.
  const juce::File trackFile = juce::File::getSpecialLocation(juce::File::userHomeDirectory)
                                   .getChildFile("Downloads")
                                   .getChildFile(kTrackFilename);

  if (!trackFile.existsAsFile()) {
    return;  // Button stays disabled; editor shows a "file not found" status.
  }

  std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(trackFile));
  if (reader == nullptr) {
    jassertfalse;  // decoder failed for an existing file
    return;
  }

  fileSampleRate = reader->sampleRate;

  const int numChannels = static_cast<int>(reader->numChannels);
  const int numSamples = static_cast<int>(reader->lengthInSamples);

  fileBuffer.setSize(numChannels, numSamples, false, true, false);
  // The last two args map source channels to buffer channels: always read the
  // left; only read the right if the source is stereo (mono sources are
  // duplicated to stereo output downstream in processBlock).
  reader->read(&fileBuffer, 0, numSamples, 0, true, numChannels > 1);

  // MemoryAudioSource provides a non-looping, seekable source over fileBuffer.
  // Wrapping it in ResamplingAudioSource lets us keep fileBuffer at the file's
  // native rate while still emitting samples at whatever the host asks for.
  memorySource = std::make_unique<juce::MemoryAudioSource>(fileBuffer, false, false);
  resamplingSource = std::make_unique<juce::ResamplingAudioSource>(
      memorySource.get(), false, juce::jmax(1, numChannels)
  );
}

void TrackPlayerProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
  hostSampleRate = sampleRate;

  // The host can change sample rate between sessions (or after bus config
  // changes), so we always recompute the ratio here. ratio > 1.0 speeds the
  // source up (host rate lower than file rate) and < 1.0 slows it down.
  if (resamplingSource != nullptr && fileSampleRate > 0.0) {
    resamplingSource->setResamplingRatio(fileSampleRate / sampleRate);
    resamplingSource->prepareToPlay(samplesPerBlock, sampleRate);
  }
}

void TrackPlayerProcessor::releaseResources() {
  if (resamplingSource != nullptr) resamplingSource->releaseResources();
}

bool TrackPlayerProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
  const auto& out = layouts.getMainOutputChannelSet();
  return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void TrackPlayerProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
  // Denormal floats cause huge CPU stalls on x86. Force FTZ/DAZ for the block.
  const juce::ScopedNoDenormals noDenormals;

  // Always emit defined silence first so every early-return below produces a
  // clean output buffer.
  buffer.clear();

  if (resamplingSource == nullptr || memorySource == nullptr) return;

  // Acquire pairs with the release stores in startPlayback()/stopPlayback() so
  // any UI-thread writes that preceded the flag flip are visible here.
  if (!playing.load(std::memory_order_acquire)) return;

  // One-shot seek-and-flush. exchange() makes this self-clearing so each
  // reset request is consumed exactly once — multiple rapid presses from the
  // UI still produce a single reset on the next audio tick.
  if (resetRequested.exchange(false, std::memory_order_acq_rel)) {
    memorySource->setNextReadPosition(0);
    resamplingSource->flushBuffers();
  }

  // The host's bus layout may not match the file's channel count (e.g. mono
  // bus on a stereo file, or vice versa). When they match we can render
  // directly into the output buffer; otherwise we render into a temp sized to
  // the file and mix/duplicate into the output.
  const int numOutputChannels = buffer.getNumChannels();
  const int numFileChannels = fileBuffer.getNumChannels();

  if (numOutputChannels == numFileChannels) {
    const juce::AudioSourceChannelInfo info(buffer);
    resamplingSource->getNextAudioBlock(info);
  } else {
    juce::AudioBuffer<float> temp(numFileChannels, buffer.getNumSamples());
    temp.clear();
    const juce::AudioSourceChannelInfo info(&temp, 0, buffer.getNumSamples());
    resamplingSource->getNextAudioBlock(info);

    if (numFileChannels == 1 && numOutputChannels >= 2) {
      // Mono → stereo (or wider): duplicate L to R; any extra channels stay
      // silent from the buffer.clear() above.
      buffer.copyFrom(0, 0, temp, 0, 0, buffer.getNumSamples());
      buffer.copyFrom(1, 0, temp, 0, 0, buffer.getNumSamples());
    } else if (numFileChannels >= 2 && numOutputChannels == 1) {
      // Stereo (or wider) → mono: average L+R. Simple fold-down; -6 dB
      // relative to summed energy but prevents clipping on correlated content.
      buffer.copyFrom(0, 0, temp, 0, 0, buffer.getNumSamples());
      buffer.addFrom(0, 0, temp, 1, 0, buffer.getNumSamples());
      buffer.applyGain(0.5f);
    } else {
      for (int ch = 0; ch < juce::jmin(numOutputChannels, numFileChannels); ++ch)
        buffer.copyFrom(ch, 0, temp, ch, 0, buffer.getNumSamples());
    }
  }

  // Natural end of file. Re-arm resetRequested so the next startPlayback()
  // begins from the top even though the UI-side startPlayback() also sets it
  // — belt-and-braces in case the UI observes isPlaying() going false and
  // tries to resume without re-pressing Play.
  if (memorySource->getNextReadPosition() >= memorySource->getTotalLength()) {
    playing.store(false, std::memory_order_release);
    resetRequested.store(true, std::memory_order_release);
  }
}

juce::AudioProcessorEditor* TrackPlayerProcessor::createEditor() {
  return new TrackPlayerEditor(*this);
}

void TrackPlayerProcessor::getStateInformation(juce::MemoryBlock&) {}
void TrackPlayerProcessor::setStateInformation(const void*, int) {}

void TrackPlayerProcessor::startPlayback() {
  if (!hasLoadedAudio()) return;

  // Order matters: arm the reset flag *before* flipping `playing`, so the very
  // first audio block observing playing=true also observes resetRequested=true
  // and seeks to 0 instead of picking up wherever the source happened to be.
  resetRequested.store(true, std::memory_order_release);
  playing.store(true, std::memory_order_release);
}

void TrackPlayerProcessor::stopPlayback() {
  // Stop output first, then arm a reset so the next Start begins from the top.
  playing.store(false, std::memory_order_release);
  resetRequested.store(true, std::memory_order_release);
}

double TrackPlayerProcessor::getPlaybackPositionSeconds() const noexcept {
  if (memorySource == nullptr || fileSampleRate <= 0.0) return 0.0;

  return static_cast<double>(memorySource->getNextReadPosition()) / fileSampleRate;
}

double TrackPlayerProcessor::getTotalLengthSeconds() const noexcept {
  if (fileSampleRate <= 0.0) return 0.0;

  return static_cast<double>(fileBuffer.getNumSamples()) / fileSampleRate;
}

// Required factory function the JUCE plugin wrapper calls to instantiate the
// processor — one per plugin instance loaded by the host.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TrackPlayerProcessor(); }
