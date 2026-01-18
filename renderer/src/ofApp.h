#pragma once

#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <vector>

#include <ofMain.h>
#if __has_include(<ofxMidi.h>)
#include <ofxMidi.h>
#define PROJECTION_HAS_OFX_MIDI 1
#else
#define PROJECTION_HAS_OFX_MIDI 0
#endif

#include "RenderState.h"
#include "net/RendererClient.h"
#include "util/InteractionUtils.h"

class ofApp : public ofBaseApp,
#if PROJECTION_HAS_OFX_MIDI
              public ofxMidiListener,
#endif
              public projection::renderer::RendererCommandHandler {
 public:
  explicit ofApp(std::string host,
                 int port,
                 std::string name,
                 int connectRetries,
                 bool verbose = false,
                 bool enableAudio = true);

  void setup() override;
  void update() override;
  void draw() override;
  void exit() override;

#if PROJECTION_HAS_OFX_MIDI
  void audioIn(ofSoundBuffer& input) override;
  void newMidiMessage(ofxMidiMessage& msg) override;
#else
  void audioIn(ofSoundBuffer& input) override;
#endif

  void handle(const projection::core::RendererMessage& message) override;

 private:
  void updateStatusForHello(const projection::core::HelloMessage& hello, const std::string& commandId);
  void updateStatusForLoadScene(const projection::core::LoadSceneMessage& loadScene,
                                const std::string& commandId);
  void updateStatusForSetFeed(const projection::core::SetFeedForSurfaceMessage& setFeed,
                              const std::string& commandId);
  void updateStatusForPlayCue(const projection::core::PlayCueMessage& playCue, const std::string& commandId);
  void processMessage(const projection::core::RendererMessage& message);

  projection::renderer::RendererClient client_;
  std::string host_;
  int port_;
  std::string name_;
  bool verbose_{false};

  projection::renderer::RenderState renderState_{};

  std::mutex queueMutex_{};
  std::queue<projection::core::RendererMessage> messageQueue_{};
  std::mutex stateMutex_{};
  std::mutex audioMutex_{};
  std::string lastCommand_;
  std::string lastError_;
  std::string sceneId_;
  std::string rendererRole_;
  std::string rendererVersion_;

#if PROJECTION_HAS_OFX_MIDI
  ofxMidiIn midiIn_{};
#endif
  float midiBrightness_{1.0f};

  ofSoundStream soundStream_{};
  std::vector<float> audioBuffer_{};
  float audioScale_{1.0f};
  float smoothedEnergy_{0.0f};
  bool audioEnabled_{false};
  bool audioRequested_{true};

  // Monochrome filter with color tints and dramatic effects
  ofFbo monoFbo_{};
  bool monochromeEnabled_{true};
  bool colorTintEnabled_{true};
  bool dramaticModeEnabled_{true};  // High contrast dramatic look
  int tintPaletteIndex_{0};         // Which palette to use

  // Color tint palette - creative colors for each surface
  struct TintColor {
    float r, g, b;
  };
  static constexpr int kNumPalettes = 5;
  static constexpr int kColorsPerPalette = 12;  // More colors for surface variety
  // Palettes: Mixed Neon, Cyan/Magenta, Fire/Ice, Tropical, Noir
  static constexpr TintColor kTintPalettes[kNumPalettes][kColorsPerPalette] = {
      // Palette 0: Mixed Neon - high contrast complementary colors
      {{0.0f, 0.9f, 1.0f},      // Electric cyan
       {1.0f, 0.0f, 0.6f},      // Hot pink
       {0.4f, 1.0f, 0.2f},      // Neon green
       {1.0f, 0.4f, 0.0f},      // Vivid orange
       {0.6f, 0.0f, 1.0f},      // Electric purple
       {1.0f, 1.0f, 0.0f},      // Bright yellow
       {0.0f, 0.6f, 1.0f},      // Azure blue
       {1.0f, 0.2f, 0.4f},      // Coral red
       {0.2f, 0.8f, 0.6f},      // Turquoise
       {0.9f, 0.5f, 1.0f},      // Orchid pink
       {0.0f, 1.0f, 0.5f},      // Spring green
       {1.0f, 0.6f, 0.2f}},     // Tangerine
      // Palette 1: Cyan/Magenta duotone - dramatic contrast
      {{0.0f, 0.85f, 1.0f},     // Cyan primary
       {1.0f, 0.0f, 0.65f},     // Magenta primary
       {0.0f, 0.7f, 0.9f},      // Deep cyan (brightened)
       {0.9f, 0.0f, 0.6f},      // Deep magenta (brightened)
       {0.3f, 0.95f, 1.0f},     // Light cyan
       {1.0f, 0.3f, 0.75f},     // Light magenta
       {0.0f, 0.65f, 0.85f},    // Mid-dark cyan (brightened)
       {0.8f, 0.0f, 0.55f},     // Mid-dark magenta (brightened)
       {0.15f, 0.75f, 0.9f},    // Mid cyan
       {0.9f, 0.15f, 0.6f},     // Mid magenta
       {0.0f, 0.95f, 0.85f},    // Aqua cyan
       {0.95f, 0.0f, 0.55f}},   // Rose magenta
      // Palette 2: Fire/Ice - warm vs cool dramatic
      {{1.0f, 0.3f, 0.0f},      // Fire orange
       {0.0f, 0.6f, 1.0f},      // Ice blue
       {1.0f, 0.1f, 0.1f},      // Hot red
       {0.3f, 0.8f, 0.95f},     // Frost blue
       {1.0f, 0.55f, 0.0f},     // Amber
       {0.0f, 0.5f, 0.9f},      // Deep ice (brightened)
       {0.9f, 0.0f, 0.2f},      // Crimson
       {0.5f, 0.85f, 1.0f},     // Sky ice
       {1.0f, 0.7f, 0.2f},      // Gold fire
       {0.35f, 0.6f, 0.85f},    // Steel blue (brightened)
       {0.85f, 0.2f, 0.0f},     // Burnt red
       {0.4f, 0.7f, 0.9f}},     // Pale ice
      // Palette 3: Tropical Sunset - vibrant warm/cool mix
      {{1.0f, 0.4f, 0.6f},      // Flamingo pink
       {0.0f, 0.8f, 0.7f},      // Teal lagoon
       {1.0f, 0.6f, 0.0f},      // Mango orange
       {0.5f, 0.3f, 0.9f},      // Twilight purple (brightened)
       {1.0f, 0.85f, 0.3f},     // Sunset yellow
       {0.0f, 0.7f, 0.7f},      // Ocean teal (brightened)
       {1.0f, 0.3f, 0.45f},     // Hibiscus
       {0.4f, 0.8f, 0.7f},      // Sea foam (brightened)
       {0.95f, 0.5f, 0.3f},     // Papaya
       {0.6f, 0.4f, 0.85f},     // Orchid purple (brightened)
       {1.0f, 0.75f, 0.5f},     // Peach sunset
       {0.3f, 0.75f, 0.85f}},   // Paradise blue (brightened)
      // Palette 4: Noir Accent - mostly grayscale with pops of color
      {{0.9f, 0.9f, 0.95f},     // Cool white
       {1.0f, 0.15f, 0.3f},     // Accent red
       {0.75f, 0.75f, 0.8f},    // Silver
       {0.0f, 0.7f, 0.9f},      // Accent cyan
       {0.65f, 0.65f, 0.7f},    // Medium gray (brightened)
       {0.95f, 0.7f, 0.0f},     // Accent gold
       {0.55f, 0.55f, 0.6f},    // Soft gray (brightened)
       {0.7f, 0.0f, 0.9f},      // Accent purple (brightened)
       {0.85f, 0.85f, 0.85f},   // Light gray
       {0.0f, 0.9f, 0.5f},      // Accent green
       {0.6f, 0.6f, 0.65f},     // Pewter (brightened)
       {1.0f, 0.5f, 0.0f}}      // Accent orange
  };
  ofColor getTintColorForSurface(int surfaceIndex) const;

  // Calibration grid overlay modes: 0=off, 1=solid (opaque), 2=overlay (transparent)
  enum class GridMode { Off = 0, Solid = 1, Overlay = 2 };
  GridMode gridMode_{GridMode::Off};
  bool showDebugInfo_{true};  // Toggle debug text overlay
  void drawCalibrationGrid();

  // Crosshair overlay for vertex alignment (sent from composer during drag)
  bool crosshairEnabled_{false};
  float crosshairX_{0.0f};  // Normalized -1 to 1
  float crosshairY_{0.0f};  // Normalized -1 to 1
  float crosshairLastUpdateTime_{0.0f};  // Time of last crosshair update
  static constexpr float kCrosshairTimeoutSeconds = 2.0f;  // Auto-hide after 2 seconds
  void drawCrosshair();

  void keyPressed(int key) override;
};
