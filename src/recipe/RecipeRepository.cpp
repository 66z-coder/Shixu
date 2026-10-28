#include "RecipeRepository.h"

#include <algorithm>

namespace {

// 名称规范化：去掉首尾空白。中文名称按字节处理不受影响。
std::string normalizeName(const std::string& name)
{
    const std::string whitespace = " \t\r\n";
    const std::size_t begin = name.find_first_not_of(whitespace);
    if (begin == std::string::npos) {
        return {};
    }
    const std::size_t end = name.find_last_not_of(whitespace);
    return name.substr(begin, end - begin + 1);
}

} // namespace

RecipeRepository::RecipeRepository(std::vector<Recipe> recipes)
    : m_recipes(std::move(recipes))
{
    for (std::size_t i = 0; i < m_recipes.size(); ++i) {
        for (const RecipeIngredient& ingredient : m_recipes[i].ingredients) {
            m_invertedIndex[normalizeName(ingredient.foodName)].push_back(i);
        }
    }
}

const std::vector<Recipe>& RecipeRepository::all() const
{
    return m_recipes;
}

std::vector<std::size_t> RecipeRepository::findByIngredient(const std::string& foodName) const
{
    const auto it = m_invertedIndex.find(normalizeName(foodName));
    if (it == m_invertedIndex.end()) {
        return {};
    }
    return it->second;
}

const Recipe* RecipeRepository::findById(int id) const
{
    for (const Recipe& r : m_recipes) {
        if (r.id == id) {
            return &r;
        }
    }
    return nullptr;
}
