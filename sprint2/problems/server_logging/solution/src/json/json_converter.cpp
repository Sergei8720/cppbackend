#include "json_converter.h"

#include <boost/json.hpp>
#include <boost/json/array.hpp>
#include <map>
#include <sstream>

#include "json_key_storage.h"
#include "model_key_storage.h"

namespace json_converter {

std::string ConvertMapListToJson(const model::Game& game) {
  boost::json::array maps_array;
  for (const auto& map : game.GetMaps()) {
    boost::json::value item = {{model::kMapId, *(map.GetId())},
                               {model::kMapName, map.GetName()}};
    maps_array.push_back(item);
  }
  return boost::json::serialize(maps_array);
}

std::string ConvertMapToJson(const model::Map& map) {
  return boost::json::serialize(boost::json::value_from(map));
}

std::string CreateMapNotFoundResponse() {
  boost::json::value msg = {
      {json_keys::kResponseCode, "mapNotFound"},
      {json_keys::kResponseMessage, "Map not found"}};
  return boost::json::serialize(msg);
}

std::string CreateBadRequestResponse() {
  boost::json::value msg = {
      {json_keys::kResponseCode, "badRequest"},
      {json_keys::kResponseMessage, "Bad request"}};
  return boost::json::serialize(msg);
}

std::string CreatePageNotFoundResponse() {
  boost::json::value msg = {
      {json_keys::kResponseCode, "pageNotFound"},
      {json_keys::kResponseMessage, "Page not found"}};
  return boost::json::serialize(msg);
}

}