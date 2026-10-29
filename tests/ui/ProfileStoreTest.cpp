#include "core/UserProfile.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <iostream>
#include <stdexcept>
void check(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    try {
        QTemporaryDir dir;
        check(dir.isValid(), "temp");
        auto path = dir.filePath("profile.json");
        UserProfileStore store(path);
        check(store.error.isEmpty(), "default");
        auto profile = store.value;
        profile.nickname = QStringLiteral("小厨师");
        profile.avatar = 2;
        profile.policy = Shixu::Contracts::V1::InventoryPolicy::OnlyInventory;
        profile.excluded = {QStringLiteral("番茄")};
        int notices = 0;
        store.changed = [&] { ++notices; };
        check(store.save(profile) && notices == 1, "save notify");
        UserProfileStore restored(path);
        check(restored.value.nickname == profile.nickname && restored.value.excluded == profile.excluded &&
                  restored.value.policy == profile.policy,
              "restart");
        UserProfileStore blocked(dir.filePath("missing/profile.json"));
        check(!blocked.save(profile) && blocked.value.nickname != profile.nickname,
              "failed save leaves state");
        QFile file(path);
        check(file.open(QIODevice::WriteOnly), "fixture");
        file.write("broken");
        file.close();
        UserProfileStore corrupt(path);
        check(!corrupt.error.isEmpty() && !corrupt.save(profile), "corrupt cannot overwrite");
        check(file.open(QIODevice::ReadOnly) && file.readAll() == "broken", "preserved");
        std::cout << "Profile persistence checks passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what();
        return 1;
    }
}
