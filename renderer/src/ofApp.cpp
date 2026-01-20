#include "ofApp.h"

#include <algorithm>
#include <cmath>
#include <csignal>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>
#include <utility>
#include <vector>

using projection::core::RendererMessageType;

ofApp::ofApp(std::string host, int port, std::string name, int connectRetries, bool verbose, bool enableAudio)
    : client_(*this, std::move(host), port, std::move(name), connectRetries, verbose),
      host_(client_.host()),
      port_(port),
      name_(client_.name()),
      verbose_(verbose),
      audioRequested_(enableAudio) {}

void ofApp::setup() {
  if (verbose_) {
    std::cerr << "[renderer] connecting to server " << host_ << ":" << port_ << " as " << name_ << std::endl;
  }
  client_.start();

  // Load grayscale shader for monochrome filter
  // Set the data path to be relative to the executable directory
  // This ensures shaders are found whether running from build dir or installed location
  std::string exePath = ofFilePath::getEnclosingDirectory(ofFilePath::getCurrentExeDir());
  std::string dataPath = ofFilePath::join(ofFilePath::getCurrentExeDir(), "data");
  ofSetDataPathRoot(dataPath);
  if (verbose_) {
    std::cerr << "[renderer] data path set to: " << dataPath << std::endl;
  }

  grayscaleShaderLoaded_ = grayscaleShader_.load("shaders/grayscale");
  if (verbose_) {
    if (grayscaleShaderLoaded_) {
      std::cerr << "[renderer] grayscale shader loaded successfully" << std::endl;
    } else {
      std::cerr << "[renderer] grayscale shader failed to load; monochrome filter will use fallback" << std::endl;
    }
  }

#if PROJECTION_HAS_OFX_MIDI
  midiIn_.openPort(0);
  midiIn_.addListener(this);
#endif

  if (audioRequested_) {
    const int inputChannels = 1;
    const int outputChannels = 0;
    const int sampleRate = 44100;
    const int bufferSize = 512;
    bool audioInitialized = false;
    try {
      ofSoundStreamSettings soundSettings;
      soundSettings.setInListener(this);
      soundSettings.sampleRate = sampleRate;
      soundSettings.numInputChannels = inputChannels;
      soundSettings.numOutputChannels = outputChannels;
      soundSettings.bufferSize = bufferSize;
      soundSettings.numBuffers = 4;

      audioInitialized = soundStream_.setup(soundSettings);
      if (!audioInitialized && verbose_) {
        std::cerr << "[renderer] audio input failed to initialize; continuing without audio" << std::endl;
      }
    } catch (const std::exception& ex) {
      if (verbose_) {
        std::cerr << "[renderer] audio input initialization error: " << ex.what()
                  << "; continuing without audio" << std::endl;
      }
    }
    audioEnabled_ = audioInitialized;
  } else if (verbose_) {
    std::cerr << "[renderer] audio disabled by configuration" << std::endl;
  }
  if (verbose_) {
    if (audioEnabled_) {
      std::cerr << "[renderer] audio/midi initialized" << std::endl;
    } else {
      std::cerr << "[renderer] audio disabled; continuing without audio input" << std::endl;
    }
  }
}

void ofApp::update() {
  if (!client_.running()) {
    const std::string serverError = client_.lastError();
    if (!serverError.empty()) {
      std::lock_guard<std::mutex> lock(stateMutex_);
      lastError_ = serverError;
    }
    std::raise(SIGTERM);
    return;
  }

  std::vector<projection::core::RendererMessage> messages;
  {
    std::lock_guard<std::mutex> lock(queueMutex_);
    while (!messageQueue_.empty()) {
      messages.push_back(messageQueue_.front());
      messageQueue_.pop();
    }
  }

  for (const auto& message : messages) {
    processMessage(message);
  }

  renderState_.updateVideoPlayers();

  std::vector<float> audioCopy;
  {
    std::lock_guard<std::mutex> lock(audioMutex_);
    audioCopy = audioBuffer_;
  }

  if (!audioCopy.empty()) {
    double energySum = 0.0;
    for (float sample : audioCopy) {
      energySum += static_cast<double>(sample) * static_cast<double>(sample);
    }
    const float averageEnergy = static_cast<float>(energySum / static_cast<double>(audioCopy.size()));

    constexpr float smoothingFactor = 0.9f;
    smoothedEnergy_ = smoothingFactor * smoothedEnergy_ + (1.0f - smoothingFactor) * averageEnergy;
    audioScale_ = projection::renderer::mapEnergyToScale(smoothedEnergy_);
  }
}

