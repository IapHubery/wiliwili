//
// Created by 贾海峰 on 2023/7/7.
//

#include "bilibili.h"
#include "presenter/mine_later.hpp"
#include "borealis/core/i18n.hpp"
#include "utils/config_helper.hpp"
#include "utils/local_store.hpp"

using namespace brls::literals;

void MineLaterRequest::onWatchLaterList(const bilibili::WatchLaterListWrapper& result) {}

void MineLaterRequest::onError(const std::string& error) {}

void MineLaterRequest::requestData(bool refresh) { this->requestWatchLaterList(); }

void MineLaterRequest::requestWatchLaterList() {
    auto mid = ProgramConfig::instance().getUserID();
    if (mid.empty() || mid == "0") {
        // 未登录：使用本地稍后再看列表
        bilibili::WatchLaterListWrapper result;
        auto list = LocalStore::instance().getLater();
        result.count = (int)list.size();
        for (const auto& item : list) {
            bilibili::WatchLaterItem v;
            v.aid          = item.aid;
            v.cid          = item.cid;
            v.bvid         = item.bvid;
            v.title        = item.title;
            v.pic          = item.cover;
            v.duration     = item.duration;
            v.owner.name   = item.author;
            v.stat.view    = 0;
            v.stat.danmaku = 0;
            result.list.push_back(v);
        }
        this->onWatchLaterList(result);
        return;
    }
    BILI::getWatchLater([this](const auto& result) { this->onWatchLaterList(result); },
                        [this](BILI_ERR) { this->onError(error); });
}