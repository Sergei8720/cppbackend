#ifndef JSON_LOADER_H_
#define JSON_LOADER_H_

#include <filesystem>

#include "model.h"

namespace json_loader {

model::Game LoadGame(const std::filesystem::path& json_path);

}  // namespace json_loader

#endif  // JSON_LOADER_H_