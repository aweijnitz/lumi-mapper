#include "util/ShutdownUtils.h"

#include <sstream>

#include <catch2/catch_test_macros.hpp>

namespace {

constexpr const char* kTag = "test";

}  // namespace

TEST_CASE("ShutdownUtils logs when window is missing", "[shutdown]") {
  std::ostringstream log;

  const bool available = projection::renderer::isWindowAvailable(nullptr, log, kTag);

  REQUIRE(!available);
  REQUIRE(log.str().find("[test] window not initialized") != std::string::npos);
}

TEST_CASE("ShutdownUtils returns false when requesting close on null window", "[shutdown]") {
  std::ostringstream log;

  const bool closed = projection::renderer::requestWindowClose(nullptr, log, kTag);

  REQUIRE(!closed);
  REQUIRE(log.str().find("[test] window not initialized") != std::string::npos);
}
