#include "tzbounds/timezones.h"

#include <algorithm>
#include <array>
#include <istream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "boost/iostreams/device/array.hpp"
#include "boost/iostreams/filter/gzip.hpp"
#include "boost/iostreams/filtering_streambuf.hpp"
#include "boost/json.hpp"

#include "rtree.h"
#include "tg.h"
#include "utl/verify.h"

#include "tzbounds_timezones.h"

namespace tzbounds {

constexpr auto const kResource = "combined-with-oceans-1970.json.gz";

struct rtree_deleter {
  void operator()(rtree* t) const { rtree_free(t); }
};

struct tg_deleter {
  void operator()(tg_geom* g) const { tg_geom_free(g); }
};

using rtree_ptr = std::unique_ptr<rtree, rtree_deleter>;
using tg_ptr = std::unique_ptr<tg_geom, tg_deleter>;

std::string inflate_gzip(std::string_view compressed) {
  auto const in =
      boost::iostreams::array_source{compressed.data(), compressed.size()};
  auto in_buf = boost::iostreams::filtering_istreambuf{};
  in_buf.push(boost::iostreams::gzip_decompressor{});
  in_buf.push(in);
  auto out = std::string{};
  out.assign(std::istreambuf_iterator<char>{&in_buf},
             std::istreambuf_iterator<char>{});
  return out;
}

std::optional<std::string> tzid(std::string_view extra) {
  try {
    auto const val = boost::json::parse(extra);
    auto const& obj = val.as_object();
    if (auto const it = obj.find("properties"); it != obj.end()) {
      auto const& props = it->value().as_object();
      if (auto const t = props.find("tzid"); t != props.end()) {
        return std::string{t->value().as_string()};
      }
    }
    return std::nullopt;
  } catch (...) {
    return std::nullopt;
  }
}

struct timezone_lookup::impl {
  struct feature {
    tg_geom const* geom_{nullptr};
    std::string tzid_{};
  };

  impl() {
    auto const res = tzbounds_timezones::get_resource(kResource);
    auto const compressed =
        std::string_view{reinterpret_cast<char const*>(res.ptr_), res.size_};
    auto const json = inflate_gzip(compressed);

    geom_.reset(
        tg_parse_geojsonn_ix(json.data(), json.size(),
                             static_cast<tg_index>(TG_YSTRIPES | TG_NATURAL)));
    if (geom_ == nullptr || tg_geom_error(geom_.get()) != nullptr ||
        !tg_geom_is_featurecollection(geom_.get())) {
      geom_.reset();
      return;
    }

    tree_.reset(rtree_new());
    utl::verify(tree_ != nullptr, "timezone rtree creation failed");

    features_.reserve(static_cast<std::size_t>(
        std::max(0, tg_geom_num_geometries(geom_.get()))));
    auto const n = tg_geom_num_geometries(geom_.get());
    for (auto i = 0; i != n; ++i) {
      auto const* feature_geom = tg_geom_geometry_at(geom_.get(), i);
      if (feature_geom == nullptr) {
        continue;
      }
      auto const* extra_json = tg_geom_extra_json(feature_geom);
      if (extra_json == nullptr) {
        continue;
      }
      auto const id = tzid(extra_json);
      if (!id.has_value()) {
        continue;
      }
      features_.push_back(feature{feature_geom, *id});
      auto const& feature = features_.back();
      auto const rect = tg_geom_rect(feature_geom);
      auto const min = std::array{rect.min.x, rect.min.y};
      auto const max = std::array{rect.max.x, rect.max.y};
      utl::verify(rtree_insert(tree_.get(), min.data(), max.data(), &feature),
                  "timezone rtree insert failed");
    }
  }

  struct search {
    double lon_{};
    double lat_{};
    std::optional<std::string_view> result_{};
  };

  std::optional<std::string_view> lookup(double const lon,
                                         double const lat) const {
    if (tree_ == nullptr) {
      return std::nullopt;
    }
    auto search_state = search{lon, lat};
    auto const point = std::array{lon, lat};
    rtree_search(
        tree_.get(), point.data(), point.data(),
        [](double const*, double const*, void const* data,
           void* udata) -> bool {
          auto* state = static_cast<search*>(udata);
          auto const* candidate = static_cast<feature const*>(data);
          if (tg_geom_intersects_xy(candidate->geom_, state->lon_,
                                    state->lat_)) {
            state->result_ = candidate->tzid_;
            return false;
          }
          return true;
        },
        &search_state);
    return search_state.result_;
  }

  tg_ptr geom_;
  rtree_ptr tree_;
  std::vector<feature> features_;
};

timezone_lookup::timezone_lookup() : impl_{std::make_unique<impl>()} {}

timezone_lookup::~timezone_lookup() = default;

std::optional<std::string_view> timezone_lookup::lookup(
    geo::latlng const p) const {
  return impl_->lookup(p.lng(), p.lat());
}

}  // namespace tzbounds
