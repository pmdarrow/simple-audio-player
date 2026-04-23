# Track Player

An Audio Unit instrument plugin for macOS that plays a WAV file on a button
press. Built with [JUCE](https://juce.com/) 8 and CMake.

The plugin currently expects the track at
`~/Downloads/Luke Melville - El Monte.wav`. If the file isn't there the Play
button is disabled and the UI shows a "file not found" message.

## Requirements

- macOS 10.13 or later (Intel or Apple Silicon)
- [CMake](https://cmake.org/) 3.22 or later
- [Ninja](https://ninja-build.org/) (`brew install ninja`)
- Xcode command-line tools (`xcode-select --install`) — a full Xcode install is
  recommended so the AU can be validated with `auval`
- Git (used by CMake's `FetchContent` to pull JUCE)

Optional, for dev tooling:

- `brew install llvm` — provides `clang-format` and `clang-tidy` used by the
  `format` / `format-check` / `tidy` CMake targets and the git pre-commit hook.

## Building

From the project root:

```bash
cmake -B build -G Ninja
cmake --build build
```

The first configure step clones JUCE 8.0.12 into `build/_deps/` via
`FetchContent`; this can take a few minutes. Subsequent builds are incremental.

On success the Audio Unit bundle is produced at:

```
build/TrackPlayer_artefacts/AU/Track Player.component
```

For a universal (Intel + Apple Silicon) distribution build, override the
architecture at configure time:

```bash
cmake -B build-universal -G Ninja -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64"
cmake --build build-universal
```

Because `COPY_PLUGIN_AFTER_BUILD` is enabled in [CMakeLists.txt](CMakeLists.txt),
the bundle is also copied into the user plugin folder:

```
~/Library/Audio/Plug-Ins/Components/Track Player.component
```

### Validating the plugin (optional)

macOS caches AU metadata aggressively. After the first install (or any time you
re-build) you may want to:

```bash
killall -9 AudioComponentRegistrar || true
auval -v aumu Trkp Pdar
```

`aumu` is the AU type for music devices / instruments. `Trkp` is this plugin's
four-character code and `Pdar` is the manufacturer code — both defined in
[CMakeLists.txt](CMakeLists.txt).

## Using the plugin

1. Launch an AU-compatible host (Logic Pro, GarageBand, Ableton Live 11+,
   Reaper, AUM, …).
2. Create a new instrument track and choose **Peter Darrow → Track Player**.
3. Click **Play** to start the track; click again to stop. Playback auto-stops
   at the end of the file. Position / length is displayed beneath the button.

## Development setup

All dev tooling is driven by CMake targets and a checked-in git hook — no
Python/Node frameworks required.

The plugin is built with Apple Clang (standard for the macOS AU flow);
`clang-format` and `clang-tidy` come from Homebrew's keg-only `llvm`, which
the CMake targets and git hook locate automatically.

### One-time setup after cloning

```bash
brew install llvm                           # clang-format + clang-tidy
git config core.hooksPath .githooks         # activate the checked-in hook
```

The pre-commit hook ([.githooks/pre-commit](.githooks/pre-commit)) runs
`clang-format --dry-run` against staged C/C++ files and blocks the commit if
anything would be reformatted. Bypass with `git commit --no-verify`.

### Available CMake targets

| Target          | What it does                                              |
| --------------- | --------------------------------------------------------- |
| `format`        | Rewrite sources in place via clang-format.                |
| `format-check`  | Fail if any source needs reformatting. Useful in CI.      |
| `tidy`          | Run clang-tidy across the sources.                        |

Run any of them with `cmake --build build --target <name>`.

Canonical configs:

- Style — [.clang-format](.clang-format)
- Lint  — [.clang-tidy](.clang-tidy) (rationale for each disabled check is inline)

## Project layout

```
.
├── CMakeLists.txt
├── README.md
├── .clang-format                    # code style
├── .clang-tidy                      # lint config
├── .githooks/pre-commit             # checked-in pre-commit hook
└── Source/
    ├── PluginProcessor.h / .cpp     # juce::AudioProcessor subclass
    └── PluginEditor.h   / .cpp      # juce::AudioProcessorEditor subclass
```

## Upgrading JUCE

Bump the `GIT_TAG` in [CMakeLists.txt](CMakeLists.txt) and re-run the configure
step. Any 8.x tag should be drop-in compatible.
