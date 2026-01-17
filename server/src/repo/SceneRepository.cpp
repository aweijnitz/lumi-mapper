#include "repo/SceneRepository.h"

#include <sqlite3.h>

#include <chrono>
#include <stdexcept>
#include <string>

#include "repo/SurfaceRepository.h"

namespace projection::server::repo {

namespace {
std::string generateId() {
    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    return "scene-" + std::to_string(now);
}
}

SceneRepository::SceneRepository(db::SqliteConnection& connection) : connection_(connection) {}

core::Scene SceneRepository::createScene(const core::Scene& scene) {
    auto lock = connection_.lock();
    sqlite3* handle = connection_.getHandle();
    if (handle == nullptr) {
        throw std::runtime_error("SQLite connection is not open");
    }
    if (scene.getProjectId().value.empty()) {
        throw std::runtime_error("Scene project id must not be empty");
    }

    const std::string idValue = scene.getId().value.empty() ? generateId() : scene.getId().value;
    const char* sql = "INSERT INTO scenes(project_id, id, name, description) VALUES(?, ?, ?, ?);";

    sqlite3_stmt* stmt = nullptr;
    int result = sqlite3_prepare_v2(handle, sql, -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare scene insert statement: " + std::string(sqlite3_errmsg(handle)));
    }

    int bindIndex = 1;
    result = sqlite3_bind_text(stmt, bindIndex++, scene.getProjectId().value.c_str(), -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to bind scene project id: " + std::string(sqlite3_errmsg(handle)));
    }

    result = sqlite3_bind_text(stmt, bindIndex++, idValue.c_str(), -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to bind scene id: " + std::string(sqlite3_errmsg(handle)));
    }

    result = sqlite3_bind_text(stmt, bindIndex++, scene.getName().c_str(), -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to bind scene name: " + std::string(sqlite3_errmsg(handle)));
    }

    result = sqlite3_bind_text(stmt, bindIndex, scene.getDescription().c_str(), -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to bind scene description: " + std::string(sqlite3_errmsg(handle)));
    }

    result = sqlite3_step(stmt);
    if (result != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to insert scene: " + std::string(sqlite3_errmsg(handle)));
    }

    sqlite3_finalize(stmt);

    core::Scene created = scene;
    created.setId(core::SceneId(idValue));

    repo::SurfaceRepository surfaceRepo(connection_);
    for (const auto& surface : scene.getSurfaces()) {
        surfaceRepo.createSurface(surface, scene.getProjectId(), created.getId());
    }

    return created;
}

std::vector<core::Scene> SceneRepository::listScenes(const core::ProjectId& projectId) {
    auto lock = connection_.lock();
    sqlite3* handle = connection_.getHandle();
    if (handle == nullptr) {
        throw std::runtime_error("SQLite connection is not open");
    }

    const char* sql = "SELECT id, name, description FROM scenes WHERE project_id=? ORDER BY id;";

    sqlite3_stmt* stmt = nullptr;
    int result = sqlite3_prepare_v2(handle, sql, -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare scene select statement: " + std::string(sqlite3_errmsg(handle)));
    }

    result = sqlite3_bind_text(stmt, 1, projectId.value.c_str(), -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to bind project id for scene list: " + std::string(sqlite3_errmsg(handle)));
    }

    std::vector<core::Scene> scenes;
    while ((result = sqlite3_step(stmt)) == SQLITE_ROW) {
        const unsigned char* idText = sqlite3_column_text(stmt, 0);
        const unsigned char* nameText = sqlite3_column_text(stmt, 1);
        const unsigned char* descText = sqlite3_column_text(stmt, 2);

        std::string idString = idText ? reinterpret_cast<const char*>(idText) : "";
        std::string name = nameText ? reinterpret_cast<const char*>(nameText) : "";
        std::string description = descText ? reinterpret_cast<const char*>(descText) : "";

        scenes.emplace_back(core::Scene(projectId, core::SceneId(idString), name, description, {}));
    }

    if (result != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to read scenes: " + std::string(sqlite3_errmsg(handle)));
    }

    sqlite3_finalize(stmt);

    repo::SurfaceRepository surfaceRepo(connection_);
    for (auto& scene : scenes) {
        scene.setSurfaces(surfaceRepo.listSurfacesForScene(projectId, scene.getId()));
    }

    return scenes;
}

std::optional<core::Scene> SceneRepository::findSceneById(const core::ProjectId& projectId,
                                                          const core::SceneId& sceneId) {
    auto lock = connection_.lock();
    sqlite3* handle = connection_.getHandle();
    if (handle == nullptr) {
        throw std::runtime_error("SQLite connection is not open");
    }

    const char* sql = "SELECT id, name, description FROM scenes WHERE project_id = ? AND id = ? LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;
    int result = sqlite3_prepare_v2(handle, sql, -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare scene select statement: " + std::string(sqlite3_errmsg(handle)));
    }

    result = sqlite3_bind_text(stmt, 1, projectId.value.c_str(), -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to bind scene project id: " + std::string(sqlite3_errmsg(handle)));
    }

    result = sqlite3_bind_text(stmt, 2, sceneId.value.c_str(), -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to bind scene id: " + std::string(sqlite3_errmsg(handle)));
    }

    result = sqlite3_step(stmt);
    if (result == SQLITE_ROW) {
        const unsigned char* nameText = sqlite3_column_text(stmt, 1);
        const unsigned char* descText = sqlite3_column_text(stmt, 2);

        std::string name = nameText ? reinterpret_cast<const char*>(nameText) : "";
        std::string description = descText ? reinterpret_cast<const char*>(descText) : "";
        sqlite3_finalize(stmt);
        repo::SurfaceRepository surfaceRepo(connection_);
        auto surfaces = surfaceRepo.listSurfacesForScene(projectId, sceneId);
        return core::Scene(projectId, sceneId, name, description, surfaces);
    }

    if (result != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to read scene: " + std::string(sqlite3_errmsg(handle)));
    }

    sqlite3_finalize(stmt);
    return std::nullopt;
}

bool SceneRepository::sceneExists(const core::ProjectId& projectId, const core::SceneId& sceneId) {
    return findSceneById(projectId, sceneId).has_value();
}

core::Scene SceneRepository::updateScene(const core::Scene& scene) {
    auto lock = connection_.lock();
    sqlite3* handle = connection_.getHandle();
    if (handle == nullptr) {
        throw std::runtime_error("SQLite connection is not open");
    }
    if (scene.getId().value.empty()) {
        throw std::runtime_error("Scene id must not be empty for update");
    }
    if (scene.getProjectId().value.empty()) {
        throw std::runtime_error("Scene project id must not be empty for update");
    }

    const char* sql = "UPDATE scenes SET name=?, description=? WHERE project_id=? AND id=?;";
    sqlite3_stmt* stmt = nullptr;
    int result = sqlite3_prepare_v2(handle, sql, -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare scene update statement: " + std::string(sqlite3_errmsg(handle)));
    }

    result = sqlite3_bind_text(stmt, 1, scene.getName().c_str(), -1, SQLITE_TRANSIENT);
    result |= sqlite3_bind_text(stmt, 2, scene.getDescription().c_str(), -1, SQLITE_TRANSIENT);
    result |= sqlite3_bind_text(stmt, 3, scene.getProjectId().value.c_str(), -1, SQLITE_TRANSIENT);
    result |= sqlite3_bind_text(stmt, 4, scene.getId().value.c_str(), -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to bind scene update fields: " + std::string(sqlite3_errmsg(handle)));
    }
    result = sqlite3_step(stmt);
    if (result != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to update scene: " + std::string(sqlite3_errmsg(handle)));
    }
    sqlite3_finalize(stmt);

    // Replace surfaces: delete and re-insert
    repo::SurfaceRepository surfaceRepo(connection_);
    surfaceRepo.deleteSurfacesForScene(scene.getProjectId(), scene.getId());
    for (const auto& surface : scene.getSurfaces()) {
        surfaceRepo.createSurface(surface, scene.getProjectId(), scene.getId());
    }
    return scene;
}

void SceneRepository::deleteScene(const core::ProjectId& projectId, const core::SceneId& sceneId) {
    auto lock = connection_.lock();
    sqlite3* handle = connection_.getHandle();
    if (handle == nullptr) {
        throw std::runtime_error("SQLite connection is not open");
    }

    const char* sql = "DELETE FROM scenes WHERE project_id=? AND id=?;";
    sqlite3_stmt* stmt = nullptr;
    int result = sqlite3_prepare_v2(handle, sql, -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare scene delete statement: " + std::string(sqlite3_errmsg(handle)));
    }

    result = sqlite3_bind_text(stmt, 1, projectId.value.c_str(), -1, SQLITE_TRANSIENT);
    result |= sqlite3_bind_text(stmt, 2, sceneId.value.c_str(), -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to bind scene id for delete: " + std::string(sqlite3_errmsg(handle)));
    }

    result = sqlite3_step(stmt);
    if (result != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to delete scene: " + std::string(sqlite3_errmsg(handle)));
    }
    sqlite3_finalize(stmt);

    repo::SurfaceRepository surfaceRepo(connection_);
    surfaceRepo.deleteSurfacesForScene(projectId, sceneId);
}

}  // namespace projection::server::repo
