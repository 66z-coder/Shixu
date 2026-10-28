#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

#include "model/Recipe.h"

// C 模块内部：菜谱库访问机制，持有菜谱数据并维护“食材名称 -> 菜谱”倒排索引。
// 非公共接口，其他模块不应直接使用。
class RecipeRepository
{
public:
    explicit RecipeRepository(std::vector<Recipe> recipes);

    // 全部菜谱（只读）。
    const std::vector<Recipe>& all() const;

    // 查询包含指定食材的菜谱下标。名称按去首尾空白后精确匹配。
    std::vector<std::size_t> findByIngredient(const std::string& foodName) const;

    // 按菜谱 ID 查询；找不到返回 nullptr。
    const Recipe* findById(int id) const;

private:
    std::vector<Recipe> m_recipes;
    std::unordered_map<std::string, std::vector<std::size_t>> m_invertedIndex;
};
