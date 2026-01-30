#pragma once

#include "model.h"
#include <json/json.h>
#include <string>

namespace json_converter {

std::string ConvertMapListToJson(const model::Game& game);
std::string ConvertMapToJson(const model::Map& map);
std::string CreateMapNotFoundResponse();
std::string CreateBadRequestResponse();

}  // namespace json_converter