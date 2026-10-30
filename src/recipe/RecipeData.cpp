#include "RecipeData.h"

#include <algorithm>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

namespace {

// ----------------------------------------------------------------------------
// 食材营养表（每 100 克含量，估算值）。
// 数值参考《中国食物成分表》等公开资料的常见取值取整，用于课程示例，
// 不是精确实测数据。字段顺序：能量(kcal) / 蛋白质(g) / 碳水化合物(g) / 脂肪(g)。
// ----------------------------------------------------------------------------
const std::unordered_map<std::string, NutritionInfo> kNutritionPer100g = {
    {"食用油", {899.0, 0.0, 0.0, 99.9}},
    {"鸡蛋", {144.0, 13.3, 2.8, 8.8}},
    {"番茄", {20.0, 0.9, 4.0, 0.2}},
    {"土豆", {81.0, 2.6, 17.8, 0.2}},
    {"青椒", {23.0, 1.0, 5.4, 0.3}},
    {"鸡胸肉", {133.0, 19.4, 2.5, 5.0}},
    {"花生", {574.0, 24.8, 21.7, 44.3}},
    {"黄瓜", {16.0, 0.8, 3.6, 0.2}},
    {"猪里脊", {155.0, 20.2, 0.7, 7.9}},
    {"猪肉末", {395.0, 13.2, 2.4, 37.0}},
    {"木耳", {27.0, 1.5, 6.0, 0.2}},   // 水发木耳
    {"胡萝卜", {39.0, 1.0, 8.8, 0.2}},
    {"豆腐", {98.0, 12.2, 2.0, 4.8}}, // 北豆腐
    {"五花肉", {349.0, 13.0, 1.0, 31.0}},
    {"鸡翅", {194.0, 17.4, 4.6, 11.8}},
    {"可乐", {43.0, 0.0, 11.2, 0.0}},
    {"西兰花", {36.0, 4.1, 4.3, 0.6}},
    {"大蒜", {126.0, 4.5, 27.6, 0.2}},
    {"小白菜", {15.0, 1.5, 2.4, 0.3}},
    {"牛肉", {106.0, 20.2, 1.2, 2.3}}, // 瘦牛肉
    {"洋葱", {39.0, 1.1, 9.0, 0.2}},
    {"米饭", {116.0, 2.6, 25.9, 0.3}},
    {"青豆", {111.0, 7.4, 21.2, 0.3}},
    {"面条", {280.0, 8.5, 59.5, 1.6}}, // 生面条（标准粉）
    {"油菜", {23.0, 1.8, 3.8, 0.5}},
    {"香菇", {26.0, 2.2, 5.2, 0.3}},   // 鲜香菇
    {"冬瓜", {12.0, 0.4, 2.6, 0.2}},
    {"排骨", {278.0, 16.7, 0.7, 23.1}}, // 猪小排
    {"紫菜", {250.0, 26.7, 44.1, 1.1}}, // 干紫菜
    {"韭菜", {26.0, 2.4, 4.6, 0.4}},
    {"芹菜", {16.0, 0.8, 3.9, 0.1}},
    {"香干", {152.0, 15.8, 5.1, 7.8}},
    {"油麦菜", {15.0, 1.4, 2.1, 0.4}},
    {"茄子", {23.0, 1.1, 4.9, 0.2}},
    {"豆角", {34.0, 2.0, 7.4, 0.3}},
    {"鲈鱼", {105.0, 18.6, 0.0, 3.4}},
    {"姜", {46.0, 1.3, 10.3, 0.6}},
    {"大葱", {30.0, 1.7, 6.5, 0.3}},
    {"虾", {101.0, 18.2, 3.9, 1.4}},
    {"菠菜", {28.0, 2.6, 4.5, 0.3}},
    {"西葫芦", {19.0, 0.8, 3.8, 0.2}},
};

// 构造一种食材用量；名称必须存在于营养表，否则视为数据错误立即暴露。
RecipeIngredient ing(const char* name, double grams)
{
    const auto it = kNutritionPer100g.find(name);
    if (it == kNutritionPer100g.end()) {
        throw std::out_of_range(std::string("内置菜谱数据缺少营养条目: ") + name);
    }
    RecipeIngredient ingredient;
    ingredient.foodName = name;
    ingredient.amountGrams = grams;
    ingredient.nutritionPer100g = it->second;
    return ingredient;
}

Recipe dish(int id, const char* name, const char* category, int minutes,
            std::vector<RecipeIngredient> ingredients,
            std::vector<std::string> steps,
            std::vector<std::string> tags)
{
    Recipe recipe;
    recipe.id = id;
    recipe.name = name;
    recipe.category = category;
    recipe.cookingTimeMinutes = minutes;
    recipe.ingredients = std::move(ingredients);
    recipe.steps = std::move(steps);
    recipe.tags = std::move(tags);
    // Apply conservative, transparent thresholds to the optional nutrition tags.
    // Values are based on the recipe's raw ingredient weights and estimates.
    static const std::set<std::string> vegetables = {
        "番茄", "青椒", "黄瓜", "胡萝卜", "西兰花", "小白菜", "洋葱", "青豆",
        "油菜", "冬瓜", "韭菜", "芹菜", "油麦菜", "茄子", "豆角", "菠菜", "西葫芦"};
    double weight = 0, protein = 0, vegetableWeight = 0;
    for (const auto &ingredient : recipe.ingredients) {
        weight += ingredient.amountGrams;
        protein += ingredient.amountGrams * ingredient.nutritionPer100g.protein / 100.0;
        if (vegetables.count(ingredient.foodName)) vegetableWeight += ingredient.amountGrams;
    }
    auto removeTag = [&](const char *tag) {
        recipe.tags.erase(std::remove(recipe.tags.begin(), recipe.tags.end(), tag), recipe.tags.end());
    };
    if (weight <= 0 || protein / weight < 0.10) removeTag("high_protein");
    if (weight <= 0 || vegetableWeight < 200 || vegetableWeight / weight < 0.5)
        removeTag("vegetable_rich");
    return recipe;
}

} // namespace

