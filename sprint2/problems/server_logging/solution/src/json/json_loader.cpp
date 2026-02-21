#include "json_loader.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string_view>

#include "json_key_storage.h"
#include "logger.h"
#include "model_key_storage.h"

namespace json_loader {

using namespace std::literals;

namespace {

boost::json::value ReadFile(const std::filesystem::path& json_path) {
  std::ifstream file(json_path);
  if (!file.is_open()) {
    logware::ErrorLogData error_data;
    error_data.code = EXIT_FAILURE;
    error_data.text = "Error: Can't open file.";
    error_data.where = "ReadFile";
    BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage("error", error_data);
    std::exit(EXIT_FAILURE);
  }

  std::stringstream string_stream;
  string_stream << file.rdbuf();
  boost::json::value root = boost::json::parse(string_stream.str());
  return root;
}

}  // namespace

model::Game LoadGame(const std::filesystem::path& json_path) {
  model::Game game;
  boost::json::value json_value = ReadFile(json_path);
  std::vector<model::Map> maps =
      boost::json::value_to<std::vector<model::Map>>(
          json_value.as_object().at(model::kMaps));
  game.AddMaps(maps);
  return game;
}

}  // namespace json_loader