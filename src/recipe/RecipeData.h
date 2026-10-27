#pragma once

#include <vector>

#include "model/Recipe.h"

// C 模块内部：加载内置示例菜谱库。
// 当前为编译期内置数据（30 道中式家常菜），非公共接口。
// 注意：营养数值为估算值，参考常见食物成分表的常见数值，非精确实测。
std::vector<Recipe> loadBuiltinRecipes();