void ofApp::draw() {
  std::string lastCommand;
  std::string lastError;
  std::string sceneId;
  std::string role;
  std::string version;
  int serverPort = port_;

  {
    std::lock_guard<std::mutex> lock(stateMutex_);
    lastCommand = lastCommand_;
    lastError = lastError_;
    sceneId = sceneId_;
    role = rendererRole_;
    version = rendererVersion_;
  }

  ofBackground(0, 0, 0);
  ofSetColor(255, 255, 255);

  // Draw loaded feeds (video and images) onto their skewed surfaces using textured meshes.
  const auto& scene = renderState_.currentScene();
  const auto& videoFeeds = renderState_.videoFeeds();
  auto& imageFeeds = renderState_.imageFeeds();  // non-const for pan state updates

  const float screenW = static_cast<float>(ofGetWidth());
  const float screenH = static_cast<float>(ofGetHeight());
  const float currentTime = ofGetElapsedTimef();

  // Surface index counter for unique color assignment per surface
  int surfaceIndex = 0;

  // Monochrome filter: render to FBO, then convert to grayscale with tint
  if (!monoFbo_.isAllocated() || monoFbo_.getWidth() != screenW || monoFbo_.getHeight() != screenH) {
    monoFbo_.allocate(static_cast<int>(screenW), static_cast<int>(screenH), GL_RGBA);
  }

  monoFbo_.begin();
  ofClear(0, 0, 0, 255);

  ofPushMatrix();
  ofTranslate(screenW / 2.0f, screenH / 2.0f);
  ofScale(audioScale_, audioScale_);
  ofTranslate(-screenW / 2.0f, -screenH / 2.0f);

  // Debug: log available feeds once per scene load
  static std::string lastSceneId;
  if (scene.getId().value != lastSceneId) {
    lastSceneId = scene.getId().value;
    std::cerr << "[renderer] Scene loaded: " << scene.getId().value
              << " with " << scene.getSurfaces().size() << " surfaces, "
              << videoFeeds.size() << " video feeds, "
              << imageFeeds.size() << " image feeds" << std::endl;
    for (const auto& s : scene.getSurfaces()) {
      std::cerr << "[renderer]   Surface " << s.getId().value << " -> feedId=" << s.getFeedId().value << std::endl;
    }
    for (const auto& [id, _] : imageFeeds) {
      std::cerr << "[renderer]   ImageFeed available: " << id << std::endl;
    }
  }

  for (const auto& surface : scene.getSurfaces()) {
    // Check if this surface uses a video feed or image feed
    auto videoFeedIt = videoFeeds.find(surface.getFeedId().value);
    auto imageFeedIt = imageFeeds.find(surface.getFeedId().value);

    const bool hasVideoFeed = (videoFeedIt != videoFeeds.end());
    const bool hasImageFeed = (imageFeedIt != imageFeeds.end());

    if (!hasVideoFeed && !hasImageFeed) {
      surfaceIndex++;  // Still increment to maintain consistent coloring
      continue;
    }

    // Each surface gets its own unique color from the palette
    const int currentSurfaceIndex = surfaceIndex++;

    // Get texture dimensions based on feed type
    float feedW = 0.0f;
    float feedH = 0.0f;
    const ofTexture* texturePtr = nullptr;

    if (hasVideoFeed) {
      auto& player = videoFeedIt->second.player;
      if (!player.isLoaded() || player.getTexture().getTextureData().textureID == 0) {
        continue;
      }
      feedW = player.getWidth();
      feedH = player.getHeight();
      texturePtr = &player.getTexture();
    } else if (hasImageFeed) {
      auto& imageResource = imageFeedIt->second;
      if (!imageResource.image.isAllocated()) {
        continue;
      }
      feedW = static_cast<float>(imageResource.image.getWidth());
      feedH = static_cast<float>(imageResource.image.getHeight());
      texturePtr = &imageResource.image.getTexture();
    }

    if (feedW <= 0.0f || feedH <= 0.0f || texturePtr == nullptr || !texturePtr->isAllocated()) {
      continue;
    }

    // Get vertices - for ellipse surfaces, generate from center/radii
    std::vector<projection::core::Vec2> vertices;
    if (surface.isEllipse()) {
      vertices = surface.generateEllipseVertices(32);
    } else {
      vertices = surface.getVertices();
    }
    if (vertices.size() < 3) {
      continue;
    }

    std::vector<glm::vec2> screenVerts;
    screenVerts.reserve(vertices.size());
    for (const auto& v : vertices) {
      // Scene coordinates are normalized -1..1; map to screen pixels.
      float x = (v.x * 0.5f + 0.5f) * screenW;
      float y = (v.y * 0.5f + 0.5f) * screenH;
      screenVerts.emplace_back(x, y);
    }

    float minX = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::lowest();
    float minY = std::numeric_limits<float>::max();
    float maxY = std::numeric_limits<float>::lowest();
    for (const auto& v : screenVerts) {
      minX = std::min(minX, v.x);
      maxX = std::max(maxX, v.x);
      minY = std::min(minY, v.y);
      maxY = std::max(maxY, v.y);
    }

    if (maxX <= minX || maxY <= minY) {
      continue;
    }

    // Get rotation angle for this surface (in degrees)
    // Negate to match the UI handle direction (clockwise drag = clockwise video rotation)
    const float rotationDeg = -surface.getRotation();
    const float rotationRad = rotationDeg * static_cast<float>(M_PI) / 180.0f;
    const float cosR = std::cos(rotationRad);
    const float sinR = std::sin(rotationRad);

    // Calculate pan offset for image feeds
    float panOffsetU = 0.0f;
    float visiblePortionW = feedW;  // How much of the image width is visible

    if (hasImageFeed) {
      auto& imageResource = imageFeedIt->second;

      // Calculate visible portion width
      visiblePortionW = feedW * imageResource.visiblePortion;

      // Calculate maximum pan distance (remaining width after visible portion)
      const float panRange = feedW - visiblePortionW;

      if (panRange > 0.0f && imageResource.panDurationSeconds > 0.0f) {
        // Calculate elapsed time since pan started
        float elapsed = currentTime - imageResource.panStartTime;

        // Calculate normalized pan position (0 to 1)
        float panProgress = std::fmod(elapsed, imageResource.panDurationSeconds) / imageResource.panDurationSeconds;

        // Apply direction
        switch (imageResource.panDirection) {
          case projection::core::PanDirection::LeftToRight:
            panOffsetU = panProgress * panRange;
            break;
          case projection::core::PanDirection::RightToLeft:
            panOffsetU = (1.0f - panProgress) * panRange;
            break;
          case projection::core::PanDirection::PingPong: {
            // Ping-pong: 0->1->0 over the duration
            // Use a triangle wave: 2 * |progress - 0.5| gives 1->0->1, invert for 0->1->0
            float triangleWave = 1.0f - 2.0f * std::abs(panProgress - 0.5f);
            panOffsetU = triangleWave * panRange;
            break;
          }
        }
      }
    }

    // Calculate center of texture region for rotation pivot
    // For images with pan, adjust the center based on the visible portion
    const float centerU = hasImageFeed ? (panOffsetU + visiblePortionW * 0.5f) : (feedW * 0.5f);
    const float centerV = feedH * 0.5f;

    ofMesh mesh;
    mesh.setMode(OF_PRIMITIVE_TRIANGLE_FAN);
    for (const auto& v : screenVerts) {
      // Map screen position to texture coordinates
      float u, t;

      if (hasImageFeed) {
        // For images: map to the visible portion with pan offset
        u = ofMap(v.x, minX, maxX, panOffsetU, panOffsetU + visiblePortionW, true);
        t = ofMap(v.y, minY, maxY, 0.0f, feedH, true);
      } else {
        // For video: map to full texture
        u = ofMap(v.x, minX, maxX, 0.0f, feedW, true);
        t = ofMap(v.y, minY, maxY, 0.0f, feedH, true);
      }

      // Apply rotation around texture center
      if (std::abs(rotationDeg) > 0.01f) {
        float du = u - centerU;
        float dv = t - centerV;
        u = centerU + du * cosR - dv * sinR;
        t = centerV + du * sinR + dv * cosR;
      }

      mesh.addVertex(glm::vec3(v.x, v.y, 0.0f));
      mesh.addTexCoord(glm::vec2(u, t));
    }

    const float alpha = std::clamp(surface.getOpacity() * midiBrightness_, 0.0f, 1.0f);
    const float brightness = std::clamp(surface.getBrightness(), 0.0f, 1.0f);
    const int alphaValue = static_cast<int>(std::round(alpha * 255.0f));

    // Apply filter based on scene settings (or fall back to renderer defaults)
    const auto& sceneSettings = scene.getSettings();
    const bool useColorTint = sceneSettings.filter == projection::core::SceneFilter::ColorTint ||
                              (sceneSettings.filter == projection::core::SceneFilter::None && colorTintEnabled_ && monochromeEnabled_);
    const bool useMonochrome = sceneSettings.filter == projection::core::SceneFilter::Monochrome ||
                               (sceneSettings.filter == projection::core::SceneFilter::None && monochromeEnabled_ && !colorTintEnabled_);

    const ofTexture& texture = *texturePtr;

    if (useColorTint) {
      // Apply tint color per-surface: each surface gets a unique color from the palette
      const int paletteIdx = (sceneSettings.filter == projection::core::SceneFilter::ColorTint ?
                              sceneSettings.colorPaletteIndex : tintPaletteIndex_) % kNumPalettes;
      const int colorIdx = currentSurfaceIndex % kColorsPerPalette;
      const TintColor& tint = kTintPalettes[paletteIdx][colorIdx];
      const int r = static_cast<int>(std::round(brightness * tint.r * 255.0f));
      const int g = static_cast<int>(std::round(brightness * tint.g * 255.0f));
      const int b = static_cast<int>(std::round(brightness * tint.b * 255.0f));
      ofSetColor(r, g, b, alphaValue);
      texture.bind();
      mesh.draw();
      texture.unbind();
    } else if (useMonochrome && grayscaleShaderLoaded_) {
      // Monochrome filter using GLSL shader for true grayscale conversion
      ofSetColor(255, 255, 255);  // Shader handles all color manipulation
      grayscaleShader_.begin();
      grayscaleShader_.setUniformTexture("tex0", texture, 0);
      grayscaleShader_.setUniform1f("brightness", brightness);
      grayscaleShader_.setUniform1f("alpha", alpha);
      texture.bind();
      mesh.draw();
      texture.unbind();
      grayscaleShader_.end();
    } else if (useMonochrome) {
      // Fallback monochrome without shader - use gray tint (less accurate)
      const int grayValue = static_cast<int>(std::round(brightness * 200.0f));
      ofSetColor(grayValue, grayValue, grayValue, alphaValue);
      texture.bind();
      mesh.draw();
      texture.unbind();
    } else {
      // No filter - render in full color (white tint preserves original colors)
      // Brightness is applied uniformly to all channels
      const int brightnessValue = static_cast<int>(std::round(brightness * 255.0f));
      ofSetColor(brightnessValue, brightnessValue, brightnessValue, alphaValue);
      texture.bind();
      mesh.draw();
      texture.unbind();
    }
  }

  ofPopMatrix();
  monoFbo_.end();

  // Draw FBO directly - monochrome/dramatic effects are now applied during
  // surface rendering via tint colors for better GPU performance.
  // CPU-based pixel processing was removed as it caused significant performance issues.
  ofSetColor(255, 255, 255);
  monoFbo_.draw(0, 0);

  ofSetColor(255, 255, 255);

  // Draw debug info overlay if enabled
  if (showDebugInfo_) {
    ofDrawBitmapString("Renderer connected to: " + host_ + ":" + std::to_string(serverPort), 20, 20);
    if (!role.empty()) {
      ofDrawBitmapString("Role: " + role + " | Version: " + version, 20, 40);
    }
    if (!sceneId.empty()) {
      ofDrawBitmapString("Loaded Scene: " + sceneId, 20, 60);
    }
    if (!lastCommand.empty()) {
      ofDrawBitmapString("Last Command: " + lastCommand, 20, 80);
    }
    if (!lastError.empty()) {
      ofSetColor(255, 0, 0);
      ofDrawBitmapString("Last Error: " + lastError, 20, 100);
    }
  }

  // Draw calibration grid overlay if enabled
  if (gridMode_ != GridMode::Off) {
    drawCalibrationGrid();
  }

  // Draw crosshair overlay if enabled (from composer vertex drag)
  // Auto-hide crosshair after timeout to handle cases where disable message is lost
  if (crosshairEnabled_) {
    const float timeSinceUpdate = ofGetElapsedTimef() - crosshairLastUpdateTime_;
    if (timeSinceUpdate > kCrosshairTimeoutSeconds) {
      crosshairEnabled_ = false;
      if (verbose_) {
        std::cerr << "[renderer] crosshair auto-hidden after timeout" << std::endl;
      }
    } else {
      drawCrosshair();
    }
  }
}

