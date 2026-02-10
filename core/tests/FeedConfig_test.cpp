#include "projection/core/Feed.h"

#include <catch2/catch_test_macros.hpp>

using projection::core::AssetId;
using projection::core::Feed;
using projection::core::FeedId;
using projection::core::FeedSettings;
using projection::core::PanDirection;
using projection::core::ProjectId;
using projection::core::makeFeed;

TEST_CASE("Feed settings store variant selection and pan defaults", "[core][feed][settings]") {
    FeedSettings settings;
    settings.variantPath = "/assets/demo_low.mp4";
    settings.monochrome = true;
    settings.panDirection = PanDirection::PingPong;
    settings.panDurationSeconds = 30.0f;
    settings.visiblePortion = 0.5f;

    Feed feed = makeFeed(ProjectId{"project-1"}, FeedId{"10"}, "Demo", AssetId{"asset-1"}, settings);

    REQUIRE(feed.getAssetId() == AssetId{"asset-1"});
    REQUIRE(feed.getSettings() == settings);
}

TEST_CASE("Feed settings default values are stable", "[core][feed][settings]") {
    Feed feed = makeFeed(ProjectId{"project-1"}, FeedId{"11"}, "Default", AssetId{"asset-2"});

    REQUIRE(feed.getSettings().variantPath.empty());
    REQUIRE(feed.getSettings().monochrome == false);
    REQUIRE(feed.getSettings().panDirection == PanDirection::LeftToRight);
    REQUIRE(feed.getSettings().panDurationSeconds == 120.0f);
    REQUIRE(feed.getSettings().visiblePortion == 0.6f);
}

