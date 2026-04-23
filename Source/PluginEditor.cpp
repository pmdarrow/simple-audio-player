#include "PluginEditor.h"

namespace {
// Guards against negatives and NaN/inf — getPlaybackPositionSeconds() can in
// principle return a junk value before prepareToPlay has run.
juce::String formatSeconds(double seconds) {
  if (seconds < 0.0 || !std::isfinite(seconds)) seconds = 0.0;

  const int total = static_cast<int>(std::floor(seconds));
  return juce::String::formatted("%d:%02d", total / 60, total % 60);
}
}  // namespace

TrackPlayerEditor::TrackPlayerEditor(TrackPlayerProcessor& p)
    : AudioProcessorEditor(&p), processor(p) {
  setSize(360, 220);
  // Fixed window: no resize controls, no constraints needed. Hosts that honour
  // editor sizing will render this at exactly 360x220.
  setResizable(false, false);

  titleLabel.setText("Track Player", juce::dontSendNotification);
  titleLabel.setJustificationType(juce::Justification::centred);
  titleLabel.setFont(juce::Font(juce::FontOptions(24.0f).withStyle("Bold")));
  addAndMakeVisible(titleLabel);

  subtitleLabel.setText("Luke Melville - El Monte", juce::dontSendNotification);
  subtitleLabel.setJustificationType(juce::Justification::centred);
  subtitleLabel.setFont(juce::Font(juce::FontOptions(13.0f)));
  subtitleLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.7f));
  addAndMakeVisible(subtitleLabel);

  playButton.setButtonText("Play");
  playButton.onClick = [this] {
    if (processor.isPlaying())
      processor.stopPlayback();
    else
      processor.startPlayback();

    // Immediate UI update on click — don't wait up to ~67 ms for the next
    // timer tick to swap the button label between Play/Stop.
    refresh();
  };
  // Disable the button when the embedded resource failed to decode, so the
  // user isn't tempted to press a button that can't do anything.
  playButton.setEnabled(processor.hasLoadedAudio());
  addAndMakeVisible(playButton);

  statusLabel.setJustificationType(juce::Justification::centred);
  statusLabel.setFont(juce::Font(juce::FontOptions(13.0f)));
  statusLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.7f));
  addAndMakeVisible(statusLabel);

  refresh();
  // 15 Hz is fast enough for a smooth mm:ss readout (sub-second granularity
  // is invisible) while keeping the editor effectively idle between frames.
  startTimerHz(15);
}

TrackPlayerEditor::~TrackPlayerEditor() { stopTimer(); }

void TrackPlayerEditor::paint(juce::Graphics& g) {
  g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void TrackPlayerEditor::resized() {
  auto bounds = getLocalBounds().reduced(20);

  titleLabel.setBounds(bounds.removeFromTop(34));
  subtitleLabel.setBounds(bounds.removeFromTop(20));

  bounds.removeFromTop(12);

  statusLabel.setBounds(bounds.removeFromBottom(24));
  playButton.setBounds(bounds.reduced(40, 10));
}

void TrackPlayerEditor::timerCallback() { refresh(); }

void TrackPlayerEditor::refresh() {
  playButton.setButtonText(processor.isPlaying() ? "Stop" : "Play");

  if (!processor.hasLoadedAudio()) {
    statusLabel.setText("Audio file not found in ~/Downloads", juce::dontSendNotification);
    return;
  }

  statusLabel.setText(
      formatSeconds(processor.getPlaybackPositionSeconds()) + " / " +
          formatSeconds(processor.getTotalLengthSeconds()),
      juce::dontSendNotification
  );
}
