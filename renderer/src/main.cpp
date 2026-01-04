#include "ofApp.h"

#include <ofMain.h>

#include <atomic>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
#include <unistd.h>

#include "util/ShutdownUtils.h"

namespace {
std::atomic<bool> shutdownRequested{false};

void safeSignalHandler(int signum) {
  if (shutdownRequested.exchange(true)) {
    return;
  }
  std::signal(SIGTERM, SIG_DFL);
  std::signal(SIGQUIT, SIG_DFL);
  std::signal(SIGINT, SIG_DFL);
  std::signal(SIGHUP, SIG_DFL);
  std::signal(SIGABRT, SIG_DFL);

  std::cerr << "[renderer] signal " << signum << " received; requesting shutdown" << std::endl;
  if (!projection::renderer::requestWindowClose(ofGetWindowPtr(), std::cerr, "renderer")) {
    std::_Exit(signum);
  }
}

void installSignalHandlers() {
  std::signal(SIGTERM, &safeSignalHandler);
  std::signal(SIGQUIT, &safeSignalHandler);
  std::signal(SIGINT, &safeSignalHandler);
  std::signal(SIGHUP, &safeSignalHandler);
  std::signal(SIGABRT, &safeSignalHandler);
}

struct Args {
  std::string host;
  int port;
  std::string name;
  bool verbose;
  bool enableAudio;
  int connectRetries;
};

std::string defaultHost() {
  const char* envHost = std::getenv("RENDERER_HOST");
  if (envHost && *envHost) {
    return envHost;
  }
  return "127.0.0.1";
}

int defaultPort() {
  const char* envPort = std::getenv("RENDERER_PORT");
  if (envPort) {
    try {
      return std::stoi(envPort);
    } catch (...) {
      std::cerr << "Invalid RENDERER_PORT value, defaulting to 5050" << std::endl;
    }
  }
  return 5050;
}

std::string defaultName() {
  const char* envName = std::getenv("RENDERER_NAME");
  if (envName && *envName) {
    return envName;
  }
  return "renderer-" + std::to_string(static_cast<long long>(::getpid()));
}

Args parseArgs(int argc, char* argv[]) {
  const char* disableAudioEnv = std::getenv("RENDERER_DISABLE_AUDIO");
  bool enableAudio = !(disableAudioEnv && *disableAudioEnv);
  int connectRetries = 10;
  if (const char* envRetries = std::getenv("RENDERER_CONNECT_RETRIES")) {
    try {
      connectRetries = std::stoi(envRetries);
    } catch (...) {
      std::cerr << "Invalid RENDERER_CONNECT_RETRIES value, defaulting to 10" << std::endl;
    }
  }
  Args args{defaultHost(), defaultPort(), defaultName(), false, enableAudio, connectRetries};
  for (int i = 1; i < argc; ++i) {
    std::string arg(argv[i]);
    if (arg == "--server-host" && i + 1 < argc) {
      args.host = argv[++i];
    } else if (arg.rfind("--server-host=", 0) == 0) {
      args.host = arg.substr(14);
    } else if (arg == "--server-port" && i + 1 < argc) {
      args.port = std::stoi(argv[++i]);
    } else if (arg.rfind("--server-port=", 0) == 0) {
      args.port = std::stoi(arg.substr(14));
    } else if (arg == "--name" && i + 1 < argc) {
      args.name = argv[++i];
    } else if (arg.rfind("--name=", 0) == 0) {
      args.name = arg.substr(7);
    } else if (arg == "--port" && i + 1 < argc) {
      args.port = std::stoi(argv[++i]);
    } else if (arg.rfind("--port=", 0) == 0) {
      args.port = std::stoi(arg.substr(7));
    } else if (arg == "--verbose") {
      args.verbose = true;
    } else if (arg == "--disable-audio" || arg == "--no-audio") {
      args.enableAudio = false;
    } else if (arg == "--connect-retries" && i + 1 < argc) {
      args.connectRetries = std::stoi(argv[++i]);
    } else if (arg.rfind("--connect-retries=", 0) == 0) {
      args.connectRetries = std::stoi(arg.substr(18));
    }
  }
  return args;
}
}  // namespace

int main(int argc, char* argv[]) {
  const auto args = parseArgs(argc, argv);
  if (args.verbose) {
    std::cerr << "[renderer] verbose mode on" << std::endl;
  }
  ofSetupOpenGL(640, 480, OF_WINDOW);
  if (!projection::renderer::isWindowAvailable(ofGetWindowPtr(), std::cerr, "renderer")) {
    std::cerr << "[renderer] OpenGL initialization failed; window was not created" << std::endl;
    return 1;
  }
  installSignalHandlers();
  return ofRunApp(new ofApp(args.host, args.port, args.name, args.connectRetries, args.verbose, args.enableAudio));
}
