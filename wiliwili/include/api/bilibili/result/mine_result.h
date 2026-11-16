//
// Created by fang on 2022/7/26.
//

#pragma once

#include "bilibili/util/json.hpp"
#include "bilibili/result/dynamic_video.h"
#include "bilibili/result/inbox_result.h"

namespace bilibili {

class QrLoginTokenResult {
public:
    std::string url;
    std::string oauthKey;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(QrLoginTokenResult, url, oauthKey);

class QrLoginTokenResultV2 {
public:
    std::string url;
    std::string qrcode_key;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(QrLoginTokenResultV2, url, qrcode_key);

enum class LoginInfo {
    SUCCESS           = 1,
    OAUTH_KEY_ERROR   = -1,
    OAUTH_KEY_TIMEOUT = -2,
    NEED_SCAN         = -4,
    NEED_CONFIRM      = -5,
    NONE              = -10
};

class QrLoginInfoResult {
public:
    bool status;
    LoginInfo data;
    std::string message;
};

inline void from_json(const nlohmann::json& nlohmann_json_j, QrLoginInfoResult& nlohmann_json_t) {
    if (nlohmann_json_j.at("data").is_number()) {
        nlohmann_json_t.data = LoginInfo(nlohmann_json_j.at("data"));
    } else {
        nlohmann_json_t.data = LoginInfo::SUCCESS;
    }
    NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_FROM, status));
}

class QrLoginInfoResultV2 {
public:
    bool status = false;
    LoginInfo data;
    std::string refresh_token;
};

inline void from_json(const nlohmann::json& nlohmann_json_j, QrLoginInfoResultV2& nlohmann_json_t) {
    switch (nlohmann_json_j.at("code").get<int>()) {
        case 0:
            nlohmann_json_t.status = true;
            nlohmann_json_t.data   = LoginInfo::SUCCESS;
            break;
        case 86101:
            nlohmann_json_t.data = LoginInfo::NEED_SCAN;
            break;
        case 86090:
            nlohmann_json_t.data = LoginInfo::NEED_CONFIRM;
            break;
        case 86038:
            nlohmann_json_t.data = LoginInfo::OAUTH_KEY_TIMEOUT;
            break;
        default:
            nlohmann_json_t.data = LoginInfo::OAUTH_KEY_ERROR;
    }
    NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_FROM, refresh_token));
}

/// 用户空间用户信息（/x/space/wbi/acc/info）
class UserSpaceInfo {
public:
    uint64_t mid = 0;
    std::string name;
    std::string face;
    std::string sign;
    int level = 0;
};
inline void from_json(const nlohmann::json& j, UserSpaceInfo& u) {
    u.mid   = j.value("mid", 0ULL);
    u.name  = j.value("name", std::string{});
    u.face  = j.value("face", std::string{});
    u.sign  = j.value("sign", std::string{});
    u.level = j.value("level", 0);
}

/// 用户名片（/x/web-interface/card），空间接口风控时的降级方案
class UserCardInfo {
public:
    uint64_t mid = 0;
    std::string name;
    std::string face;
    std::string sign;
};
inline void from_json(const nlohmann::json& j, UserCardInfo& u) {
    const auto& card = (j.contains("card") && j.at("card").is_object()) ? j.at("card") : j;
    u.mid            = card.value("mid", 0ULL);
    u.name           = card.value("name", std::string{});
    u.face           = card.value("face", std::string{});
    u.sign           = card.value("sign", std::string{});
}

/// 空间投稿（app 接口 /x/v2/space/archive/cursor）
class SpaceArchiveItem {
public:
    std::string title;
    std::string cover;
    std::string bvid;
    std::string author;
    std::string length;
    std::string param;
    uint64_t aid = 0;
    uint64_t cid = 0;
    int duration = 0;
    int play     = 0;
    int danmaku  = 0;
    int ctime    = 0;
};
inline void from_json(const nlohmann::json& j, SpaceArchiveItem& v) {
    v.title   = j.value("title", std::string{});
    v.cover   = j.value("cover", std::string{});
    v.bvid    = j.value("bvid", std::string{});
    v.author  = j.value("author", std::string{});
    v.length  = j.value("length", std::string{});
    v.param   = j.value("param", std::string{});
    v.duration = j.value("duration", 0);
    v.play     = j.value("play", 0);
    v.danmaku  = j.value("danmaku", 0);
    v.ctime    = j.value("ctime", 0);
    v.cid      = j.value("first_cid", 0ULL);
    if (v.aid == 0 && !v.param.empty()) {
        try {
            v.aid = std::stoull(v.param);
        } catch (...) {
        }
    }
}

class SpaceArchiveWrapper {
public:
    std::vector<SpaceArchiveItem> item;
    bool has_next = false;
    int count     = 0;
    int next      = 0;
};
inline void from_json(const nlohmann::json& j, SpaceArchiveWrapper& w) {
    if (j.contains("item") && j.at("item").is_array()) j.at("item").get_to(w.item);
    w.has_next = j.value("has_next", false);
    w.count    = j.value("count", 0);
    w.next     = j.value("next", 0);
}

class UserUploadedVideoResult {
public:
    int comment;
    int play;
    std::string pic;
    std::string description;
    std::string copyright;
    std::string title;
    unsigned int video_review;
    std::string author;
    uint64_t mid;
    unsigned int created;
    std::string length;
    uint64_t aid;
    std::string bvid;
    bool is_charging_arc; // 充电专属视频
};
inline void from_json(const nlohmann::json& nlohmann_json_j, UserUploadedVideoResult& nlohmann_json_t) {
    if (nlohmann_json_j.at("play").is_number()) {
        nlohmann_json_j.at("play").get_to(nlohmann_json_t.play);
    } else {
        nlohmann_json_t.play = -1;
    }
    if (nlohmann_json_j.contains("is_charging_arc")) {
        nlohmann_json_j.at("is_charging_arc").get_to(nlohmann_json_t.is_charging_arc);
    }
    NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_FROM, comment, pic, description, copyright, title,
                                             video_review, author, mid, created, length, aid, bvid));
}

typedef std::vector<UserUploadedVideoResult> UserUploadedVideoListResult;

class UserUploadedVideoPageResult {
public:
    unsigned int pn;
    unsigned int ps;
    unsigned int count;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(UserUploadedVideoPageResult, pn, ps, count);

class UserUploadedVideoResultWrapper {
public:
    UserUploadedVideoPageResult page;
    UserUploadedVideoListResult list;
};

inline void from_json(const nlohmann::json& nlohmann_json_j, UserUploadedVideoResultWrapper& nlohmann_json_t) {
    nlohmann_json_j.at("list").at("vlist").get_to(nlohmann_json_t.list);
    NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_FROM, page));
}

class UserDynamicVideoResultWrapper {
public:
    UserUploadedVideoPageResult page;
    DynamicVideoListResult archives;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(UserDynamicVideoResultWrapper, page, archives);

class UserDynamicCount {
public:
    size_t dyn_num;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(UserDynamicCount, dyn_num);

class UserRelationStat {
public:
    uint64_t mid, following, black, follower;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(UserRelationStat, mid, following, black, follower);

// 单个用户关系详情（来自 /x/relation）
class UserRelationDetail {
public:
    uint64_t mid{};
    int attribute{};  // 0: 未关注, 2: 已关注, 6: 互相关注, 128: 拉黑
    uint64_t mtime{};
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(UserRelationDetail, mid, attribute, mtime);

}  // namespace bilibili
