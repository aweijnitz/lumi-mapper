#include <ofMain.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

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

  std::cerr << "[hello_app] signal " << signum << " received; requesting shutdown" << std::endl;
  if (!projection::renderer::requestWindowClose(ofGetWindowPtr(), std::cerr, "hello_app")) {
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
  std::string message{"Hello world"};
  bool verbose{false};
  int quitAfterSeconds{0};  // 0 = run until closed
};

Args parseArgs(int argc, char* argv[]) {
  Args args{};
  for (int i = 1; i < argc; ++i) {
    std::string arg(argv[i]);
    if ((arg == "--message" || arg == "-m") && i + 1 < argc) {
      args.message = argv[++i];
    } else if (arg.rfind("--message=", 0) == 0) {
      args.message = arg.substr(std::string("--message=").size());
    } else if (arg == "--verbose" || arg == "-v") {
      args.verbose = true;
    } else if (arg == "--quit-after" && i + 1 < argc) {
      args.quitAfterSeconds = std::stoi(argv[++i]);
    } else if (arg.rfind("--quit-after=", 0) == 0) {
      args.quitAfterSeconds = std::stoi(arg.substr(std::string("--quit-after=").size()));
    }
  }
  return args;
}

class HelloApp : public ofBaseApp {
 public:
  HelloApp(std::string message, bool verbose, int quitAfterSeconds)
      : message_(std::move(message)),
        verbose_(verbose),
        quitAfterSeconds_(quitAfterSeconds),
        start_(std::chrono::steady_clock::now()) {}

  void setup() override {
    if (verbose_) {
      std::cerr << "[hello_app] setup (message='" << message_ << "')" << std::endl;
    }
  }

  void update() override {
    if (quitAfterSeconds_ <= 0) {
      return;
    }
    const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - start_);
    if (elapsed.count() >= quitAfterSeconds_) {
      if (verbose_) {
        std::cerr << "[hello_app] quit-after reached (" << quitAfterSeconds_ << "s); exiting" << std::endl;
      }
      std::raise(SIGTERM);
    }
  }

  void draw() override {
    ofBackground(0, 0, 0);
    ofSetColor(255, 255, 255);
    ofDrawBitmapString(message_, 50, 50);
  }

 private:
  std::string message_;
  bool verbose_{false};
  int quitAfterSeconds_{0};
  std::chrono::steady_clock::time_point start_;
};

}  // namespace

int main(int argc, char* argv[]) {
  const auto args = parseArgs(argc, argv);
  if (args.verbose) {
    std::cerr << "[hello_app] verbose mode on" << std::endl;
  }
  ofSetupOpenGL(640, 480, OF_WINDOW);
  if (!projection::renderer::isWindowAvailable(ofGetWindowPtr(), std::cerr, "hello_app")) {
    std::cerr << "[hello_app] OpenGL initialization failed; window was not created" << std::endl;
    return 1;
  }
  installSignalHandlers();
  return ofRunApp(new HelloApp(args.message, args.verbose, args.quitAfterSeconds));
}
