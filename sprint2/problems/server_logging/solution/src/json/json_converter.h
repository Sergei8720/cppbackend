#pragma once

#include <string>

#include "model.h"

namespace json_converter {

std::string ConvertMapListToJson(const model::Game& game);
std::string ConvertMapToJson(const model::Map& map);
std::string CreateMapNotFoundResponse();
std::string CreateBadRequestResponse();
std::string CreatePageNotFoundResponse();

}  // namespace json_converter