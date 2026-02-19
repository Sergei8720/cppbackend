#ifndef JSON_CONVERTER_H_
#define JSON_CONVERTER_H_

#include "model.h"

#include <string>

namespace json_converter {

std::string ConvertMapListToJson(const model::Game& game);

std::string ConvertMapToJson(const model::Map& map);

std::string CreateMapNotFoundResponse();

std::string CreateBadRequestResponse();

std::string CreatePageNotFoundResponse();

}  // namespace json_converter

#endif  // JSON_CONVERTER_H_