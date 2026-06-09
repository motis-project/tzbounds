#pragma once

#include <memory>
#include <optional>
#include <string_view>

#include "geo/latlng.h"

namespace tzbounds {

struct timezone_lookup {
  timezone_lookup();
  timezone_lookup(timezone_lookup const&) = delete;
  timezone_lookup& operator=(timezone_lookup const&) = delete;
  timezone_lookup(timezone_lookup&&) = delete;
  timezone_lookup& operator=(timezone_lookup&&) = delete;
  ~timezone_lookup();

  std::optional<std::string_view> lookup(geo::latlng) const;

  struct impl;
  std::unique_ptr<impl> impl_;
};

}  // namespace tzbounds
