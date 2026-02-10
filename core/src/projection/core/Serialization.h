#pragma once

#include <stdexcept>
#include <string>

#include <nlohmann/json.hpp>

#include "projection/core/Asset.h"
#include "projection/core/Cue.h"
#include "projection/core/Enums.h"
#include "projection/core/Feed.h"
#include "projection/core/Project.h"
#include "projection/core/Scene.h"
#include "projection/core/Surface.h"

namespace projection::core {

// JSON serialization helpers for core domain types.
//
// Error handling strategy: deserialization functions throw std::runtime_error when JSON is
// missing required fields, has the wrong type, or contains invalid enum strings.

void to_json(nlohmann::json& j, const AssetType& type);
void from_json(const nlohmann::json& j, AssetType& type);

void to_json(nlohmann::json& j, const BlendMode& mode);
void from_json(const nlohmann::json& j, BlendMode& mode);

void to_json(nlohmann::json& j, const AssetVariant& variant);
void from_json(const nlohmann::json& j, AssetVariant& variant);

void to_json(nlohmann::json& j, const Asset& asset);
void from_json(const nlohmann::json& j, Asset& asset);

void to_json(nlohmann::json& j, const FeedSettings& settings);
void from_json(const nlohmann::json& j, FeedSettings& settings);

void to_json(nlohmann::json& j, const Vec2& vec);
void from_json(const nlohmann::json& j, Vec2& vec);

void to_json(nlohmann::json& j, const Feed& feed);
void from_json(const nlohmann::json& j, Feed& feed);

void to_json(nlohmann::json& j, const Surface& surface);
void from_json(const nlohmann::json& j, Surface& surface);

void to_json(nlohmann::json& j, const SceneSettings& settings);
void from_json(const nlohmann::json& j, SceneSettings& settings);

void to_json(nlohmann::json& j, const Scene& scene);
void from_json(const nlohmann::json& j, Scene& scene);

void to_json(nlohmann::json& j, const Cue& cue);
void from_json(const nlohmann::json& j, Cue& cue);

void to_json(nlohmann::json& j, const ProjectSettings& settings);
void from_json(const nlohmann::json& j, ProjectSettings& settings);

void to_json(nlohmann::json& j, const Project& project);
void from_json(const nlohmann::json& j, Project& project);

}  // namespace projection::core
