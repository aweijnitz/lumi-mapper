# Local Development (Composer + Server + Renderer)

This file covers local end-to-end development of the Composer UI (webapp with hot reload) talking to the server on localhost.

## Prerequisites

- C++ toolchain + CMake (see `README.md` for platform notes).
- SQLite3 development headers (server links against system `sqlite3`. See `README.md`).
- openFrameworks install for the renderer, with `OPENFRAMEWORKS_DIR` environment variable set (See `README.md`).
- Node.js + npm for the Composer UI.

## 1) Build the native binaries

From the repo root:

```sh
./scripts/build_all.sh # Assumes OPENFRAMEWORKS_DIR previouslyy set "export OPENFRAMEWORKS_DIR=..."

# Example with OPENFRAMEWORKS_DIR
OPENFRAMEWORKS_DIR=/Users/aweijnitz/openFrameworks/of_v0.12.1_osx_release ./scripts/build_all.sh
```

This produces:

- `./build/server/lumi_server`
- `./build/renderer/renderer_default`

## 2) Start the server (localhost API)

In terminal A:

```sh
SERVER_ARGS="--verbose" ./scripts/server.sh start
```

Defaults:

- HTTP API: `http://localhost:8080`
- Renderer listen port: `5050`
- DB: `./data/db/projection.db`

To stop it:

```sh
./scripts/server.sh stop
```

## 3) Start the renderer

In terminal B:

```sh
./scripts/renderer.sh --verbose
```

This connects to the server on `127.0.0.1:5050` and opens the renderer window.

## 4) Start the Composer (hot reload)

In terminal C:

```sh
cd clients/composer
npm install
npm run dev
```

The Vite dev server proxies `/api/*` calls to `http://localhost:8080`, so the Composer can talk to the C++ server while hot-reloading.

## Quick sanity check

```sh
curl http://localhost:8080/api/renderer/ping
```

If the renderer is connected, you should see it listed by name.

### Troubleshooting

The VIite proxy can be problematic.

  - Open http://localhost:5173/api/renderer/ping in the browser; if that 404s, Vite isn’t proxying.
  - Open http://localhost:8080/api/renderer/ping directly; if that 404s, the server isn’t exposing /api (or it’s on a different port).

Expected response is something along the lines of
```sh
{"renderers":["renderer-25728"],"status":"ok"}
```

## Common overrides

You can override ports or paths via environment variables:

```sh
# server
SERVER_PORT=8081 RENDERER_PORT=6060 ./scripts/server.sh start

# renderer
RENDERER_PORT=6060 RENDERER_NAME=renderer-dev ./scripts/renderer.sh --verbose
```

If you change ports, update the Composer proxy target in `clients/composer/vite.config.ts` or set it via whatever local override you use.
