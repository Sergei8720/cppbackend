#ifndef JSON_KEY_STORAGE_H_
#define JSON_KEY_STORAGE_H_

#include <string>

namespace json_keys {

constexpr const char* kMaps = "maps";
constexpr const char* kMapId = "id";
constexpr const char* kMapName = "name";

constexpr const char* kRoads = "roads";
constexpr const char* kRoadX0 = "x0";
constexpr const char* kRoadY0 = "y0";
constexpr const char* kRoadX1 = "x1";
constexpr const char* kRoadY1 = "y1";

constexpr const char* kBuildings = "buildings";
constexpr const char* kBuildingX = "x";
constexpr const char* kBuildingY = "y";
constexpr const char* kBuildingWidth = "w";
constexpr const char* kBuildingHeight = "h";

constexpr const char* kOffices = "offices";
constexpr const char* kOfficeId = "id";
constexpr const char* kOfficeX = "x";
constexpr const char* kOfficeY = "y";
constexpr const char* kOfficeOffsetX = "offsetX";
constexpr const char* kOfficeOffsetY = "offsetY";

constexpr const char* kResponseCode = "code";
constexpr const char* kResponseMessage = "message";

}  // namespace json_keys

#endif  // JSON_KEY_STORAGE_H_