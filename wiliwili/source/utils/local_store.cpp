//
// 本地数据存储（未登录模式下的历史记录 / 稍后再看 / 我的收藏）
//

#include "utils/local_store.hpp"

#include <algorithm>
#include <fstream>

#include <borealis/core/logger.hpp>

#include "utils/config_helper.hpp"

LocalStore& LocalStore::instance() {
    static LocalStore store;
    return store;
}

LocalStore::LocalStore() {
    this->path = ProgramConfig::instance().getConfigDir() + "/local_data.json";
    this->load();
}

void LocalStore::load() {
    std::lock_guard<std::mutex> lock(this->mutex);
    this->historyList.clear();
    this->laterList.clear();
    this->favoriteList.clear();

    std::ifstream file(this->path);
    if (!file) return;
    try {
        nlohmann::json content;
        file >> content;
        if (content.contains("history")) content.at("history").get_to(this->historyList);
        if (content.contains("later")) content.at("later").get_to(this->laterList);
        if (content.contains("favorites")) content.at("favorites").get_to(this->favoriteList);
    } catch (const std::exception& e) {
        brls::Logger::error("LocalStore::load: {}", e.what());
    }
}

void LocalStore::save() {
    nlohmann::json content;
    content["history"]   = this->historyList;
    content["later"]     = this->laterList;
    content["favorites"] = this->favoriteList;

    std::ofstream file(this->path, std::ios::trunc);
    if (!file) {
        brls::Logger::error("LocalStore::save: cannot open {}", this->path);
        return;
    }
    file << content.dump(2);
}

void LocalStore::reload() { this->load(); }

std::vector<LocalVideoItem> LocalStore::getHistory() {
    std::lock_guard<std::mutex> lock(this->mutex);
    return this->historyList;
}

void LocalStore::addHistory(const LocalVideoItem& item) {
    if (item.bvid.empty()) return;
    std::lock_guard<std::mutex> lock(this->mutex);
    this->historyList.erase(std::remove_if(this->historyList.begin(), this->historyList.end(),
                                           [&item](const LocalVideoItem& i) { return i.bvid == item.bvid; }),
                            this->historyList.end());
    this->historyList.insert(this->historyList.begin(), item);
    if (this->historyList.size() > 200) this->historyList.resize(200);
    this->save();
}

void LocalStore::clearHistory() {
    std::lock_guard<std::mutex> lock(this->mutex);
    this->historyList.clear();
    this->save();
}

std::vector<LocalVideoItem> LocalStore::getLater() {
    std::lock_guard<std::mutex> lock(this->mutex);
    return this->laterList;
}

bool LocalStore::inLater(const std::string& bvid) {
    std::lock_guard<std::mutex> lock(this->mutex);
    return std::any_of(this->laterList.begin(), this->laterList.end(),
                       [&bvid](const LocalVideoItem& i) { return i.bvid == bvid; });
}

void LocalStore::addLater(const LocalVideoItem& item) {
    if (item.bvid.empty()) return;
    std::lock_guard<std::mutex> lock(this->mutex);
    this->laterList.erase(std::remove_if(this->laterList.begin(), this->laterList.end(),
                                         [&item](const LocalVideoItem& i) { return i.bvid == item.bvid; }),
                          this->laterList.end());
    this->laterList.insert(this->laterList.begin(), item);
    if (this->laterList.size() > 500) this->laterList.resize(500);
    this->save();
}

void LocalStore::removeLater(const std::string& bvid) {
    std::lock_guard<std::mutex> lock(this->mutex);
    this->laterList.erase(std::remove_if(this->laterList.begin(), this->laterList.end(),
                                         [&bvid](const LocalVideoItem& i) { return i.bvid == bvid; }),
                          this->laterList.end());
    this->save();
}

bool LocalStore::toggleLater(const LocalVideoItem& item) {
    if (this->inLater(item.bvid)) {
        this->removeLater(item.bvid);
        return false;
    }
    this->addLater(item);
    return true;
}

std::vector<LocalVideoItem> LocalStore::getFavorites() {
    std::lock_guard<std::mutex> lock(this->mutex);
    return this->favoriteList;
}

bool LocalStore::inFavorites(const std::string& bvid) {
    std::lock_guard<std::mutex> lock(this->mutex);
    return std::any_of(this->favoriteList.begin(), this->favoriteList.end(),
                       [&bvid](const LocalVideoItem& i) { return i.bvid == bvid; });
}

void LocalStore::addFavorite(const LocalVideoItem& item) {
    if (item.bvid.empty()) return;
    std::lock_guard<std::mutex> lock(this->mutex);
    this->favoriteList.erase(std::remove_if(this->favoriteList.begin(), this->favoriteList.end(),
                                            [&item](const LocalVideoItem& i) { return i.bvid == item.bvid; }),
                             this->favoriteList.end());
    this->favoriteList.insert(this->favoriteList.begin(), item);
    if (this->favoriteList.size() > 500) this->favoriteList.resize(500);
    this->save();
}

void LocalStore::removeFavorite(const std::string& bvid) {
    std::lock_guard<std::mutex> lock(this->mutex);
    this->favoriteList.erase(std::remove_if(this->favoriteList.begin(), this->favoriteList.end(),
                                            [&bvid](const LocalVideoItem& i) { return i.bvid == bvid; }),
                             this->favoriteList.end());
    this->save();
}

bool LocalStore::toggleFavorite(const LocalVideoItem& item) {
    if (this->inFavorites(item.bvid)) {
        this->removeFavorite(item.bvid);
        return false;
    }
    this->addFavorite(item);
    return true;
}