void ofApp::audioIn(ofSoundBuffer& input) {
  const int channels = std::max<int>(1, input.getNumChannels());
  std::vector<float> mono;
  mono.reserve(static_cast<size_t>(input.getNumFrames()));
  for (int i = 0; i < input.getNumFrames(); ++i) {
    mono.push_back(input[static_cast<size_t>(i * channels)]);
  }

  std::lock_guard<std::mutex> lock(audioMutex_);
  audioBuffer_ = std::move(mono);
}

#if PROJECTION_HAS_OFX_MIDI
void ofApp::newMidiMessage(ofxMidiMessage& msg) {
  if (msg.status == ofxMidiMessage::MIDI_CONTROL_CHANGE && msg.control == 1) {
    midiBrightness_ = projection::renderer::mapMidiValueToBrightness(msg.value);
  }
}
#endif

void ofApp::exit() {
  if (audioEnabled_) {
    soundStream_.stop();
  }
#if PROJECTION_HAS_OFX_MIDI
  midiIn_.closePort();
#endif
  client_.stop();
}

void ofApp::handle(const projection::core::RendererMessage& message) {
  std::lock_guard<std::mutex> lock(queueMutex_);
  messageQueue_.push(message);
}

void ofApp::processMessage(const projection::core::RendererMessage& message) {
  {
    std::lock_guard<std::mutex> lock(stateMutex_);
    lastError_.clear();
  }

  switch (message.type) {
    case RendererMessageType::Hello:
      {
        std::lock_guard<std::mutex> lock(stateMutex_);
        updateStatusForHello(*message.hello, message.commandId);
      }
      break;
    case RendererMessageType::LoadScene:
      {
        std::lock_guard<std::mutex> lock(stateMutex_);
        updateStatusForLoadScene(*message.loadScene, message.commandId);
      }
      break;
    case RendererMessageType::LoadSceneDefinition:
      if (verbose_) {
        std::cerr << "[renderer] LoadSceneDefinition with scene " << message.loadSceneDefinition->scene.getId().value
                  << " feeds=" << message.loadSceneDefinition->feeds.size() << std::endl;
      }
      renderState_.loadSceneDefinition(message.loadSceneDefinition->scene, message.loadSceneDefinition->feeds);
      {
        std::lock_guard<std::mutex> lock(stateMutex_);
        sceneId_ = message.loadSceneDefinition->scene.getId().value;
        lastCommand_ = "LoadSceneDefinition (#" + message.commandId + ")";
      }
      break;
    case RendererMessageType::SetFeedForSurface:
      {
        std::lock_guard<std::mutex> lock(stateMutex_);
        updateStatusForSetFeed(*message.setFeedForSurface, message.commandId);
      }
      break;
    case RendererMessageType::PlayCue:
      {
        std::lock_guard<std::mutex> lock(stateMutex_);
        updateStatusForPlayCue(*message.playCue, message.commandId);
      }
      break;
    case RendererMessageType::ShowTestPattern:
      if (message.showTestPattern) {
        // Remote toggle uses Overlay mode (transparent over video) when enabled
        gridMode_ = message.showTestPattern->enabled ? GridMode::Overlay : GridMode::Off;
        if (verbose_) {
          std::cerr << "[renderer] output grid " << (message.showTestPattern->enabled ? "enabled (overlay)" : "disabled")
                    << " via remote command" << std::endl;
        }
        {
          std::lock_guard<std::mutex> lock(stateMutex_);
          lastCommand_ = "ShowTestPattern (#" + message.commandId + ")";
        }
      }
      break;
    case RendererMessageType::ShowCrosshair:
      if (message.showCrosshair) {
        crosshairEnabled_ = message.showCrosshair->enabled;
        crosshairX_ = message.showCrosshair->x;
        crosshairY_ = message.showCrosshair->y;
        crosshairLastUpdateTime_ = ofGetElapsedTimef();
        if (verbose_) {
          std::cerr << "[renderer] crosshair " << (crosshairEnabled_ ? "enabled" : "disabled")
                    << " at (" << crosshairX_ << ", " << crosshairY_ << ")" << std::endl;
        }
      }
      break;
    case RendererMessageType::Ack:
    case RendererMessageType::Error:
      // Renderer should not receive these in normal operation, ignore.
      break;
  }
}

