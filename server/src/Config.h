#pragma once

#include "ServerApp.h"

namespace projection::server {

// Parses command-line arguments for server configuration. Supported options:
//   --db <path>    : Path to the SQLite database file.
//   --db=<path>
//   --web-root <path>     : Path to a static web root directory for the SPA.
//   --web-root=<path>
//   --port <port>          : HTTP port to listen on.
//   --port=<port>
//   --renderer-port <port> : Renderer TCP listen port.
//   --renderer-port=<port>
//   --verbose             : Enable verbose logging to stdout/stderr.
//
// Defaults:
//   databasePath = "./data/db/projection.db"
//   webRoot = ""
//   httpPort = 8080
//   rendererPort = 5050
//   verbose = false
ServerConfig parseServerConfig(int argc, char* argv[]);

}  // namespace projection::server
