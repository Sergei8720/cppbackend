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
    boost::json::object map_object;
    map_object[model::kMapId] = *(map.GetId());
    map_object[model::kMapName] = map.GetName();
    maps_array.push_back(map_object);
  }
  return boost::json::serialize(maps_array);
}

std::string ConvertMapToJson(const model::Map& map) {
  return boost::json::serialize(boost::json::value_from(map));
}

std::string CreateMapNotFoundResponse() {
  boost::json::object response;
  response[json_keys::kResponseCode] = "mapNotFound";
  response[json_keys::kResponseMessage] = "Map not found";
  return boost::json::serialize(response);
}

std::string CreateBadRequestResponse() {
  boost::json::object response;
  response[json_keys::kResponseCode] = "badRequest";
  response[json_keys::kResponseMessage] = "Bad request";
  return boost::json::serialize(response);
}

std::string CreatePageNotFoundResponse() {
  boost::json::object response;
  response[json_keys::kResponseCode] = "pageNotFound";
  response[json_keys::kResponseMessage] = "Page not found";
  return boost::json::serialize(response);
}

}  // namespace json_converter