void ofApp::updateStatusForHello(const projection::core::HelloMessage& hello, const std::string& commandId) {
  rendererRole_ = hello.role;
  rendererVersion_ = hello.version;
  lastCommand_ = "Hello (#" + commandId + ")";
}

void ofApp::updateStatusForLoadScene(const projection::core::LoadSceneMessage& loadScene,
                                     const std::string& commandId) {
  sceneId_ = loadScene.sceneId.value;
  lastCommand_ = "LoadScene (#" + commandId + ") project " + loadScene.projectId.value;
}

void ofApp::updateStatusForSetFeed(const projection::core::SetFeedForSurfaceMessage& setFeed,
                                   const std::string& commandId) {
  lastCommand_ = "SetFeedForSurface (#" + commandId + ") project " + setFeed.projectId.value + " -> surface " +
                 setFeed.surfaceId.value + " feed " + setFeed.feedId.value;
}

void ofApp::updateStatusForPlayCue(const projection::core::PlayCueMessage& playCue, const std::string& commandId) {
  lastCommand_ =
      "PlayCue (#" + commandId + ") project " + playCue.projectId.value + " -> cue " + playCue.cueId.value;
}

void ofApp::keyPressed(int key) {
  if (key == 'g' || key == 'G') {
    // Cycle through grid modes: Off -> Solid -> Overlay -> Off
    switch (gridMode_) {
      case GridMode::Off:
        gridMode_ = GridMode::Solid;
        break;
      case GridMode::Solid:
        gridMode_ = GridMode::Overlay;
        break;
      case GridMode::Overlay:
        gridMode_ = GridMode::Off;
        break;
    }
    if (verbose_) {
      const char* modeNames[] = {"off", "solid", "overlay"};
      std::cerr << "[renderer] calibration grid: " << modeNames[static_cast<int>(gridMode_)] << std::endl;
    }
  } else if (key == 'm' || key == 'M') {
    monochromeEnabled_ = !monochromeEnabled_;
    if (verbose_) {
      std::cerr << "[renderer] monochrome filter " << (monochromeEnabled_ ? "enabled" : "disabled") << std::endl;
    }
  } else if (key == 't' || key == 'T') {
    colorTintEnabled_ = !colorTintEnabled_;
    if (verbose_) {
      std::cerr << "[renderer] color tint " << (colorTintEnabled_ ? "enabled" : "disabled") << std::endl;
    }
  } else if (key == 'd' || key == 'D') {
    dramaticModeEnabled_ = !dramaticModeEnabled_;
    if (verbose_) {
      std::cerr << "[renderer] dramatic mode " << (dramaticModeEnabled_ ? "enabled" : "disabled") << std::endl;
    }
  } else if (key == 'p' || key == 'P') {
    tintPaletteIndex_ = (tintPaletteIndex_ + 1) % kNumPalettes;
    if (verbose_) {
      const char* paletteNames[] = {"Mixed Neon", "Cyan/Magenta", "Fire/Ice", "Tropical", "Noir"};
      std::cerr << "[renderer] switched to palette: " << paletteNames[tintPaletteIndex_] << std::endl;
    }
  } else if (key == 'v' || key == 'V') {
    verbose_ = !verbose_;
    std::cerr << "[renderer] verbose mode " << (verbose_ ? "enabled" : "disabled") << std::endl;
  } else if (key == 'i' || key == 'I') {
    showDebugInfo_ = !showDebugInfo_;
    if (verbose_) {
      std::cerr << "[renderer] debug info " << (showDebugInfo_ ? "enabled" : "disabled") << std::endl;
    }
  }
}

