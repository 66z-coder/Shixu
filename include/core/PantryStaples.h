#pragma once
#include <string>

namespace Shixu {
inline bool isPantryStaple(const std::string &name) {
    return name == "食用油" || name == "盐" || name == "食盐" || name == "糖" ||
           name == "白糖" || name == "酱油" || name == "醋" || name == "蚝油" || name == "料酒";
}
}
