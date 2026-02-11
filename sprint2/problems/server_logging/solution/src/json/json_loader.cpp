#include "json_loader.h"
#include "logger.h"
#include "json_key_storage.h"
#include "model_key_storage.h"
#include <fstream>
#include <sstream>

namespace json_loader {

using namespace std::literals;

boost::json::value ReadFile(const std::filesystem::path& json_path) {
    std::ifstream file(json_path);
    if (!file.is_open()) {
        BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage("error",
            logware::ExceptionLogData(EXIT_FAILURE,
                "Error: Can't open file.",
                "json_loader::ReadFile"));
        std::exit(1);
    }
    
    std::stringstream ss;
    ss << file.rdbuf();
    return boost::json::parse(ss.str());
}

model::Game LoadGame(const std::filesystem::path& json_path) {
    model::Game game;
    boost::json::value json_val = ReadFile(json_path);
    std::vector<model::Map> maps = boost::json::value_to<std::vector<model::Map>>(
        json_val.as_object().at(model::MAPS));
    
    for (auto& map : maps) {
        game.AddMap(std::move(map));
    }
    
    return game;
}

}