ofColor ofApp::getTintColorForSurface(int surfaceIndex) const {
  const int paletteIdx = tintPaletteIndex_ % kNumPalettes;
  const int colorIdx = surfaceIndex % kColorsPerPalette;
  const TintColor& tint = kTintPalettes[paletteIdx][colorIdx];
  return ofColor(
      static_cast<int>(tint.r * 255.0f),
      static_cast<int>(tint.g * 255.0f),
      static_cast<int>(tint.b * 255.0f));
}

// Note: applyDramaticFilter was removed for performance reasons.
// The dramatic/tint effects are now applied during surface rendering
// via per-surface tint colors, which is much more GPU-efficient than
// CPU-based per-pixel processing.

void ofApp::drawCalibrationGrid() {
  const float screenW = static_cast<float>(ofGetWidth());
  const float screenH = static_cast<float>(ofGetHeight());

  const bool isOverlay = (gridMode_ == GridMode::Overlay);

  // Theme colors - more subtle in overlay mode to not overpower video
  const ofColor accentColor(0, 180, 216);      // #00b4d8 - cyan accent
  const ofColor gridColor = isOverlay ? ofColor(80, 80, 80) : ofColor(58, 58, 58);
  const ofColor borderColor = isOverlay ? ofColor(0, 180, 216, 180) : ofColor(0, 180, 216);
  const ofColor centerColor = isOverlay ? ofColor(255, 149, 0, 180) : ofColor(255, 149, 0);
  const ofColor labelColor(200, 200, 200);     // #c8c8c8 - light text

  // Draw dark background only in Solid mode
  if (!isOverlay) {
    ofSetColor(26, 26, 26, 230);  // #1a1a1a with alpha
    ofDrawRectangle(0, 0, screenW, screenH);
  }

  // Major grid divisions (8x8 for a nice grid)
  const int majorDivisionsX = 8;
  const int majorDivisionsY = 8;
  const float majorCellW = screenW / static_cast<float>(majorDivisionsX);
  const float majorCellH = screenH / static_cast<float>(majorDivisionsY);

  // In overlay mode, only draw major grid lines (less clutter over video)
  // In solid mode, also draw minor subdivisions
  if (!isOverlay) {
    // Minor grid divisions (subdivide each major cell into 4)
    const int minorSubdivisions = 4;
    const float minorCellW = majorCellW / static_cast<float>(minorSubdivisions);
    const float minorCellH = majorCellH / static_cast<float>(minorSubdivisions);

    // Draw minor grid lines (subtle)
    ofSetColor(gridColor, 80);
    ofSetLineWidth(1.0f);
    for (float x = 0; x <= screenW; x += minorCellW) {
      ofDrawLine(x, 0, x, screenH);
    }
    for (float y = 0; y <= screenH; y += minorCellH) {
      ofDrawLine(0, y, screenW, y);
    }
  }

  // Draw major grid lines - subtle in overlay mode
  const int majorAlpha = isOverlay ? 60 : 180;
  ofSetColor(gridColor, majorAlpha);
  ofSetLineWidth(isOverlay ? 1.0f : 1.5f);
  for (float x = 0; x <= screenW; x += majorCellW) {
    ofDrawLine(x, 0, x, screenH);
  }
  for (float y = 0; y <= screenH; y += majorCellH) {
    ofDrawLine(0, y, screenW, y);
  }

  // Draw center crosshair
  const float centerX = screenW / 2.0f;
  const float centerY = screenH / 2.0f;
  const float crosshairSize = std::min(screenW, screenH) * 0.15f;

  ofSetColor(centerColor);
  ofSetLineWidth(2.0f);
  // Horizontal line
  ofDrawLine(centerX - crosshairSize, centerY, centerX + crosshairSize, centerY);
  // Vertical line
  ofDrawLine(centerX, centerY - crosshairSize, centerX, centerY + crosshairSize);
  // Center circle
  ofNoFill();
  ofSetLineWidth(2.0f);
  ofDrawCircle(centerX, centerY, 20);
  ofDrawCircle(centerX, centerY, 40);
  ofFill();

  // Draw border frame (cyan accent)
  ofNoFill();
  ofSetColor(borderColor);
  ofSetLineWidth(isOverlay ? 2.0f : 4.0f);
  ofDrawRectangle(2, 2, screenW - 4, screenH - 4);

  // Draw corner markers
  const float cornerSize = 60.0f;
  ofSetLineWidth(isOverlay ? 2.0f : 3.0f);

  // Top-left corner
  ofDrawLine(2, 2, 2 + cornerSize, 2);
  ofDrawLine(2, 2, 2, 2 + cornerSize);

  // Top-right corner
  ofDrawLine(screenW - 2, 2, screenW - 2 - cornerSize, 2);
  ofDrawLine(screenW - 2, 2, screenW - 2, 2 + cornerSize);

  // Bottom-left corner
  ofDrawLine(2, screenH - 2, 2 + cornerSize, screenH - 2);
  ofDrawLine(2, screenH - 2, 2, screenH - 2 - cornerSize);

  // Bottom-right corner
  ofDrawLine(screenW - 2, screenH - 2, screenW - 2 - cornerSize, screenH - 2);
  ofDrawLine(screenW - 2, screenH - 2, screenW - 2, screenH - 2 - cornerSize);

  ofFill();

  // Draw resolution label
  ofSetColor(labelColor);
  const std::string resLabel = std::to_string(static_cast<int>(screenW)) + " x " +
                               std::to_string(static_cast<int>(screenH));
  ofDrawBitmapString("CALIBRATION GRID", 20, screenH - 100);
  ofDrawBitmapString("Resolution: " + resLabel, 20, screenH - 80);
  ofDrawBitmapString("G: Grid | M: Mono | T: Tint | D: Dramatic | P: Palette | I: Info | V: Verbose", 20, screenH - 60);

  // Show current mode status
  const char* paletteNames[] = {"Mixed Neon", "Cyan/Magenta", "Fire/Ice", "Tropical", "Noir"};
  std::string status = "Mono: " + std::string(monochromeEnabled_ ? "ON" : "OFF") +
                       " | Tint: " + std::string(colorTintEnabled_ ? "ON" : "OFF") +
                       " | Dramatic: " + std::string(dramaticModeEnabled_ ? "ON" : "OFF");
  ofDrawBitmapString(status, 20, screenH - 40);
  ofDrawBitmapString("Palette: " + std::string(paletteNames[tintPaletteIndex_ % kNumPalettes]), 20, screenH - 20);

  // Draw "LUMI MAPPER" title at top
  ofSetColor(accentColor);
  ofDrawBitmapString("LUMI MAPPER", centerX - 40, 30);
}

