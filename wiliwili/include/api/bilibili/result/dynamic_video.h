//
// Created by fang on 2022/8/18.
//

#pragma once

#include "bilibili/util/json.hpp"
#include "user_result.h"
#include "home_result.h"
#include "dynamic_article.h"

namespace bilibili {

class DynamicVideoResult {
public:
    uint64_t aid = 0;
    std::string bvid;
    std::string pic;
    std::string title;
    int duration = 0;
    int pubdate  = 0;
    UserSimpleResult owner;
    VideoSimpleStateResult stat;
};
inline void to_json(nlohmann::json& nlohmann_json_j, const DynamicVideoResult& nlohmann_json_t) {
    NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_TO, aid, bvid, pic, title, duration, pubdate, stat));
}
inline void from_json(const nlohmann::json& nlohmann_json_j, DynamicVideoResult& nlohmann_json_t) {
    if (nlohmann_json_j.contains("owner")) {
        nlohmann_json_j.at("owner").get_to(nlohmann_json_t.owner);
    } else if (nlohmann_json_j.contains("author")) {
        nlohmann_json_j.at("author").get_to(nlohmann_json_t.owner);
    }
    if (nlohmann_json_j.at("duration").is_number()) {
        nlohmann_json_j.at("duration").get_to(nlohmann_json_t.duration);
    }
    if (nlohmann_json_j.at("pubdate").is_number()) {
        nlohmann_json_j.at("pubdate").get_to(nlohmann_json_t.pubdate);
    }
    NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_FROM, aid, bvid, pic, title, stat));
}

typedef std::vector<DynamicVideoResult> DynamicVideoListResult;
typedef std::vector<DynamicArticleResult> DynamicArticleListResult;

template <typename Item>
class DynamicListResultWrapper {
public:
    Item items;
    bool has_more{};
    std::string offset;
    std::string update_baseline;
    unsigned int update_num{};
    unsigned int page{};
};
template <typename Item>
inline void from_json(const nlohmann::json& nlohmann_json_j, DynamicListResultWrapper<Item>& nlohmann_json_t) {
    if (nlohmann_json_j.contains("items") && nlohmann_json_j.at("items").is_array()) {
        nlohmann_json_j.at("items").get_to(nlohmann_json_t.items);
    }
    NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_FROM, has_more, offset, update_baseline, update_num));
}

class DynamicUpResult {
public:
    bool has_update;
    std::string face;
    std::string uname;
    std::string mid;
};
inline void to_json(nlohmann::json& j, const DynamicUpResult& u) {
    j = nlohmann::json{
        {"has_update", u.has_update}, {"face", u.face}, {"uname", u.uname}, {"mid", u.mid}};
}
inline void from_json(const nlohmann::json& j, DynamicUpResult& u) {
    if (j.contains("has_update") && j.at("has_update").is_boolean())
        u.has_update = j.at("has_update").get<bool>();
    if (j.contains("face") && j.at("face").is_string()) u.face = j.at("face").get<std::string>();
    if (j.contains("uname") && j.at("uname").is_string()) u.uname = j.at("uname").get<std::string>();
    // 关注列表接口返回的 mid 是字符串，其它接口可能是数字，这里两种都兼容
    auto it = j.find("mid");
    if (it != j.end() && !it->is_null()) {
        if (it->is_string()) {
            u.mid = it->get<std::string>();
        } else if (it->is_number()) {
            u.mid = std::to_string(it->get<uint64_t>());
        }
    }
}

typedef std::vector<DynamicUpResult> DynamicUpListResult;

class DynamicUpListResultWrapper {
public:
    DynamicUpListResult items;
};
inline void from_json(const nlohmann::json& nlohmann_json_j, DynamicUpListResultWrapper& nlohmann_json_t) {
    if (nlohmann_json_j.contains("items") && nlohmann_json_j.at("items").is_array()) {
        nlohmann_json_j.at("items").get_to(nlohmann_json_t.items);
    }
}
};  // namespace bilibili