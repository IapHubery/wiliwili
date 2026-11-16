//
// Created by fang on 2022/7/28.
//

#include "bilibili.h"
#include "presenter/mine_history.hpp"
#include "borealis/core/i18n.hpp"
#include "utils/config_helper.hpp"
#include "utils/local_store.hpp"

using namespace brls::literals;

namespace {

bilibili::HistoryVideoResult historyFromLocal(const LocalVideoItem& item) {
    bilibili::HistoryVideoResult result;
    result.title       = item.title;
    result.cover       = item.cover;
    result.author_name = item.author;
    result.view_at     = (int)item.view_at;
    result.duration    = item.duration;
    result.progress    = item.progress;
    result.live_status = 0;
    result.history.bvid     = item.bvid;
    result.history.cid      = item.cid;
    result.history.oid      = item.aid;
    result.history.epid     = 0;
    result.history.business = "archive";
    result.history.dt       = 0;
    return result;
}

}  // namespace

void MineHistoryRequest::onHistoryList(const bilibili::HistoryVideoResultWrapper &result) {}

void MineHistoryRequest::onError(const std::string &error) {}

void MineHistoryRequest::requestData(bool refresh) {
    CHECK_REQUEST
    if (refresh) this->cursor = bilibili::HistoryVideoListCursor();
    this->requestHistoryVideoList();
}

void MineHistoryRequest::requestHistoryVideoList() {
    std::string mid = ProgramConfig::instance().getUserID();
    if (mid.empty() || mid == "0") {
        // 未登录：使用本地历史记录
        bilibili::HistoryVideoResultWrapper result;
        result.cursor = bilibili::HistoryVideoListCursor();
        for (const auto& item : LocalStore::instance().getHistory()) {
            result.list.push_back(historyFromLocal(item));
        }
        this->onHistoryList(result);
        return;
    }
    CHECK_AND_SET_REQUEST
    BILI::get_my_history(
        cursor,
        [this](const bilibili::HistoryVideoResultWrapper &result) {
            this->onHistoryList(result);
            this->cursor = result.cursor;
            UNSET_REQUEST
        },
        [this](BILI_ERR) {
            this->onError(error);
            UNSET_REQUEST
        });
}