void ofApp::drawCrosshair() {
  const float screenW = static_cast<float>(ofGetWidth());
  const float screenH = static_cast<float>(ofGetHeight());

  // Convert normalized coordinates (-1 to 1) to screen coordinates
  const float screenX = (crosshairX_ * 0.5f + 0.5f) * screenW;
  const float screenY = (crosshairY_ * 0.5f + 0.5f) * screenH;

  // Crosshair colors - high visibility
  const ofColor crosshairColor(0, 180, 216);     // Cyan accent
  const ofColor coordColor(255, 255, 255, 200);  // White text

  // Draw full-screen crosshair lines (dashed appearance via segments)
  ofSetColor(crosshairColor, 180);
  ofSetLineWidth(1.5f);

  // Draw horizontal line
  ofDrawLine(0, screenY, screenW, screenY);

  // Draw vertical line
  ofDrawLine(screenX, 0, screenX, screenH);

  // Draw center indicator - concentric circles
  ofNoFill();
  ofSetColor(crosshairColor);
  ofSetLineWidth(2.0f);
  ofDrawCircle(screenX, screenY, 8);
  ofDrawCircle(screenX, screenY, 20);
  ofFill();

  // Draw small filled center dot
  ofSetColor(crosshairColor);
  ofDrawCircle(screenX, screenY, 3);

  // Draw coordinate label
  ofSetColor(coordColor);
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(3) << "(" << crosshairX_ << ", " << crosshairY_ << ")";

  // Position label to avoid edge clipping
  float labelX = screenX + 25;
  float labelY = screenY - 15;

  // Adjust if too close to edges
  if (labelX > screenW - 100) {
    labelX = screenX - 100;
  }
  if (labelY < 20) {
    labelY = screenY + 25;
  }

  ofDrawBitmapString(oss.str(), labelX, labelY);
}
