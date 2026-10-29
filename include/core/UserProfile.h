#pragma once
#include "contracts/CoreContracts.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStringList>
#include <functional>
struct UserProfile {
    QString nickname = QStringLiteral("食序用户");
    int avatar = 0;
    Shixu::Contracts::V1::InventoryPolicy policy = Shixu::Contracts::V1::InventoryPolicy::PreferInventory;
    QStringList excluded;
};
class UserProfileStore {
  public:
    UserProfile value;
    QString error;
    std::function<void()> changed;
    explicit UserProfileStore(QString path) : path_(std::move(path)) { load(); }
    bool save(const UserProfile &next) {
        if (!error.isEmpty())
            return false;
        if (next.nickname.trimmed().isEmpty() || next.nickname.size() > 30 || next.avatar < 0 ||
            next.avatar > 3 || int(next.policy) < 0 || int(next.policy) > 2)
            return false;
        QJsonArray excluded;
        for (const auto &name : next.excluded)
            excluded.append(name);
        QJsonObject o{{"version", 1},
                      {"nickname", next.nickname.trimmed()},
                      {"avatar", next.avatar},
                      {"policy", int(next.policy)},
                      {"excluded", excluded}};
        QSaveFile file(path_);
        file.setDirectWriteFallback(false);
        auto bytes = QJsonDocument(o).toJson();
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
            return false;
        value = next;
        value.nickname = value.nickname.trimmed();
        if (changed)
            changed();
        return true;
    }

  private:
    QString path_;
    void load() {
        if (!QFile::exists(path_))
            return;
        QFile file(path_);
        if (!file.open(QIODevice::ReadOnly)) {
            error = QStringLiteral("个人资料无法读取，暂不可保存。");
            return;
        }
        QJsonParseError parse;
        auto doc = QJsonDocument::fromJson(file.readAll(), &parse);
        auto o = doc.object();
        if (file.error() != QFileDevice::NoError || parse.error != QJsonParseError::NoError ||
            !doc.isObject() || o["version"].toInt() != 1 || !o["nickname"].isString() ||
            o["nickname"].toString().trimmed().isEmpty() || o["nickname"].toString().size() > 30 ||
            !o["avatar"].isDouble() || o["avatar"].toInt(-1) < 0 || o["avatar"].toInt() > 3 ||
            !o["policy"].isDouble() || o["policy"].toInt(-1) < 0 || o["policy"].toInt() > 2 ||
            !o["excluded"].isArray()) {
            error = QStringLiteral("个人资料已损坏，请保留原文件；暂不可保存或推荐。");
            return;
        }
        for (const auto &item : o["excluded"].toArray()) {
            if (!item.isString() || item.toString().isEmpty()) {
                error = QStringLiteral("避开食材记录无效，暂不可保存或推荐。");
                return;
            }
            value.excluded.append(item.toString());
        }
        value.nickname = o["nickname"].toString();
        value.avatar = o["avatar"].toInt();
        value.policy = static_cast<Shixu::Contracts::V1::InventoryPolicy>(o["policy"].toInt());
    }
};
