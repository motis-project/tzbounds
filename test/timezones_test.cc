#include "tzbounds/timezones.h"

#include "gtest/gtest.h"

namespace tzbounds {

TEST(tzbounds, lookup_timezone) {
  auto lookup = timezone_lookup{};

  EXPECT_EQ("Europe/Berlin", lookup.lookup({52.520008, 13.404954}));
  EXPECT_EQ("America/New_York", lookup.lookup({40.712776, -74.005974}));
}

}  // namespace tzbounds
