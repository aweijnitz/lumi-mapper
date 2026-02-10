#include "projection/core/Serialization.h"

#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using nlohmann::json;

namespace projection::core {
namespace {

template <typename T>
const T& requireField(const json& j, const std::string& key) {
  if (!j.contains(key)) {
    throw std::runtime_error("Missing required field: " + key);
  }
  return j.at(key);
}

float requireNumber(const json& j, const std::string& key) {
  const auto& value = requireField<json>(j, key);
  if (!value.is_number()) {
    throw std::runtime_error("Field '" + key + "' must be a number");
  }
  return value.get<float>();
}

int requireInteger(const json& j, const std::string& key) {
  const auto& value = requireField<json>(j, key);
  if (!value.is_number_integer()) {
    throw std::runtime_error("Field '" + key + "' must be an integer");
  }
  return value.get<int>();
}

std::string requireString(const json& j, const std::string& key) {
  const auto& value = requireField<json>(j, key);
  if (!value.is_string()) {
    throw std::runtime_error("Field '" + key + "' must be a string");
  }
  return value.get<std::string>();
}

AssetType parseAssetTypeString(const std::string& raw) {
  AssetType type{};
  if (!fromString(raw, type)) {
    throw std::runtime_error("Invalid AssetType: " + raw);
  }
  return type;
}

BlendMode parseBlendModeString(const std::string& raw) {
  BlendMode mode{};
  if (!fromString(raw, mode)) {
    throw std::runtime_error("Invalid BlendMode: " + raw);
  }
  return mode;
}

std::map<SurfaceId, float> readSurfaceValueArray(const json& array, const std::string& field) {
  if (!array.is_array()) {
    throw std::runtime_error("Field '" + field + "' must be an array");
  }
  std::map<SurfaceId, float> result;
  for (const auto& entry : array) {
    if (!entry.is_object()) {
      throw std::runtime_error("Entries in '" + field + "' must be objects");
    }
    auto surfaceId = requireString(entry, "surfaceId");
    const auto value = requireNumber(entry, "value");
    result.emplace(SurfaceId(surfaceId), value);
  }
  return result;
}

json surfaceValueArray(const std::map<SurfaceId, float>& values) {
  json arr = json::array();
  for (const auto& [id, value] : values) {
    arr.push_back({{"surfaceId", id.value}, {"value", value}});
  }
  return arr;
}

std::vector<std::string> readStringArray(const json& array, const std::string& field) {
  if (!array.is_array()) {
    throw std::runtime_error("Field '" + field + "' must be an array");
  }
  std::vector<std::string> values;
  values.reserve(array.size());
  for (const auto& entry : array) {
    if (!entry.is_string()) {
      throw std::runtime_error("Entries in '" + field + "' must be strings");
    }
    values.emplace_back(entry.get<std::string>());
  }
  return values;
}

}  // namespace

void to_json(json& j, const AssetType& type) { j = toString(type); }

void from_json(const json& j, AssetType& type) {
  if (!j.is_string()) {
    throw std::runtime_error("AssetType must be a string");
  }
  type = parseAssetTypeString(j.get<std::string>());
}

void to_json(json& j, const BlendMode& mode) { j = toString(mode); }

void from_json(const json& j, BlendMode& mode) {
  if (!j.is_string()) {
    throw std::runtime_error("BlendMode must be a string");
  }
  mode = parseBlendModeString(j.get<std::string>());
}

void to_json(json& j, const AssetVariant& variant) {
  j = json{{"path", variant.path}, {"note", variant.note}};
}

void from_json(const json& j, AssetVariant& variant) {
  if (!j.is_object()) {
    throw std::runtime_error("AssetVariant must be an object");
  }
  variant.path = requireString(j, "path");
  if (j.contains("note") && j["note"].is_string()) {
    variant.note = j["note"].get<std::string>();
  } else {
    variant.note.clear();
  }
}

void to_json(json& j, const Asset& asset) {
  j = json{{"id", asset.getId().value},
           {"name", asset.getName()},
           {"type", asset.getType()},
           {"path", asset.getPath()},
           {"variants", asset.getVariants()}};
}

void from_json(const json& j, Asset& asset) {
  if (!j.is_object()) {
    throw std::runtime_error("Asset must be an object");
  }
  const auto id = requireString(j, "id");
  const auto name = requireString(j, "name");
  const auto typeStr = requireString(j, "type");
  const auto path = requireString(j, "path");

  std::vector<AssetVariant> variants;
  if (j.contains("variants")) {
    const auto& variantsJson = j.at("variants");
    if (!variantsJson.is_array()) {
      throw std::runtime_error("Field 'variants' must be an array");
    }
    variants = variantsJson.get<std::vector<AssetVariant>>();
  }

  asset = Asset(AssetId{id}, name, parseAssetTypeString(typeStr), path, variants);
}

void to_json(json& j, const FeedSettings& settings) {
  j = json{{"variantPath", settings.variantPath},
           {"monochrome", settings.monochrome},
           {"panDirection", toString(settings.panDirection)},
           {"panDurationSeconds", settings.panDurationSeconds},
           {"visiblePortion", settings.visiblePortion}};
}

void from_json(const json& j, FeedSettings& settings) {
  if (!j.is_object()) {
    throw std::runtime_error("FeedSettings must be an object");
  }
  settings = FeedSettings{};
  if (j.contains("variantPath") && j["variantPath"].is_string()) {
    settings.variantPath = j["variantPath"].get<std::string>();
  }
  if (j.contains("monochrome") && j["monochrome"].is_boolean()) {
    settings.monochrome = j["monochrome"].get<bool>();
  }
  if (j.contains("panDirection") && j["panDirection"].is_string()) {
    PanDirection dir{};
    if (fromString(j["panDirection"].get<std::string>(), dir)) {
      settings.panDirection = dir;
    }
  }
  if (j.contains("panDurationSeconds") && j["panDurationSeconds"].is_number()) {
    settings.panDurationSeconds = j["panDurationSeconds"].get<float>();
  }
  if (j.contains("visiblePortion") && j["visiblePortion"].is_number()) {
    settings.visiblePortion = j["visiblePortion"].get<float>();
  }
}

void to_json(json& j, const Vec2& vec) { j = json{{"x", vec.x}, {"y", vec.y}}; }

void from_json(const json& j, Vec2& vec) {
  if (!j.is_object()) {
    throw std::runtime_error("Vec2 must be an object");
  }
  vec.x = requireNumber(j, "x");
  vec.y = requireNumber(j, "y");
}

void to_json(json& j, const Feed& feed) {
  j = json{{"projectId", feed.getProjectId().value},
           {"id", feed.getId().value},
           {"name", feed.getName()},
           {"assetId", feed.getAssetId().value},
           {"settings", feed.getSettings()}};
}

void from_json(const json& j, Feed& feed) {
  if (!j.is_object()) {
    throw std::runtime_error("Feed must be an object");
  }
  const auto projectId = requireString(j, "projectId");
  const auto id = requireString(j, "id");
  const auto name = requireString(j, "name");
  const auto assetId = requireString(j, "assetId");

  FeedSettings settings;
  if (j.contains("settings")) {
    from_json(j.at("settings"), settings);
  }

  feed = Feed(ProjectId{projectId}, FeedId{id}, name, AssetId{assetId}, settings);
}

void to_json(json& j, const Surface& surface) {
  j = json{{"id", surface.getId().value},
           {"name", surface.getName()},
           {"feedId", surface.getFeedId().value},
           {"opacity", surface.getOpacity()},
           {"brightness", surface.getBrightness()},
           {"blendMode", surface.getBlendMode()},
           {"zOrder", surface.getZOrder()},
           {"rotation", surface.getRotation()}};

  if (surface.isEllipse()) {
    j["surfaceType"] = "ellipse";
    j["center"] = surface.getCenter();
    j["radiusX"] = surface.getRadiusX();
    j["radiusY"] = surface.getRadiusY();
  } else {
    j["surfaceType"] = "polygon";
    j["vertices"] = surface.getVertices();
  }
}

void from_json(const json& j, Surface& surface) {
  if (!j.is_object()) {
    throw std::runtime_error("Surface must be an object");
  }

  const auto id = requireString(j, "id");
  const auto name = requireString(j, "name");
  const auto feedId = requireString(j, "feedId");
  const auto opacity = requireNumber(j, "opacity");
  const auto brightness = requireNumber(j, "brightness");
  const auto blendModeStr = requireString(j, "blendMode");
  const int zOrder = requireInteger(j, "zOrder");
  const BlendMode blendMode = parseBlendModeString(blendModeStr);

  std::string surfaceType = "polygon";
  if (j.contains("surfaceType") && j["surfaceType"].is_string()) {
    surfaceType = j["surfaceType"].get<std::string>();
  }

  if (surfaceType == "ellipse") {
    const auto& centerJson = requireField<json>(j, "center");
    Vec2 center{};
    from_json(centerJson, center);
    const float radiusX = requireNumber(j, "radiusX");
    const float radiusY = requireNumber(j, "radiusY");

    surface = Surface(SurfaceId{id}, name, center, radiusX, radiusY, FeedId{feedId}, opacity, brightness, blendMode,
                      zOrder);
  } else {
    const auto& verticesJson = requireField<json>(j, "vertices");
    if (!verticesJson.is_array()) {
      throw std::runtime_error("Field 'vertices' must be an array");
    }
    std::vector<Vec2> vertices;
    vertices.reserve(verticesJson.size());
    for (const auto& vertexJson : verticesJson) {
      Vec2 vec{};
      from_json(vertexJson, vec);
      vertices.push_back(vec);
    }

    surface = Surface(SurfaceId{id}, name, vertices, FeedId{feedId}, opacity, brightness, blendMode, zOrder);
  }

  if (j.contains("rotation") && j["rotation"].is_number()) {
    surface.setRotation(j["rotation"].get<float>());
  }
}

std::string sceneFilterToString(SceneFilter filter) {
  switch (filter) {
    case SceneFilter::ColorTint:
      return "colorTint";
    case SceneFilter::Monochrome:
      return "monochrome";
    case SceneFilter::None:
    default:
      return "none";
  }
}

SceneFilter sceneFilterFromString(const std::string& str) {
  if (str == "colorTint") return SceneFilter::ColorTint;
  if (str == "monochrome") return SceneFilter::Monochrome;
  return SceneFilter::None;
}

void to_json(json& j, const SceneSettings& settings) {
  j = json{{"filter", sceneFilterToString(settings.filter)}, {"colorPaletteIndex", settings.colorPaletteIndex}};
}

void from_json(const json& j, SceneSettings& settings) {
  settings = SceneSettings{};
  if (j.contains("filter") && j["filter"].is_string()) {
    settings.filter = sceneFilterFromString(j["filter"].get<std::string>());
  }
  if (j.contains("colorPaletteIndex") && j["colorPaletteIndex"].is_number_integer()) {
    settings.colorPaletteIndex = j["colorPaletteIndex"].get<int>();
  }
}

void to_json(json& j, const Scene& scene) {
  j = json{{"projectId", scene.getProjectId().value},
           {"id", scene.getId().value},
           {"name", scene.getName()},
           {"description", scene.getDescription()},
           {"surfaces", scene.getSurfaces()},
           {"settings", scene.getSettings()}};
}

void from_json(const json& j, Scene& scene) {
  if (!j.is_object()) {
    throw std::runtime_error("Scene must be an object");
  }
  const auto projectId = requireString(j, "projectId");
  const auto id = requireString(j, "id");
  const auto name = requireString(j, "name");
  const auto description = requireString(j, "description");
  const auto& surfacesJson = requireField<json>(j, "surfaces");
  if (!surfacesJson.is_array()) {
    throw std::runtime_error("Field 'surfaces' must be an array");
  }

  std::vector<Surface> surfaces;
  surfaces.reserve(surfacesJson.size());
  for (const auto& surfaceJson : surfacesJson) {
    Surface surfaceInstance;
    from_json(surfaceJson, surfaceInstance);
    surfaces.push_back(surfaceInstance);
  }

  scene = Scene(ProjectId{projectId}, SceneId{id}, name, description, surfaces);

  if (j.contains("settings") && j["settings"].is_object()) {
    SceneSettings settings;
    from_json(j["settings"], settings);
    scene.setSettings(settings);
  }
}

void to_json(json& j, const Cue& cue) {
  j = json{{"projectId", cue.getProjectId().value},
           {"id", cue.getId().value},
           {"name", cue.getName()},
           {"sceneId", cue.getSceneId().value},
           {"surfaceOpacities", surfaceValueArray(cue.getSurfaceOpacities())},
           {"surfaceBrightnesses", surfaceValueArray(cue.getSurfaceBrightnesses())}};
}

void from_json(const json& j, Cue& cue) {
  if (!j.is_object()) {
    throw std::runtime_error("Cue must be an object");
  }
  const auto projectId = requireString(j, "projectId");
  const auto id = requireString(j, "id");
  const auto name = requireString(j, "name");
  const auto sceneId = requireString(j, "sceneId");

  const auto& opacitiesJson = requireField<json>(j, "surfaceOpacities");
  const auto& brightnessesJson = requireField<json>(j, "surfaceBrightnesses");

  auto opacities = readSurfaceValueArray(opacitiesJson, "surfaceOpacities");
  auto brightnesses = readSurfaceValueArray(brightnessesJson, "surfaceBrightnesses");

  cue = Cue(ProjectId{projectId}, CueId{id}, name, SceneId{sceneId});
  cue.getSurfaceOpacities() = std::move(opacities);
  cue.getSurfaceBrightnesses() = std::move(brightnesses);
}

void to_json(json& j, const ProjectSettings& settings) {
  j = json{{"controllers", settings.controllers},
           {"midiChannels", settings.midiChannels},
           {"globalConfig", settings.globalConfig}};
}

void from_json(const json& j, ProjectSettings& settings) {
  if (!j.is_object()) {
    throw std::runtime_error("ProjectSettings must be an object");
  }
  settings.controllers.clear();
  settings.midiChannels.clear();
  settings.globalConfig.clear();

  if (j.contains("controllers")) {
    const auto& controllers = j.at("controllers");
    if (!controllers.is_object()) {
      throw std::runtime_error("Field 'controllers' must be an object");
    }
    for (auto it = controllers.begin(); it != controllers.end(); ++it) {
      if (!it.value().is_string()) {
        throw std::runtime_error("Controller mappings must be string values");
      }
      settings.controllers[it.key()] = it.value().get<std::string>();
    }
  }

  if (j.contains("midiChannels")) {
    const auto& midiChannels = j.at("midiChannels");
    if (!midiChannels.is_array()) {
      throw std::runtime_error("Field 'midiChannels' must be an array");
    }
    for (const auto& channel : midiChannels) {
      if (!channel.is_number_integer()) {
        throw std::runtime_error("Entries in 'midiChannels' must be integers");
      }
      settings.midiChannels.push_back(channel.get<int>());
    }
  }

  if (j.contains("globalConfig")) {
    const auto& globals = j.at("globalConfig");
    if (globals.is_null()) {
      // Treat explicit null as an empty object for convenience.
    } else if (!globals.is_object()) {
      throw std::runtime_error("Field 'globalConfig' must be an object");
    }
    if (globals.is_object()) {
      for (auto it = globals.begin(); it != globals.end(); ++it) {
        if (!it.value().is_string()) {
          throw std::runtime_error("Global config values must be strings");
        }
        settings.globalConfig[it.key()] = it.value().get<std::string>();
      }
    }
  }
}

void to_json(json& j, const Project& project) {
  j = json{{"id", project.getId().value},
           {"name", project.getName()},
           {"description", project.getDescription()},
           {"createdAt", project.getCreatedAt()},
           {"updatedAt", project.getUpdatedAt()},
           {"assetIds", json::array()},
           {"sceneIds", json::array()},
           {"feedIds", json::array()},
           {"cueOrder", json::array()},
           {"settings", project.getSettings()}};

  for (const auto& assetId : project.getAssetIds()) {
    j["assetIds"].push_back(assetId.value);
  }
  for (const auto& sceneId : project.getSceneIds()) {
    j["sceneIds"].push_back(sceneId.value);
  }
  for (const auto& feedId : project.getFeedIds()) {
    j["feedIds"].push_back(feedId.value);
  }
  for (const auto& cueId : project.getCueOrder()) {
    j["cueOrder"].push_back(cueId.value);
  }
}

void from_json(const json& j, Project& project) {
  if (!j.is_object()) {
    throw std::runtime_error("Project must be an object");
  }
  const auto id = requireString(j, "id");
  const auto name = requireString(j, "name");
  const auto description = requireString(j, "description");
  std::string createdAt;
  if (j.contains("createdAt") && !j.at("createdAt").is_null()) {
    createdAt = requireString(j, "createdAt");
  }

  std::string updatedAt;
  if (j.contains("updatedAt") && !j.at("updatedAt").is_null()) {
    updatedAt = requireString(j, "updatedAt");
  }

  std::vector<std::string> assetIds;
  if (j.contains("assetIds") && !j.at("assetIds").is_null()) {
    assetIds = readStringArray(j.at("assetIds"), "assetIds");
  }

  std::vector<std::string> sceneIds;
  if (j.contains("sceneIds") && !j.at("sceneIds").is_null()) {
    sceneIds = readStringArray(j.at("sceneIds"), "sceneIds");
  }

  std::vector<std::string> feedIds;
  if (j.contains("feedIds") && !j.at("feedIds").is_null()) {
    feedIds = readStringArray(j.at("feedIds"), "feedIds");
  }

  std::vector<std::string> cueOrder;
  if (j.contains("cueOrder") && !j.at("cueOrder").is_null()) {
    cueOrder = readStringArray(j.at("cueOrder"), "cueOrder");
  }

  ProjectSettings settings{};
  if (j.contains("settings")) {
    from_json(j.at("settings"), settings);
  }

  std::vector<AssetId> assetIdList;
  assetIdList.reserve(assetIds.size());
  for (const auto& value : assetIds) {
    assetIdList.emplace_back(value);
  }

  std::vector<SceneId> sceneIdList;
  sceneIdList.reserve(sceneIds.size());
  for (const auto& value : sceneIds) {
    sceneIdList.emplace_back(value);
  }

  std::vector<FeedId> feedIdList;
  feedIdList.reserve(feedIds.size());
  for (const auto& value : feedIds) {
    feedIdList.emplace_back(value);
  }

  std::vector<CueId> cueIdList;
  cueIdList.reserve(cueOrder.size());
  for (const auto& value : cueOrder) {
    cueIdList.emplace_back(value);
  }

  project = Project(ProjectId{id}, name, description, createdAt, updatedAt, assetIdList, sceneIdList, feedIdList,
                    cueIdList, settings);
}

}  // namespace projection::core
