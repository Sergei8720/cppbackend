#include "json_loader.h"
#include "middleware/logging/logger.h"
#include "json_key_storage.h"
#include "model_key_storage.h"
#include "loot_generator_config.h"
#include "json_model_converter.h"

#include <fstream>
#include <string_view>
#include <sstream>

namespace json_loader {

using namespace std::literals;


boost::json::value ReadFile(const std::filesystem::path& json_path) {
    std::ifstream file(json_path);
    if (!file.is_open()) {
        BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage("error"sv,
                                        logware::ExceptionLogData(EXIT_FAILURE,
                                            "Error: Can't open file."sv,
                                            "write something here"sv));
        throw OpenConfigFileOfModelException();
    }
    
    std::stringstream ss;
    ss << file.rdbuf();
    
    try {
        boost::json::value root = boost::json::parse(ss.str());
        return root;
    } catch (const boost::json::system_error& e) {
        BOOST_LOG_TRIVIAL(error) << "JSON parsing error in config file: " << e.what();
        throw;
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Unexpected error while parsing config file: " << e.what();
        throw;
    }
};

model::Game LoadGame(const std::filesystem::path& json_path) {
    model::Game game;
    
    try {
        boost::json::value jsonVal = ReadFile(json_path);
        
        model::LootGeneratorConfig lootGenCfg = 
            boost::json::value_to<model::LootGeneratorConfig>(jsonVal.as_object().at(model::LOOT_GENERATOR_CONFIG));
        game.AddLootGeneratorConfig(lootGenCfg);
        
        std::vector<model::Map> maps = boost::json::value_to< std::vector<model::Map> >(jsonVal.as_object().at(model::MAPS));
        game.AddMaps(maps);
        
        try {
            double default_dog_velocity = boost::json::value_to<double>(jsonVal.as_object().at(model::DEFAULT_DOG_VELOCITY));
            game.SetDefaultDogVelocity(default_dog_velocity);
        } catch(const std::out_of_range& e) {
        }
        
        try {
            double default_bag_capacity = boost::json::value_to<double>(jsonVal.as_object().at(model::DEFAULT_BAG_CAPACITY));
            game.SetDefaultBagCapacity(default_bag_capacity);
        } catch(const std::out_of_range& e) {
        }
        
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Failed to load game: " << e.what();
        throw;
    }
    
    return game;
};

}  // namespace json_loader