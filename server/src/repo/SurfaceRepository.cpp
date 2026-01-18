#include "repo/SurfaceRepository.h"

#include <sqlite3.h>

#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>

#include "projection/core/Serialization.h"

namespace projection::server::repo {

SurfaceRepository::SurfaceRepository(db::SqliteConnection& connection) : connection_(connection) {}

core::Surface SurfaceRepository::createSurface(const core::Surface& surface, const core::ProjectId& projectId,
                                               const core::SceneId& sceneId) {
    auto lock = connection_.lock();
    sqlite3* handle = connection_.getHandle();
    if (handle == nullptr) {
        throw std::runtime_error("SQLite connection is not open");
    }

    if (surface.getId().value.empty()) {
        throw std::runtime_error("Surface id must not be empty");
    }

    const char* sql =
        "INSERT INTO surfaces(project_id, id, scene_id, name, feed_id, z_order, opacity, brightness, blend_mode, "
        "vertices_json, rotation) VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";

    sqlite3_stmt* stmt = nullptr;
    int result = sqlite3_prepare_v2(handle, sql, -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare surface insert statement: " + std::string(sqlite3_errmsg(handle)));
    }

    // Serialize shape data based on surface type
    nlohmann::json shapeJson;
    if (surface.isEllipse()) {
        shapeJson["surfaceType"] = "ellipse";
        shapeJson["center"] = nlohmann::json{{"x", surface.getCenter().x}, {"y", surface.getCenter().y}};
        shapeJson["radiusX"] = surface.getRadiusX();
        shapeJson["radiusY"] = surface.getRadiusY();
    } else {
        shapeJson["surfaceType"] = "polygon";
        shapeJson["vertices"] = surface.getVertices();
    }
    const std::string shapeJsonStr = shapeJson.dump();
    const std::string blendMode = core::toString(surface.getBlendMode());

    int bindIndex = 1;
    result = sqlite3_bind_text(stmt, bindIndex++, projectId.value.c_str(), -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to bind surface project id: " + std::string(sqlite3_errmsg(handle)));
    }

    result = sqlite3_bind_text(stmt, bindIndex++, surface.getId().value.c_str(), -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to bind surface id: " + std::string(sqlite3_errmsg(handle)));
    }

    result = sqlite3_bind_text(stmt, bindIndex++, sceneId.value.c_str(), -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to bind scene id: " + std::string(sqlite3_errmsg(handle)));
    }

    result = sqlite3_bind_text(stmt, bindIndex++, surface.getName().c_str(), -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to bind surface name: " + std::string(sqlite3_errmsg(handle)));
    }

    result = sqlite3_bind_text(stmt, bindIndex++, surface.getFeedId().value.c_str(), -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to bind surface feed id: " + std::string(sqlite3_errmsg(handle)));
    }

    result = sqlite3_bind_int(stmt, bindIndex++, surface.getZOrder());
    if (result != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to bind surface z_order: " + std::string(sqlite3_errmsg(handle)));
    }

    result = sqlite3_bind_double(stmt, bindIndex++, surface.getOpacity());
    if (result != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to bind surface opacity: " + std::string(sqlite3_errmsg(handle)));
    }

    result = sqlite3_bind_double(stmt, bindIndex++, surface.getBrightness());
    if (result != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to bind surface brightness: " + std::string(sqlite3_errmsg(handle)));
    }

    result = sqlite3_bind_text(stmt, bindIndex++, blendMode.c_str(), -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to bind surface blend mode: " + std::string(sqlite3_errmsg(handle)));
    }

    result = sqlite3_bind_text(stmt, bindIndex++, shapeJsonStr.c_str(), -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to bind surface shape data: " + std::string(sqlite3_errmsg(handle)));
    }

    result = sqlite3_bind_double(stmt, bindIndex, surface.getRotation());
    if (result != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to bind surface rotation: " + std::string(sqlite3_errmsg(handle)));
    }

    result = sqlite3_step(stmt);
    if (result != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to insert surface: " + std::string(sqlite3_errmsg(handle)));
    }

    sqlite3_finalize(stmt);
    return surface;
}

std::vector<core::Surface> SurfaceRepository::listSurfacesForScene(const core::ProjectId& projectId,
                                                                    const core::SceneId& sceneId) {
    auto lock = connection_.lock();
    sqlite3* handle = connection_.getHandle();
    if (handle == nullptr) {
        throw std::runtime_error("SQLite connection is not open");
    }

    const char* sql =
        "SELECT id, name, vertices_json, feed_id, opacity, brightness, blend_mode, z_order, rotation "
        "FROM surfaces WHERE project_id = ? AND scene_id = ? ORDER BY z_order, id;";

    sqlite3_stmt* stmt = nullptr;
    int result = sqlite3_prepare_v2(handle, sql, -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare surface select statement: " + std::string(sqlite3_errmsg(handle)));
    }

    result = sqlite3_bind_text(stmt, 1, projectId.value.c_str(), -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to bind surface project id: " + std::string(sqlite3_errmsg(handle)));
    }

    result = sqlite3_bind_text(stmt, 2, sceneId.value.c_str(), -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to bind scene id: " + std::string(sqlite3_errmsg(handle)));
    }

    std::vector<core::Surface> surfaces;
    while ((result = sqlite3_step(stmt)) == SQLITE_ROW) {
        const unsigned char* idText = sqlite3_column_text(stmt, 0);
        const unsigned char* nameText = sqlite3_column_text(stmt, 1);
        const unsigned char* verticesText = sqlite3_column_text(stmt, 2);
        const unsigned char* feedIdText = sqlite3_column_text(stmt, 3);
        double opacity = sqlite3_column_double(stmt, 4);
        double brightness = sqlite3_column_double(stmt, 5);
        const unsigned char* blendModeText = sqlite3_column_text(stmt, 6);
        int zOrder = sqlite3_column_int(stmt, 7);
        double rotation = sqlite3_column_double(stmt, 8);

        std::string id = idText ? reinterpret_cast<const char*>(idText) : "";
        std::string name = nameText ? reinterpret_cast<const char*>(nameText) : "";
        std::string verticesJson = verticesText ? reinterpret_cast<const char*>(verticesText) : "";
        std::string feedId = feedIdText ? reinterpret_cast<const char*>(feedIdText) : "";
        std::string blendModeStr = blendModeText ? reinterpret_cast<const char*>(blendModeText) : "";

        nlohmann::json shapeData = nlohmann::json::parse(verticesJson);

        core::BlendMode blendMode;
        if (!core::fromString(blendModeStr, blendMode)) {
            sqlite3_finalize(stmt);
            throw std::runtime_error("Failed to parse blend mode for surface: " + blendModeStr);
        }

        // Determine surface type (default to polygon for backwards compatibility)
        std::string surfaceType = "polygon";
        if (shapeData.contains("surfaceType") && shapeData["surfaceType"].is_string()) {
            surfaceType = shapeData["surfaceType"].get<std::string>();
        }

        core::Surface surface;
        if (surfaceType == "ellipse") {
            // Parse ellipse surface
            core::Vec2 center{0.0f, 0.0f};
            if (shapeData.contains("center") && shapeData["center"].is_object()) {
                center.x = shapeData["center"]["x"].get<float>();
                center.y = shapeData["center"]["y"].get<float>();
            }
            float radiusX = shapeData.value("radiusX", 0.45f);
            float radiusY = shapeData.value("radiusY", 0.45f);
            surface = core::Surface(core::SurfaceId{id}, name, center, radiusX, radiusY, core::FeedId{feedId},
                                    static_cast<float>(opacity), static_cast<float>(brightness), blendMode, zOrder);
        } else {
            // Parse polygon surface (legacy format: direct array or wrapped in "vertices" key)
            std::vector<core::Vec2> vertices;
            if (shapeData.is_array()) {
                // Legacy format: direct vertex array
                vertices = shapeData.get<std::vector<core::Vec2>>();
            } else if (shapeData.contains("vertices") && shapeData["vertices"].is_array()) {
                // New format: wrapped in object
                vertices = shapeData["vertices"].get<std::vector<core::Vec2>>();
            }
            surface = core::Surface(core::SurfaceId{id}, name, vertices, core::FeedId{feedId},
                                    static_cast<float>(opacity), static_cast<float>(brightness), blendMode, zOrder);
        }
        surface.setRotation(static_cast<float>(rotation));
        surfaces.emplace_back(std::move(surface));
    }

    if (result != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to read surfaces: " + std::string(sqlite3_errmsg(handle)));
    }

    sqlite3_finalize(stmt);
    return surfaces;
}

void SurfaceRepository::deleteSurfacesForScene(const core::ProjectId& projectId, const core::SceneId& sceneId) {
    auto lock = connection_.lock();
    sqlite3* handle = connection_.getHandle();
    if (handle == nullptr) {
        throw std::runtime_error("SQLite connection is not open");
    }

    const char* sql = "DELETE FROM surfaces WHERE project_id = ? AND scene_id = ?;";
    sqlite3_stmt* stmt = nullptr;
    int result = sqlite3_prepare_v2(handle, sql, -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare surface delete statement: " + std::string(sqlite3_errmsg(handle)));
    }

    result = sqlite3_bind_text(stmt, 1, projectId.value.c_str(), -1, SQLITE_TRANSIENT);
    result |= sqlite3_bind_text(stmt, 2, sceneId.value.c_str(), -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to bind scene id for surface delete: " + std::string(sqlite3_errmsg(handle)));
    }

    result = sqlite3_step(stmt);
    if (result != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to delete surfaces: " + std::string(sqlite3_errmsg(handle)));
    }
    sqlite3_finalize(stmt);
}

}  // namespace projection::server::repo
