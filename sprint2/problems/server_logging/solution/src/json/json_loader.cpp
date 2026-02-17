#include "json_loader.h"
#include "logger.h"

#include <fstream>
#include <sstream>
#include <cstdlib>

namespace json_loader {

namespace {

boost::json::value ReadFile(const std::filesystem::path& json_path) {
  std::ifstream file(json_path);
  
  if (!file.is_open()) {
    BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage(
      "error",
      logware::ExceptionLogData(EXIT_FAILURE, "Can't open file.", ""));
    std::exit(EXIT_FAILURE);
  }
  
  std::stringstream buffer;
  buffer << file.rdbuf();
  return boost::json::parse(buffer.str());
}

}  // namespace

model::Game LoadGame(const std::filesystem::path& json_path) {
  model::Game game;
  boost::json::value json_value = ReadFile(json_path);
  
  std::vector<model::Map> maps = boost::json::value_to<std::vector<model::Map>>(
    json_value.as_object().at(model::MAPS));
  
  game.AddMaps(maps);
  return game;
}

}  // namespace json_loader