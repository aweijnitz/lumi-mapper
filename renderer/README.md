# Renderer

**What it is**

The Renderer is the real-time visual engine that runs on a machine attached to a projector. It receives control messages from the Server over a simple newline-delimited JSON protocol, loads Scenes (geometry, surfaces) and Feeds (video files), and renders mapped video output in real time. It also optionally consumes audio input (to modulate scale) and MIDI (to control brightness).

---

## Key responsibilities

- Connect to the Server (default: `127.0.0.1:5050`) and announce itself via a `Hello` message.
- Accept ordered messages (JSON-per-line) such as:
  - `LoadScene` / `LoadSceneDefinition` — load scene and referenced feeds
  - `SetFeedForSurface` — change feed on a surface
  - `PlayCue` — playback cue-related overrides
  - `ShowTestPattern` — toggle calibration grid overlay
  - `ShowCrosshair` — show/hide alignment crosshair
- Load video feeds (VideoFile-type) and manage looping playback via `ofVideoPlayer`.
- Load image feeds (ImageFile-type) with pan animation settings.
- Render feeds to skewed surfaces using textured meshes, mapping normalized scene coordinates (-1..1) to screen pixels.
- Apply audio energy smoothing to scale the output and (optionally) map MIDI CC to brightness.
- Apply scene-level filter settings (color tint / monochrome). When the scene filter is `none`, local keyboard toggles apply.

Implementation notes:
- Core renderer logic: `src/ofApp.cpp` and `src/RenderState.cpp`.
- Networking: `src/net/RendererClient.cpp` (client that connects to server) and `src/net/RendererServer.cpp` (test helper server implementation used by unit tests).
- Message serialization uses the `projection::core` message types and newline-delimited JSON encoding.

---

## Build

Requirements:
- CMake >= 3.20
- A C++17-capable compiler (clang/gcc)
- A working openFrameworks installation (OpenFrameworks must be compiled for your platform)
- Optional: rtaudio (for audio input) and an ofxMidi-enabled build for MIDI

Set `OPENFRAMEWORKS_DIR` to your openFrameworks root (or set `OPENFRAMEWORKS_DIR` in a CMake cache GUI):

```bash
# from project root
cmake -S renderer -B renderer/build -DOPENFRAMEWORKS_DIR=/path/to/openFrameworks
cmake --build renderer/build --target renderer_default -j

# build tests
cmake --build renderer/build --target renderer_default_tests -j
```

You can also build the whole project from the repo root; renderer will be included as a subproject.

---

## Run

Basic usage (from repo root build output):

```bash
# example - connect to local server, give renderer a name and enable verbose logging
./build/renderer/renderer_default --server-host 127.0.0.1 --server-port 5050 --name studio-a --verbose
```

Command-line flags (also available via environment variables):
- `--server-host` / env `RENDERER_HOST` (default `127.0.0.1`)
- `--server-port` / env `RENDERER_PORT` (default `5050`)
- `--name` / env `RENDERER_NAME` (default `renderer-<pid>`)
- `--verbose` (enable verbose logging)
- `--disable-audio` / `--no-audio` or env `RENDERER_DISABLE_AUDIO` (disable audio input)
- `--connect-retries` or env `RENDERER_CONNECT_RETRIES` (default `10`)
- `--fullscreen` / `--windowed` or env `RENDERER_FULLSCREEN` (default windowed)
- `--display` or env `RENDERER_DISPLAY` (display index, 0 = primary)
- `--width` / `--height` or env `RENDERER_WIDTH` / `RENDERER_HEIGHT`
- `--resolution WxH` (sets width/height in one flag)

Behavioral notes:
- If audio initialization fails the renderer logs a warning and continues without audio.
- The renderer prints status (connected server, current scene, last command, errors) in the application window.
- Calibration grid can be toggled via `ShowTestPattern` or the `G` key (off → solid → overlay).
- Crosshair overlay auto-hides if no updates arrive (used during vertex dragging).

Keyboard shortcuts (in the renderer window):
- `G` cycle calibration grid (off/solid/overlay)
- `M` toggle monochrome filter
- `T` toggle color tint
- `D` toggle dramatic mode
- `P` cycle color palette
- `I` toggle debug info
- `V` toggle verbose logging

---

## Tests

A set of unit and integration tests cover the renderer networking and state code.

Build and run tests (in the renderer build directory):

```bash
# build tests (see Build section)
cmake --build renderer/build --target renderer_default_tests -j

# run via ctest
cd renderer/build
ctest -R renderer_default_tests -V

# or run the test binary directly
./renderer_default_tests
```

Notable tests:
- `RendererClient_test.cpp` — exercises the socket handshake, incoming command handling and ack flow using a test server.
- `RendererServer_test.cpp` — tests server-side message parsing and ack/error responses.
- `RenderState_test.cpp` — validates feed-to-filepath mapping and player lifecycle.

---

## Development tips

- Use `OPENFRAMEWORKS_DIR` and `OPENFRAMEWORKS_PLATFORM` to point CMake at your local OF installation.
- When editing message handling, update tests under `renderer/tests` that simulate server messages.
- Video feeds are loaded from the file path extracted from `Feed`'s VideoFile config (see `RenderState::mapVideoFeedFilePaths`). Ensure feed `configJson` contains a valid `filePath` for `VideoFile` feed types.
- The rendering pipeline uses textured meshes to map feed pixels to transformed surface geometry — see `ofApp::draw()`.

---

## Troubleshooting

- "Failed to connect to server": check server host/port and network loopback access.
- "audio input failed to initialize": audio is optional; run with `--disable-audio` if you don't want audio or install required sound backends.
- Video feeds not visible: verify the `filePath` referenced by the Feed exists and is accessible to the renderer process.