// ----------------------------------------------------------------------------
// 30 道示例菜谱。用量为 1 份（约 1–2 人食）的常见家庭分量，单位克。
// 盐、糖、酱油等家常调味品不列入 ingredients（不计入食材匹配）。
// ----------------------------------------------------------------------------
std::vector<Recipe> loadBuiltinRecipes()
{
    return {
        dish(1, "番茄炒蛋", "荤素", 18,
             {ing("番茄", 200), ing("鸡蛋", 150), ing("食用油", 15)},
             {"番茄切块，鸡蛋加少许盐打散。",
              "热锅倒油，先炒鸡蛋至凝固盛出。",
              "下番茄炒出汁，倒回鸡蛋翻匀即可。"},
             {"quick", "easy", "high_protein", "vegetable_rich", "appetizing"}),

        dish(2, "青椒土豆丝", "素菜", 15,
             {ing("土豆", 250), ing("青椒", 100), ing("食用油", 15)},
             {"土豆去皮切细丝，泡水去淀粉后沥干。",
              "热油下土豆丝大火快炒至断生。",
              "加青椒丝翻炒，调味出锅。"},
             {"quick", "easy"}),

        dish(3, "宫保鸡丁", "荤菜", 30,
             {ing("鸡胸肉", 250), ing("花生", 50), ing("黄瓜", 100), ing("食用油", 20)},
             {"鸡胸肉切丁腌制，黄瓜切丁。",
              "鸡丁滑炒至变色盛出。",
              "回锅加调味汁、黄瓜丁、花生炒匀。"},
             {"quick", "easy", "high_protein"}),

        dish(4, "鱼香肉丝", "荤菜", 25,
             {ing("猪里脊", 200), ing("木耳", 50), ing("胡萝卜", 80), ing("青椒", 50), ing("食用油", 20)},
             {"里脊切丝腌制，木耳、胡萝卜、青椒切丝。",
              "肉丝滑炒变色后盛出。",
              "下配菜与鱼香汁烧浓，回锅肉丝翻匀。"},
             {"quick", "easy", "high_protein", "vegetable_rich"}),

        dish(5, "麻婆豆腐", "荤素", 20,
             {ing("豆腐", 400), ing("猪肉末", 100), ing("食用油", 20)},
             {"豆腐切小块，焯水去腥。",
              "热油炒散肉末，加豆瓣酱炒出红油。",
              "下豆腐轻推烧入味，勾芡出锅。"},
             {"quick", "easy", "high_protein"}),

        dish(6, "红烧肉", "荤菜", 75,
             {ing("五花肉", 500), ing("食用油", 10)},
             {"五花肉切块焯水。",
              "小火煸炒出油，加酱油和糖色炒匀。",
              "加水没过肉块，小火炖约 40 分钟收汁。"},
             {"easy", "high_protein", "hearty", "special"}),

        dish(7, "可乐鸡翅", "荤菜", 40,
             {ing("鸡翅", 500), ing("可乐", 330), ing("食用油", 10)},
             {"鸡翅两面划口，焯水洗净。",
              "少油煎至两面金黄。",
              "倒入可乐没过鸡翅，中小火焖煮收汁。"},
             {"easy", "high_protein"}),

        dish(8, "蒜蓉西兰花", "素菜", 15,
             {ing("西兰花", 300), ing("大蒜", 20), ing("食用油", 15)},
             {"西兰花掰小朵焯水。",
              "热油爆香蒜末。",
              "下西兰花大火快炒，调味出锅。"},
             {"quick", "easy", "light", "vegetable_rich"}),

        dish(9, "清炒小白菜", "素菜", 10,
             {ing("小白菜", 300), ing("大蒜", 10), ing("食用油", 10)},
             {"小白菜洗净切段。",
              "热油爆香蒜末。",
              "大火快炒至断生，调味即可。"},
             {"quick", "easy", "light", "vegetable_rich"}),

        dish(10, "洋葱炒牛肉", "荤菜", 20,
             {ing("牛肉", 200), ing("洋葱", 150), ing("食用油", 15)},
             {"牛肉切片腌制，洋葱切丝。",
              "大火滑炒牛肉至变色盛出。",
              "炒软洋葱后回锅牛肉，调味翻匀。"},
             {"quick", "easy", "high_protein", "vegetable_rich"}),

        dish(11, "胡萝卜炒鸡蛋", "荤素", 15,
             {ing("胡萝卜", 200), ing("鸡蛋", 150), ing("食用油", 15)},
             {"胡萝卜切丝，鸡蛋打散。",
              "先炒鸡蛋盛出。",
              "炒软胡萝卜后回锅鸡蛋翻匀。"},
             {"quick", "easy", "high_protein", "vegetable_rich"}),

        dish(12, "蛋炒饭", "主食", 15,
             {ing("米饭", 300), ing("鸡蛋", 100), ing("胡萝卜", 50), ing("青豆", 50), ing("食用油", 15)},
             {"鸡蛋炒散盛出，胡萝卜切小丁。",
              "下胡萝卜丁、青豆炒熟。",
              "倒入米饭炒散，回锅鸡蛋调味炒匀。"},
             {"quick", "easy", "high_protein", "hearty"}),

        dish(13, "番茄鸡蛋面", "主食", 22,
             {ing("面条", 120), ing("番茄", 150), ing("鸡蛋", 100), ing("食用油", 10)},
             {"番茄切块炒出汁，加水煮开。",
              "下面条煮熟。",
              "淋入蛋液成蛋花，调味即可。"},
             {"quick", "easy", "noodle", "high_protein", "vegetable_rich", "hearty"}),

        dish(14, "青椒肉丝", "荤菜", 25,
             {ing("猪里脊", 200), ing("青椒", 150), ing("食用油", 15)},
             {"里脊切丝腌制，青椒切丝。",
              "肉丝滑炒变色盛出。",
              "炒青椒后回锅肉丝，调味翻匀。"},
             {"quick", "easy", "high_protein", "vegetable_rich"}),

        dish(15, "酸辣土豆丝", "素菜", 15,
             {ing("土豆", 300), ing("食用油", 15)},
             {"土豆切细丝泡水沥干。",
              "大火快炒至断生。",
              "加醋和少许辣椒调味出锅。"},
             {"quick", "easy"}),

        dish(16, "香菇油菜", "素菜", 15,
             {ing("香菇", 150), ing("油菜", 250), ing("食用油", 10)},
             {"香菇切片，油菜洗净。",
              "热油先炒香菇出香味。",
              "下油菜快炒，调味出锅。"},
             {"quick", "easy", "light", "vegetable_rich"}),

        dish(17, "土豆炖牛肉", "荤菜", 75,
             {ing("牛肉", 300), ing("土豆", 200), ing("胡萝卜", 100), ing("食用油", 15)},
             {"牛肉切块焯水。",
              "炒香后加水炖约 40 分钟。",
              "下土豆、胡萝卜再炖 15 分钟至软烂。"},
             {"easy", "high_protein", "hearty"}),

        dish(18, "冬瓜排骨汤", "汤类", 70,
             {ing("排骨", 400), ing("冬瓜", 300)},
             {"排骨焯水洗净。",
              "加水炖约 50 分钟。",
              "下冬瓜块再炖 15 分钟，调味即可。"},
             {"easy", "light", "high_protein", "vegetable_rich"}),

        dish(19, "紫菜蛋花汤", "汤类", 10,
             {ing("紫菜", 10), ing("鸡蛋", 100)},
             {"水烧开，放入紫菜。",
              "淋入打散的蛋液成蛋花。",
              "调味即可出锅。"},
             {"quick", "easy", "light"}),

        dish(20, "番茄蛋花汤", "汤类", 10,
             {ing("番茄", 200), ing("鸡蛋", 100)},
             {"番茄切块煮出味。",
              "淋入蛋液成蛋花。",
              "调味即可出锅。"},
             {"quick", "easy", "light", "vegetable_rich"}),

        dish(21, "黄瓜炒鸡蛋", "荤素", 15,
             {ing("黄瓜", 250), ing("鸡蛋", 150), ing("食用油", 15)},
             {"黄瓜切片，鸡蛋打散。",
              "先炒鸡蛋盛出。",
              "炒黄瓜片后回锅鸡蛋翻匀。"},
             {"quick", "easy", "high_protein", "vegetable_rich"}),

        dish(22, "韭菜炒鸡蛋", "荤素", 15,
             {ing("韭菜", 200), ing("鸡蛋", 150), ing("食用油", 15)},
             {"韭菜切段，鸡蛋打散。",
              "先炒鸡蛋盛出。",
              "炒韭菜后回锅鸡蛋翻匀。"},
             {"quick", "easy", "high_protein", "vegetable_rich"}),

        dish(23, "芹菜炒香干", "素菜", 15,
             {ing("芹菜", 250), ing("香干", 100), ing("食用油", 15)},
             {"芹菜切段，香干切条。",
              "热油先炒香干。",
              "下芹菜快炒，调味出锅。"},
             {"quick", "easy", "vegetable_rich"}),

        dish(24, "蒜蓉油麦菜", "素菜", 10,
             {ing("油麦菜", 300), ing("大蒜", 15), ing("食用油", 10)},
             {"油麦菜洗净切段。",
              "热油爆香蒜末。",
              "大火快炒至断生，调味即可。"},
             {"quick", "easy", "light", "vegetable_rich"}),

        dish(25, "红烧茄子", "素菜", 25,
             {ing("茄子", 400), ing("大蒜", 15), ing("食用油", 25)},
             {"茄子切滚刀块。",
              "多油煎软茄子盛出。",
              "爆香蒜末，回锅茄子加调味汁烧入味。"},
             {"quick", "easy", "vegetable_rich"}),

        dish(26, "干煸豆角", "荤素", 25,
             {ing("豆角", 300), ing("猪肉末", 50), ing("食用油", 20)},
             {"豆角掐段，中火煸至起皱熟透。",
              "下肉末炒散炒香。",
              "调味翻匀出锅。"},
             {"quick", "easy", "vegetable_rich"}),

        dish(27, "清蒸鲈鱼", "水产", 25,
             {ing("鲈鱼", 500), ing("姜", 10), ing("大葱", 10)},
             {"鲈鱼处理干净，两面划口，铺姜丝。",
              "水开后上锅蒸约 10 分钟。",
              "撒葱丝，淋热油和蒸鱼豉油。"},
             {"quick", "easy", "light", "high_protein"}),

        dish(28, "白灼虾", "水产", 15,
             {ing("虾", 400), ing("姜", 10)},
             {"虾剪须洗净。",
              "水加姜片烧开，下虾煮至变红。",
              "捞出装盘，蘸料食用。"},
             {"quick", "easy", "light", "high_protein"}),

        dish(29, "菠菜豆腐汤", "汤类", 15,
             {ing("菠菜", 200), ing("豆腐", 200)},
             {"菠菜焯水去草酸。",
              "水开下豆腐块煮 5 分钟。",
              "下菠菜煮开，调味即可。"},
             {"quick", "easy", "light", "high_protein", "vegetable_rich"}),

        dish(30, "西葫芦炒鸡蛋", "荤素", 15,
             {ing("西葫芦", 300), ing("鸡蛋", 150), ing("食用油", 15)},
             {"西葫芦切片，鸡蛋打散。",
              "先炒鸡蛋盛出。",
              "炒西葫芦后回锅鸡蛋翻匀。"},
             {"quick", "easy", "high_protein", "vegetable_rich"}),
    };
}
