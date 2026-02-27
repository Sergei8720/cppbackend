#pragma once
#include <string>
#include <vector>
#include "model.h"
#include <boost/json.hpp>

namespace json_converter {

std::string ConvertMapsToJson(const std::vector<model::Map>& maps);
std::string ConvertMapToJson(const model::Map& map);

} // namespace json_converter