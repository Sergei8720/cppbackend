#pragma once
#include <string_view>
#include <vector>
#include <string>

namespace rh_storage {

std::vector<std::string_view> SplitUrl(std::string_view str);

}  // namespace rh_storage