//
// Created by fang on 2022/7/30.
//

#include "presenter/mine_collection.hpp"
#include "utils/config_helper.hpp"
#include "utils/local_store.hpp"
#include "utils/number_helper.hpp"
#include "borealis/core/logger.hpp"
#include "bilibili.h"
#include "borealis/core/i18n.hpp"

using namespace brls::literals;

MineCollectionRequest::MineCollectionRequest() = default;

void MineCollectionRequest::onCollectionList(const bilibili::CollectionListResultWrapper &result) {}

void MineCollectionRequest::onError(const std::string &error) {}

void MineCollectionRequest::requestData(bool refresh) {
    CHECK_REQUEST
    if (refresh) {
        index   = 1;
        hasMore = true;
    }

    if (hasMore) {
        auto mid = ProgramConfig::instance().getUserID();
        if (mid.empty() || mid == "0") {
            // 未登录：本地收藏夹
            hasMore = false;
            bilibili::CollectionListResultWrapper result;
            result.index    = 1;
            result.has_more = false;
            result.count    = 1;

            auto favorites = LocalStore::instance().getFavorites();
            bilibili::CollectionResult folder;
            folder.id          = 0;
            folder.title       = "wiliwili/mine/local_favorites"_i18n;
            folder.media_count = (unsigned int)favorites.size();
            folder.cover       = favorites.empty() ? "" : favorites.front().cover;
            folder.ctime       = (unsigned int)wiliwili::getUnixTime();
            folder.upper.name  = "wiliwili/mine/local_favorites"_i18n;
            result.list.push_back(folder);

            this->onCollectionList(result);
            return;
        }
        this->requestCollectionList(mid, index);
    }
}

void MineCollectionRequest::requestCollectionList(std::string &mid, int i, int num) {
    CHECK_AND_SET_REQUEST
    BILI::get_my_collection_list(
        mid, i, num, requestType,
        [this](const bilibili::CollectionListResultWrapper &result) {
            if (index != result.index) {
                brls::Logger::error(
                    "MineCollectionRequest::requestCollectionList: request: "
                    "{}, got: {}",
                    index, result.index);
                return;
            }
            hasMore = result.has_more;
            index   = result.index + 1;
            this->onCollectionList(result);
            UNSET_REQUEST
        },
        [this](BILI_ERR) {
            this->onError(error);
            UNSET_REQUEST
        });
}
void MineCollectionRequest::setRequestType(int type) { requestType = type; }

int MineCollectionRequest::getRequestType() { return requestType; }
