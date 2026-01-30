#pragma once

#include "model.h"
#include <boost/json.hpp>
#include <filesystem>

namespace json_loader {

namespace json = boost::json;

model::Game LoadGame(const std::filesystem::path& json_path);

}  // namespace json_loader