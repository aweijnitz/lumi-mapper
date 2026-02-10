# System Architecture

This document summarizes the current architecture of the projection/video mapping stack and how the main pieces collaborate.

## Container/Context View
- **Diagram (Container level)**:

```mermaid
flowchart LR
    Client[Clients\nCLI / GUI tools] -->|HTTP+JSON\nport 8080| Server[Server\nlumi_server]
    Server -->|TCP control\nport 5050| Renderer[Renderer\nprojection_renderer]
    Server -->|Embedded DB\n./data/db/projection.db| SQLite[(SQLite file)]
    Server -->|Asset metadata| Assets[Asset Storage\n./data/assets]
    Renderer -->|Reads media| Assets
    Renderer -->|Video output| Projector[Projector / Display]
    Renderer -->|MIDI / Audio| Inputs[MIDI controllers\nAudio input]
```

- **Clients**: External tools (CLI/GUI) that manage assets, feeds, surfaces, scenes, and playback through the Server’s HTTP+JSON API.
- **Server (`lumi_server`)**: C++ service that exposes HTTP endpoints, orchestrates persistence, and forwards control commands to the Renderer.
- **Renderer (`projection_renderer`)**: openFrameworks-based process that receives control protocol messages and renders the active scene on the projector/display.
- **Core Library (`projection_core`)**: Shared C++ domain model (assets, feeds, scenes, surfaces, cues) with JSON serialization/validation used by both Server and Renderer.
- **SQLite Database**: Embedded file-backed DB (`./data/db/projection.db`) used by the Server for durable state.
- **Asset Storage**: Filesystem directory (e.g., `./data/assets`) holding video/image assets referenced by feeds.

## High-Level Data & Control Flows
1. **Client → Server (HTTP+JSON)**: Clients call the HTTP API to manage domain objects and drive the renderer.
   - Assets: `POST /assets` uploads assets; `GET /assets` lists assets.
   - Feeds: `POST /feeds` creates a feed (references an asset); `GET /feeds` lists feeds.
   - Scenes: `POST /scenes` creates a scene (validated against existing feeds); `GET /scenes` lists scenes; `GET /scenes/{id}` fetches one.
   - Projects: `POST /projects` creates a project (ordered `cueOrder` plus settings), `GET /projects` lists projects, `GET /projects/{id}` fetches one, `PUT /projects/{id}` updates, `DELETE /projects/{id}` removes; cues used by projects cannot be deleted.
   - Renderer control: `POST /renderer/ping` issues a `Hello` control message round-trip; `POST /renderer/loadScene` asks the renderer to load a stored scene definition.
2. **Server → SQLite**: The Server initializes a `db::SqliteConnection`, applies migrations, and persists assets/feeds/scenes through repositories before serving HTTP (`ServerApp::run`).
3. **Server → Renderer (TCP Control Protocol)**: `RendererClient` connects at startup, then sends control messages such as `LoadSceneDefinition` (scene + feeds + assets) over TCP (default renderer port 5050).
4. **Renderer → Projector/Display**: The Renderer draws the active scene using openFrameworks, applying MIDI/audio-driven modulation where configured.
5. **Assets on Disk**: Asset records point at files under `./data/assets`; feeds reference asset IDs; the Renderer reads the media directly when playing video feeds.

## Domain Model (Object Graph)

```mermaid
erDiagram
  PROJECT {
    string id
    string name
    string description
    string createdAt
    string updatedAt
    string[] assetIds
    string[] sceneIds
    string[] feedIds
    string[] cueOrder
  }
  ASSET {
    string id
    string name
    string type
    string path
  }
  FEED {
    string id
    string name
    string assetId
  }
  SCENE {
    string id
    string name
    string description
  }
  SURFACE {
    string id
    string name
    string feedId
  }
  CUE {
    string id
    string name
    string sceneId
  }
  PROJECT }o..o{ ASSET : "project_assets"
  PROJECT ||..o{ FEED : "scopes"
  PROJECT ||..o{ SCENE : "scopes"
  PROJECT ||..o{ CUE : "scopes (cueOrder)"
  FEED }o--|| ASSET : "references"
  SCENE ||--o{ SURFACE : "contains"
  FEED ||..o{ SURFACE : "source_for"
  SCENE ||..o{ CUE : "referenced_by"
  CUE }o..o{ SURFACE : "overrides"
```

## Server Component View
- **Diagram (Component level)**:

```mermaid
flowchart TB
    subgraph Server[lumi_server]
        Main[Main / Config Parsing] --> App[ServerApp wiring]
        App --> Http[HTTP Layer\nhttp::HttpServer]
        App --> DBLayer[DB Layer\ndb::SqliteConnection + SchemaMigrations]
        App --> Repo[Repositories\nAssets / Feeds / Scenes / Surfaces]
        App --> RC[Renderer Client\nTCP adapter]
        Http --> Repo
        Http --> RC
        Repo --> DBLayer
    end
    Repo -. uses .-> Core[Core Library\nprojection_core types]
    Http -. validates .-> Core
    RC -. serializes .-> Core
    DBLayer --> SQLite[(SQLite file)]
    RC --> Renderer[Renderer process]
```

- **Main/Configuration**: Entry point parses CLI flags (DB path, HTTP port, renderer host/port) and wires dependencies.
- **HTTP Layer**: Thin cpp-httplib wrapper (`http::HttpServer`) exposes project-scoped routes for feeds/scenes/cues under `/api/projects/{projectId}/*` plus renderer control; requests translate into domain operations.
- **Repositories & DB Layer**: `db::SqliteConnection` plus schema migrations back repositories that CRUD assets/feeds/scenes/cues while enforcing validation against asset/feed IDs within a project.
- **Renderer Client Adapter**: `RendererClient` connects during startup and maps HTTP handler intents (ping, load scene) to protocol messages before HTTP serving begins.
- **Domain Integration**: Uses the core library’s types and JSON helpers to validate payloads and serialize messages consistently across layers.

## Renderer Component View
- **Diagram (Component level)**:

```mermaid
flowchart TB
    subgraph Renderer[projection_renderer]
        RS[RendererServer Listener] --> Queue[Message queue / dispatcher]
        Queue --> ofApp[ofApp update loop]
        ofApp --> State[Render State\nscenes / feeds / surfaces]
        ofApp --> Render[Rendering Loop\nopenFrameworks draw]
        Inputs[MIDI / Audio Inputs] --> ofApp
    end
    Server[Server control client] --> RS
    Assets[Asset files] --> Render
    Render --> Projector[Projector / Display output]
```

- **RendererServer Listener**: Accepts TCP connections and decodes newline-delimited control protocol messages from the Server (port from `RENDERER_PORT` env or default 5050).
- **Render State Management**: `ofApp` updates in-memory scene/feed/surface state when new messages arrive (e.g., `loadSceneDefinition`).
- **Input Handlers**: MIDI via `ofxMidi` and audio via `ofxFft` modulate render parameters (brightness, scale, etc.).
- **Rendering Loop**: openFrameworks draw loop composites video feeds onto quads/meshes and outputs to the projector window.

## Deployment Notes
- Default ports: HTTP API on **8080**; renderer control on **5050**.
- Both binaries can run on the same machine (typical for projector hosts) or separate hosts if networked.
- The SQLite DB file is local to the Server process; no external DB service is required.
- Mount or preserve `./data/db` and `./data/assets` to keep state and media between runs.
