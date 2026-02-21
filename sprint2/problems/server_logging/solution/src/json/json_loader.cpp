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
    BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage(
        "error"sv,
        logware::ExceptionLogData(EXIT_FAILURE, "Error: Can't open file."sv,
                                  "write something here"sv));
    std::exit(1);
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