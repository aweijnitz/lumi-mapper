# Lumi Server

✅ **Role & Overview**

The Lumi Server is the central, authoritative backend for the projection-mapper system. It:

- Persists domain state (Projects, Feeds, Scenes, Cues) in an embedded **SQLite3** database.
- Exposes an **HTTP JSON API** for clients to CRUD projects, feeds, scenes, and cues and control the renderer.
- Manages renderer registration and control messages (renderer protocol port).
- Optionally serves the Composer single-page app from a provided `--web-root` directory.

This component is implemented in C++ (C++17) and intended to run on macOS and Linux (including Raspberry Pi).

---

## Requirements

- CMake (>= 3.20)
- A C++17-capable compiler (clang / gcc)
- SQLite3 development files (e.g. `libsqlite3-dev` or system package)
- Build tools: `make` or `ninja`
- Optional: Docker (for containerized builds)

---

## Build (local)

From the repository root (preferred):

```bash
# Configure
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug

# Build server only (recommended)
cmake --build build --target lumi_server -j

# Build tests
cmake --build build --target lumi_server_tests -j
```

Or build everything with:

```bash
cmake --build build -- -j
```

---

## Run

Default behavior: server expects an embedded SQLite DB at `./data/db/projection.db` (relative to working dir).

Run the server executable directly from the build:

```bash
./build/server/lumi_server [--db <path>] [--web-root <dir>] [--port <httpPort>] [--renderer-port <rendererPort>] [--verbose]
```

Examples:

```bash
# Use default database and port 8080
./build/server/lumi_server

# Specify a different DB and enable verbose logging
./build/server/lumi_server --db ./data/db/projection.db --verbose

# Serve a Composer build from web root and use port 9000
./build/server/lumi_server --web-root ./clients/composer/dist --port 9000
```

If you prefer Docker (build image from `server/Dockerfile`):

```bash
# Build image
docker build -t lumi_server:local server/

# Run container, mounting persistent DB and exposing port 8080
docker run --rm -p 8080:8080 -v $(pwd)/data/db:/data/db lumi_server:local
```

> Note: The Docker image uses `VOLUME ["/data/db", "/data/assets"]`, so mounting `./data/db` makes the DB persistent.

---

## HTTP API overview

The server accepts both `/api/...` and root-level `/...` paths.

Project CRUD:
- `GET /projects`
- `GET /projects/{id}`
- `POST /projects`
- `PUT /projects/{id}`
- `DELETE /projects/{id}`

Project-scoped entities:
- `GET /projects/{projectId}/feeds`
- `POST /projects/{projectId}/feeds`
- `PUT /projects/{projectId}/feeds/{feedId}`
- `DELETE /projects/{projectId}/feeds/{feedId}`
- `GET /projects/{projectId}/scenes`
- `GET /projects/{projectId}/scenes/{sceneId}`
- `POST /projects/{projectId}/scenes`
- `PUT /projects/{projectId}/scenes/{sceneId}`
- `DELETE /projects/{projectId}/scenes/{sceneId}`
- `GET /projects/{projectId}/cues`
- `POST /projects/{projectId}/cues`
- `PUT /projects/{projectId}/cues/{cueId}`
- `DELETE /projects/{projectId}/cues/{cueId}`

Renderer control:
- `GET /renderer/ping` / `POST /renderer/ping`
- `POST /projects/{projectId}/renderer/loadScene`
- `POST /projects/{projectId}/renderer/playCue`
- `POST /renderer/testPattern` (toggle calibration grid)
- `POST /renderer/crosshair` (show/hide crosshair overlay)

Assets:
- `GET /assets` (lists assets under `./data/assets`, `../data/assets`, or `../../data/assets`)
- `POST /assets` (multipart upload field name `file`, 2GB max)
- `DELETE /assets/{name}`

Demo helpers:
- `POST /demo/two-video-test`
- `POST /demo/clear-projects`

---

## Tests

Unit and integration tests live under `server/tests` and are built as `lumi_server_tests`.

Run tests with ctest from the build directory or run the test binary directly:

```bash
cd build
ctest -R lumi_server_tests -V
# or
./server/lumi_server_tests
```

---

## Development Tips

- Config flags are parsed in `server/src/Config.cpp`:
  - `--db <path>` — path to SQLite DB (default `./data/db/projection.db`)
  - `--web-root <dir>` — optional static web root for SPA
  - `--port <n>` — HTTP API port (default 8080)
  - `--renderer-port <n>` — renderer control port (default 5050)
  - `--verbose` — enable verbose logging

- The HTTP API is implemented in `server/src/http/HttpServer.cpp` and tested in `server/tests/HttpApi_test.cpp`.
- The DB schema migrations are in `server/src/db/SchemaMigrations.*` and are applied automatically at startup.
- When changing the DB schema, add a migration entry in `SchemaMigrations` and add tests that cover migration correctness.

---

## Packaging & Deployment

- For simple deployments, build the server executable and deploy it alongside the `./data/db` directory (ensure file permissions allow writes).
- For containerized deployments, use the provided `server/Dockerfile` and mount `/data/db` to persist the SQLite DB.

---

## Troubleshooting

- SQLite missing at CMake configure time: install SQLite3 dev package (e.g., `brew install sqlite` or `apt install libsqlite3-dev`).
- Port conflicts: change `--port` or stop conflicting service.
- If static SPA doesn't load, verify `--web-root` points to the built Composer output (index.html and assets).

---

## Contributing

- Add unit tests for any new logic under `server/tests`.
- Follow the project CMake and testing layout; tests must be registered with `add_test` for CI.
- Update this README if you change CLI flags or the server's public behavior.
