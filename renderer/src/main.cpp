#include "ofApp.h"

#include <ofMain.h>
#include <GLFW/glfw3.h>

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
  bool fullscreen;
  int display;
  int width;
  int height;
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

  // Display settings from environment
  bool fullscreen = false;
  if (const char* envFullscreen = std::getenv("RENDERER_FULLSCREEN")) {
    fullscreen = (std::string(envFullscreen) == "1" || std::string(envFullscreen) == "true");
  }
  int display = -1;  // -1 means use default/primary display
  if (const char* envDisplay = std::getenv("RENDERER_DISPLAY")) {
    try {
      display = std::stoi(envDisplay);
    } catch (...) {
      std::cerr << "Invalid RENDERER_DISPLAY value, using default" << std::endl;
    }
  }
  int width = 1280;
  int height = 720;
  if (const char* envWidth = std::getenv("RENDERER_WIDTH")) {
    try {
      width = std::stoi(envWidth);
    } catch (...) {
      std::cerr << "Invalid RENDERER_WIDTH value, defaulting to 1280" << std::endl;
    }
  }
  if (const char* envHeight = std::getenv("RENDERER_HEIGHT")) {
    try {
      height = std::stoi(envHeight);
    } catch (...) {
      std::cerr << "Invalid RENDERER_HEIGHT value, defaulting to 720" << std::endl;
    }
  }

  Args args{defaultHost(), defaultPort(), defaultName(), false, enableAudio, connectRetries, fullscreen, display, width, height};
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
    } else if (arg == "--fullscreen" || arg == "-f") {
      args.fullscreen = true;
    } else if (arg == "--windowed") {
      args.fullscreen = false;
    } else if (arg == "--display" && i + 1 < argc) {
      args.display = std::stoi(argv[++i]);
    } else if (arg.rfind("--display=", 0) == 0) {
      args.display = std::stoi(arg.substr(10));
    } else if (arg == "--width" && i + 1 < argc) {
      args.width = std::stoi(argv[++i]);
    } else if (arg.rfind("--width=", 0) == 0) {
      args.width = std::stoi(arg.substr(8));
    } else if (arg == "--height" && i + 1 < argc) {
      args.height = std::stoi(argv[++i]);
    } else if (arg.rfind("--height=", 0) == 0) {
      args.height = std::stoi(arg.substr(9));
    } else if (arg == "--resolution" && i + 1 < argc) {
      std::string res = argv[++i];
      auto xPos = res.find('x');
      if (xPos != std::string::npos) {
        args.width = std::stoi(res.substr(0, xPos));
        args.height = std::stoi(res.substr(xPos + 1));
      }
    } else if (arg.rfind("--resolution=", 0) == 0) {
      std::string res = arg.substr(13);
      auto xPos = res.find('x');
      if (xPos != std::string::npos) {
        args.width = std::stoi(res.substr(0, xPos));
        args.height = std::stoi(res.substr(xPos + 1));
      }
    } else if (arg == "--help" || arg == "-h") {
      std::cout << "Usage: renderer [options]\n"
                << "Options:\n"
                << "  --server-host=HOST    Server host (default: 127.0.0.1)\n"
                << "  --server-port=PORT    Server port (default: 5050)\n"
                << "  --name=NAME           Renderer name\n"
                << "  --fullscreen, -f      Run in fullscreen mode\n"
                << "  --windowed            Run in windowed mode (default)\n"
                << "  --display=N           Use display N (0 = primary, 1 = secondary, etc.)\n"
                << "  --width=W             Window width (default: 1280)\n"
                << "  --height=H            Window height (default: 720)\n"
                << "  --resolution=WxH      Set resolution (e.g., 1920x1080)\n"
                << "  --verbose             Enable verbose logging\n"
                << "  --disable-audio       Disable audio playback\n"
                << "  --connect-retries=N   Number of connection retries (default: 10)\n"
                << "\nEnvironment variables:\n"
                << "  RENDERER_HOST, RENDERER_PORT, RENDERER_NAME\n"
                << "  RENDERER_FULLSCREEN (1/true), RENDERER_DISPLAY\n"
                << "  RENDERER_WIDTH, RENDERER_HEIGHT\n"
                << "  RENDERER_DISABLE_AUDIO, RENDERER_CONNECT_RETRIES\n";
      std::exit(0);
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

  // Configure window mode
  ofWindowMode windowMode = args.fullscreen ? OF_FULLSCREEN : OF_WINDOW;

  if (args.verbose) {
    std::cerr << "[renderer] display mode: " << (args.fullscreen ? "fullscreen" : "windowed") << std::endl;
    std::cerr << "[renderer] resolution: " << args.width << "x" << args.height << std::endl;
    if (args.display >= 0) {
      std::cerr << "[renderer] target display: " << args.display << std::endl;
    }
  }

  // Setup OpenGL with specified resolution and mode
  ofSetupOpenGL(args.width, args.height, windowMode);

  if (!projection::renderer::isWindowAvailable(ofGetWindowPtr(), std::cerr, "renderer")) {
    std::cerr << "[renderer] OpenGL initialization failed; window was not created" << std::endl;
    return 1;
  }

  // Position window on the specified display if requested
  if (args.display >= 0) {
    // Get display info using GLFW (openFrameworks uses GLFW on macOS/Linux)
    int monitorCount = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);
    if (args.verbose) {
      std::cerr << "[renderer] detected " << monitorCount << " display(s)" << std::endl;
    }
    if (args.display < monitorCount && monitors != nullptr) {
      GLFWmonitor* targetMonitor = monitors[args.display];
      int monitorX, monitorY;
      glfwGetMonitorPos(targetMonitor, &monitorX, &monitorY);
      const GLFWvidmode* mode = glfwGetVideoMode(targetMonitor);
      if (args.verbose) {
        std::cerr << "[renderer] display " << args.display << " at position ("
                  << monitorX << ", " << monitorY << ") with resolution "
                  << mode->width << "x" << mode->height << std::endl;
      }
      // Move window to target display
      ofSetWindowPosition(monitorX, monitorY);
      // If fullscreen, resize to match the display resolution
      if (args.fullscreen) {
        ofSetWindowShape(mode->width, mode->height);
      }
    } else {
      std::cerr << "[renderer] warning: display " << args.display << " not found (only " << monitorCount << " available)" << std::endl;
    }
  }

  installSignalHandlers();
  return ofRunApp(new ofApp(args.host, args.port, args.name, args.connectRetries, args.verbose, args.enableAudio));
}
