//
// 本地数据存储（未登录模式下的历史记录 / 稍后再看 / 我的收藏）
//

#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

/// 本地保存的视频条目（历史记录 / 稍后再看 / 收藏 共用）
struct LocalVideoItem {
    std::string bvid;
    std::string title;
    std::string cover;
    std::string author;
    uint64_t aid     = 0;
    uint64_t cid     = 0;
    int duration     = 0;
    int progress     = 0;
    int64_t view_at  = 0;
};

inline void to_json(nlohmann::json& j, const LocalVideoItem& item) {
    j = nlohmann::json{{"bvid", item.bvid},
                       {"title", item.title},
                       {"cover", item.cover},
                       {"author", item.author},
                       {"aid", item.aid},
                       {"cid", item.cid},
                       {"duration", item.duration},
                       {"progress", item.progress},
                       {"view_at", item.view_at}};
}

inline void from_json(const nlohmann::json& j, LocalVideoItem& item) {
    auto read = [&j](const char* key, auto& out) {
        auto iter = j.find(key);
        if (iter != j.end() && !iter->is_null()) iter->get_to(out);
    };
    read("bvid", item.bvid);
    read("title", item.title);
    read("cover", item.cover);
    read("author", item.author);
    read("aid", item.aid);
    read("cid", item.cid);
    read("duration", item.duration);
    read("progress", item.progress);
    read("view_at", item.view_at);
}

/// 未登录时的本地数据存储，保存到配置目录下的 local_data.json
class LocalStore {
public:
    static LocalStore& instance();

    // 历史记录（按最近观看时间倒序）
    std::vector<LocalVideoItem> getHistory();
    void addHistory(const LocalVideoItem& item);
    void clearHistory();

    // 稍后再看
    std::vector<LocalVideoItem> getLater();
    bool inLater(const std::string& bvid);
    void addLater(const LocalVideoItem& item);
    void removeLater(const std::string& bvid);
    bool toggleLater(const LocalVideoItem& item);

    // 我的收藏（本地收藏夹）
    std::vector<LocalVideoItem> getFavorites();
    bool inFavorites(const std::string& bvid);
    void addFavorite(const LocalVideoItem& item);
    void removeFavorite(const std::string& bvid);
    bool toggleFavorite(const LocalVideoItem& item);

    // 磁盘上的数据发生变化（用于刷新列表）
    void reload();

private:
    LocalStore();
    ~LocalStore() = default;

    void load();
    void save();

    std::mutex mutex;
    std::string path;
    std::vector<LocalVideoItem> historyList;
    std::vector<LocalVideoItem> laterList;
    std::vector<LocalVideoItem> favoriteList;
};
