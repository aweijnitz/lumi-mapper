#pragma once

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

#include <ofMain.h>

namespace projection::renderer {

enum class CalibrationGridMode { Off = 0, Solid = 1, Overlay = 2 };

struct OverlayPoint {
  float x{0.0f};
  float y{0.0f};
};

struct CalibrationOverlayState {
  CalibrationGridMode gridMode{CalibrationGridMode::Off};
  bool monochromeEnabled{true};
  bool colorTintEnabled{true};
  bool dramaticModeEnabled{true};
  int tintPaletteIndex{0};
};

inline CalibrationGridMode cycleGridMode(CalibrationGridMode mode) {
  switch (mode) {
    case CalibrationGridMode::Off:
      return CalibrationGridMode::Solid;
    case CalibrationGridMode::Solid:
      return CalibrationGridMode::Overlay;
    case CalibrationGridMode::Overlay:
      return CalibrationGridMode::Off;
  }
  return CalibrationGridMode::Off;
}

inline const char* calibrationGridModeName(CalibrationGridMode mode) {
  switch (mode) {
    case CalibrationGridMode::Off:
      return "off";
    case CalibrationGridMode::Solid:
      return "solid";
    case CalibrationGridMode::Overlay:
      return "overlay";
  }
  return "off";
}

inline const char* tintPaletteName(int tintPaletteIndex) {
  static constexpr const char* kPaletteNames[] = {"Mixed Neon", "Cyan/Magenta", "Fire/Ice", "Tropical", "Noir"};
  constexpr int kPaletteCount = static_cast<int>(sizeof(kPaletteNames) / sizeof(kPaletteNames[0]));
  const int normalizedIndex = ((tintPaletteIndex % kPaletteCount) + kPaletteCount) % kPaletteCount;
  return kPaletteNames[normalizedIndex];
}

inline bool shouldAutoHideCrosshair(float currentTime, float lastUpdateTime, float timeoutSeconds) {
  return currentTime - lastUpdateTime > timeoutSeconds;
}

inline OverlayPoint mapNormalizedToScreen(float normalizedX, float normalizedY, float screenW, float screenH) {
  return {
      (normalizedX * 0.5f + 0.5f) * screenW,
      (normalizedY * 0.5f + 0.5f) * screenH,
  };
}

inline OverlayPoint computeCrosshairLabelPosition(float screenX, float screenY, float screenW) {
  float labelX = screenX + 25.0f;
  float labelY = screenY - 15.0f;

  if (labelX > screenW - 100.0f) {
    labelX = screenX - 100.0f;
  }
  if (labelY < 20.0f) {
    labelY = screenY + 25.0f;
  }

  return {labelX, labelY};
}

inline void drawCalibrationGrid(float screenW, float screenH, const CalibrationOverlayState& state) {
  const bool isOverlay = (state.gridMode == CalibrationGridMode::Overlay);

  const ofColor accentColor(0, 180, 216);
  const ofColor gridColor = isOverlay ? ofColor(80, 80, 80) : ofColor(58, 58, 58);
  const ofColor borderColor = isOverlay ? ofColor(0, 180, 216, 180) : ofColor(0, 180, 216);
  const ofColor centerColor = isOverlay ? ofColor(255, 149, 0, 180) : ofColor(255, 149, 0);
  const ofColor labelColor(200, 200, 200);

  if (!isOverlay) {
    ofSetColor(26, 26, 26, 230);
    ofDrawRectangle(0, 0, screenW, screenH);
  }

  const int majorDivisionsX = 8;
  const int majorDivisionsY = 8;
  const float majorCellW = screenW / static_cast<float>(majorDivisionsX);
  const float majorCellH = screenH / static_cast<float>(majorDivisionsY);

  if (!isOverlay) {
    const int minorSubdivisions = 4;
    const float minorCellW = majorCellW / static_cast<float>(minorSubdivisions);
    const float minorCellH = majorCellH / static_cast<float>(minorSubdivisions);

    ofSetColor(gridColor, 80);
    ofSetLineWidth(1.0f);
    for (float x = 0; x <= screenW; x += minorCellW) {
      ofDrawLine(x, 0, x, screenH);
    }
    for (float y = 0; y <= screenH; y += minorCellH) {
      ofDrawLine(0, y, screenW, y);
    }
  }

  const int majorAlpha = isOverlay ? 60 : 180;
  ofSetColor(gridColor, majorAlpha);
  ofSetLineWidth(isOverlay ? 1.0f : 1.5f);
  for (float x = 0; x <= screenW; x += majorCellW) {
    ofDrawLine(x, 0, x, screenH);
  }
  for (float y = 0; y <= screenH; y += majorCellH) {
    ofDrawLine(0, y, screenW, y);
  }

  const float centerX = screenW / 2.0f;
  const float centerY = screenH / 2.0f;
  const float crosshairSize = std::min(screenW, screenH) * 0.15f;

  ofSetColor(centerColor);
  ofSetLineWidth(2.0f);
  ofDrawLine(centerX - crosshairSize, centerY, centerX + crosshairSize, centerY);
  ofDrawLine(centerX, centerY - crosshairSize, centerX, centerY + crosshairSize);
  ofNoFill();
  ofSetLineWidth(2.0f);
  ofDrawCircle(centerX, centerY, 20);
  ofDrawCircle(centerX, centerY, 40);
  ofFill();

  ofNoFill();
  ofSetColor(borderColor);
  ofSetLineWidth(isOverlay ? 2.0f : 4.0f);
  ofDrawRectangle(2, 2, screenW - 4, screenH - 4);

  const float cornerSize = 60.0f;
  ofSetLineWidth(isOverlay ? 2.0f : 3.0f);
  ofDrawLine(2, 2, 2 + cornerSize, 2);
  ofDrawLine(2, 2, 2, 2 + cornerSize);
  ofDrawLine(screenW - 2, 2, screenW - 2 - cornerSize, 2);
  ofDrawLine(screenW - 2, 2, screenW - 2, 2 + cornerSize);
  ofDrawLine(2, screenH - 2, 2 + cornerSize, screenH - 2);
  ofDrawLine(2, screenH - 2, 2, screenH - 2 - cornerSize);
  ofDrawLine(screenW - 2, screenH - 2, screenW - 2 - cornerSize, screenH - 2);
  ofDrawLine(screenW - 2, screenH - 2, screenW - 2, screenH - 2 - cornerSize);
  ofFill();

  ofSetColor(labelColor);
  const std::string resLabel = std::to_string(static_cast<int>(screenW)) + " x " +
                               std::to_string(static_cast<int>(screenH));
  ofDrawBitmapString("CALIBRATION GRID", 20, screenH - 100);
  ofDrawBitmapString("Resolution: " + resLabel, 20, screenH - 80);
  ofDrawBitmapString("G: Grid | M: Mono | T: Tint | D: Dramatic | P: Palette | I: Info | V: Verbose", 20,
                     screenH - 60);

  std::string status = "Mono: " + std::string(state.monochromeEnabled ? "ON" : "OFF") +
                       " | Tint: " + std::string(state.colorTintEnabled ? "ON" : "OFF") +
                       " | Dramatic: " + std::string(state.dramaticModeEnabled ? "ON" : "OFF");
  ofDrawBitmapString(status, 20, screenH - 40);
  ofDrawBitmapString("Palette: " + std::string(tintPaletteName(state.tintPaletteIndex)), 20, screenH - 20);

  ofSetColor(accentColor);
  ofDrawBitmapString("LUMI MAPPER", centerX - 40, 30);
}

inline void drawCrosshair(float screenW, float screenH, float normalizedX, float normalizedY) {
  const OverlayPoint screenPoint = mapNormalizedToScreen(normalizedX, normalizedY, screenW, screenH);

  const ofColor crosshairColor(0, 180, 216);
  const ofColor coordColor(255, 255, 255, 200);

  ofSetColor(crosshairColor, 180);
  ofSetLineWidth(1.5f);
  ofDrawLine(0, screenPoint.y, screenW, screenPoint.y);
  ofDrawLine(screenPoint.x, 0, screenPoint.x, screenH);

  ofNoFill();
  ofSetColor(crosshairColor);
  ofSetLineWidth(2.0f);
  ofDrawCircle(screenPoint.x, screenPoint.y, 8);
  ofDrawCircle(screenPoint.x, screenPoint.y, 20);
  ofFill();

  ofSetColor(crosshairColor);
  ofDrawCircle(screenPoint.x, screenPoint.y, 3);

  ofSetColor(coordColor);
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(3) << "(" << normalizedX << ", " << normalizedY << ")";

  const OverlayPoint labelPosition = computeCrosshairLabelPosition(screenPoint.x, screenPoint.y, screenW);
  ofDrawBitmapString(oss.str(), labelPosition.x, labelPosition.y);
}

}  // namespace projection::renderer
