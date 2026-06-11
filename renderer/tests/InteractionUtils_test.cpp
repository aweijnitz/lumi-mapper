#include "util/InteractionUtils.h"
#include "util/RendererOverlayUtils.h"
#include "util/RendererStatusUtils.h"

#include <catch2/catch_test_macros.hpp>
#include <cmath>

using projection::renderer::computeAverageEnergy;
using projection::renderer::computeCrosshairLabelPosition;
using projection::renderer::cycleGridMode;
using projection::renderer::formatLoadSceneCommand;
using projection::renderer::formatPlayCueCommand;
using projection::renderer::formatSetFeedForSurfaceCommand;
using projection::renderer::mapNormalizedToScreen;
using projection::renderer::mapEnergyToScale;
using projection::renderer::mapMidiValueToBrightness;
using projection::renderer::shouldAutoHideCrosshair;
using projection::renderer::tintPaletteName;
using projection::renderer::CalibrationGridMode;
using projection::renderer::RendererStatusSnapshot;
using projection::renderer::buildDebugOverlayLines;

TEST_CASE("mapMidiValueToBrightness maps CC values to unit range", "[interaction]") {
  auto close = [](float value, float expected) { REQUIRE(std::abs(value - expected) < 1e-5f); };

  close(mapMidiValueToBrightness(0), 0.0f);
  close(mapMidiValueToBrightness(64), 64.0f / 127.0f);
  close(mapMidiValueToBrightness(127), 1.0f);
  close(mapMidiValueToBrightness(200), 1.0f);
  close(mapMidiValueToBrightness(-10), 0.0f);
}

TEST_CASE("computeAverageEnergy averages the requested number of bins", "[interaction]") {
  std::vector<float> magnitudes{1.0f, 3.0f, 5.0f, 7.0f};
  REQUIRE(std::abs(computeAverageEnergy(magnitudes, 2) - 2.0f) < 1e-5f);
  REQUIRE(std::abs(computeAverageEnergy(magnitudes, 10) - 4.0f) < 1e-5f);
  REQUIRE(std::abs(computeAverageEnergy({}, 4) - 0.0f) < 1e-5f);
}

TEST_CASE("mapEnergyToScale clamps to configured range", "[interaction]") {
  const float minScale = 0.8f;
  const float maxScale = 1.2f;
  const float energyForMax = 2.0f;

  REQUIRE(std::abs(mapEnergyToScale(0.0f, minScale, maxScale, energyForMax) - minScale) < 1e-5f);
  REQUIRE(std::abs(mapEnergyToScale(1.0f, minScale, maxScale, energyForMax) - 1.0f) < 1e-5f);
  REQUIRE(std::abs(mapEnergyToScale(5.0f, minScale, maxScale, energyForMax) - maxScale) < 1e-5f);
}

TEST_CASE("renderer status helpers preserve command formatting", "[interaction][renderer]") {
  REQUIRE(formatLoadSceneCommand("cmd-1", "project-a") == "LoadScene (#cmd-1) project project-a");
  REQUIRE(formatSetFeedForSurfaceCommand("cmd-2", "project-b", "surface-4", "feed-7") ==
          "SetFeedForSurface (#cmd-2) project project-b -> surface surface-4 feed feed-7");
  REQUIRE(formatPlayCueCommand("cmd-3", "project-c", "cue-9") ==
          "PlayCue (#cmd-3) project project-c -> cue cue-9");
}

TEST_CASE("renderer status helpers build overlay lines in draw order", "[interaction][renderer]") {
  const RendererStatusSnapshot snapshot{
      "127.0.0.1",
      9000,
      "preview",
      "1.2.3",
      "scene-5",
      "LoadScene (#abc)",
      "socket closed",
  };

  const auto lines = buildDebugOverlayLines(snapshot);
  REQUIRE(lines.size() == 5);
  REQUIRE(lines[0].text == "Renderer connected to: 127.0.0.1:9000");
  REQUIRE(lines[1].text == "Role: preview | Version: 1.2.3");
  REQUIRE(lines[2].text == "Loaded Scene: scene-5");
  REQUIRE(lines[3].text == "Last Command: LoadScene (#abc)");
  REQUIRE(lines[4].text == "Last Error: socket closed");
  REQUIRE(lines[4].isError);
}

TEST_CASE("renderer overlay helpers keep grid cycling and crosshair layout stable", "[interaction][renderer]") {
  REQUIRE(cycleGridMode(CalibrationGridMode::Off) == CalibrationGridMode::Solid);
  REQUIRE(cycleGridMode(CalibrationGridMode::Solid) == CalibrationGridMode::Overlay);
  REQUIRE(cycleGridMode(CalibrationGridMode::Overlay) == CalibrationGridMode::Off);

  const auto center = mapNormalizedToScreen(0.0f, 0.0f, 1920.0f, 1080.0f);
  REQUIRE(center.x == 960.0f);
  REQUIRE(center.y == 540.0f);

  const auto nearTopRight = computeCrosshairLabelPosition(1900.0f, 10.0f, 1920.0f);
  REQUIRE(nearTopRight.x == 1800.0f);
  REQUIRE(nearTopRight.y == 35.0f);

  REQUIRE(shouldAutoHideCrosshair(12.0f, 9.5f, 2.0f));
  REQUIRE(!shouldAutoHideCrosshair(11.0f, 9.5f, 2.0f));
  REQUIRE(std::string(tintPaletteName(6)) == "Cyan/Magenta");
}
