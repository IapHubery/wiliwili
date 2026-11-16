//
// 用户空间（用户主页）
//

#include <utility>

#include <fmt/format.h>
#include <borealis/core/application.hpp>
#include <borealis/core/thread.hpp>
#include <borealis/views/image.hpp>

#include "activity/space_activity.hpp"
#include "bilibili.h"
#include "utils/activity_helper.hpp"
#include "utils/image_helper.hpp"
#include "utils/number_helper.hpp"
#include "view/video_card.hpp"

using namespace brls::literals;

class DataSourceSpaceVideoList : public RecyclingGridDataSource {
public:
    explicit DataSourceSpaceVideoList(bilibili::UserUploadedVideoListResult list) : list(std::move(list)) {}

    RecyclingGridItem* cellForRow(RecyclingGrid* recycler, size_t index) override {
        auto* item = (RecyclingGridItemVideoCard*)recycler->dequeueReusableCell("Cell");

        bilibili::UserUploadedVideoResult& v = this->list[index];
        item->setCard(v.pic + ImageHelper::h_ext, v.title, v.author, (int)v.created, v.play, (int)v.video_review,
                      v.length);
        // 直接把本条数据绑定到卡片上，避免索引错位
        auto bvid = v.bvid;
        item->registerClickAction([bvid](brls::View*) {
            if (!bvid.empty()) Intent::openBV(bvid);
            return true;
        });
        return item;
    }

    size_t getItemCount() override { return this->list.size(); }

    void onItemSelected(RecyclingGrid* recycler, size_t index) override {
        if (index >= this->list.size()) return;
        Intent::openBV(this->list[index].bvid);
    }

    void appendData(const bilibili::UserUploadedVideoListResult& data) {
        this->list.insert(this->list.end(), data.begin(), data.end());
    }

    void clearData() override { this->list.clear(); }

private:
    bilibili::UserUploadedVideoListResult list;
};

namespace {

bilibili::UserUploadedVideoResult spaceItemToUploaded(const bilibili::SpaceArchiveItem& item) {
    bilibili::UserUploadedVideoResult v;
    v.aid          = item.aid;
    v.bvid         = item.bvid;
    v.title        = item.title;
    v.pic          = item.cover;
    v.author       = item.author;
    v.created      = (unsigned int)item.ctime;
    v.play         = item.play;
    v.video_review = (unsigned int)item.danmaku;
    v.length       = item.length.empty() ? wiliwili::sec2Time(item.duration) : item.length;
    return v;
}

}  // namespace

SpaceActivity::SpaceActivity(std::string mid) : mid(std::move(mid)) {}

void SpaceActivity::onContentAvailable() {
    this->grid->registerCell("Cell", []() { return RecyclingGridItemVideoCard::create(); });
    this->grid->onNextPage([this]() { this->requestVideos(); });
    this->grid->setRefreshAction([this]() {
        this->page    = 1;
        this->hasMore = true;
        this->grid->showSkeleton();
        this->requestVideos(true);
    });

    this->requestUserInfo();
    this->grid->showSkeleton();
    this->requestVideos(true);
}

void SpaceActivity::updateDesc() {
    std::string desc;
    if (!this->userSign.empty()) desc = this->userSign + "\n";
    if (this->follower > 0 || this->following > 0) {
        desc += fmt::format("粉丝 {}  关注 {}", this->follower, this->following);
    }
    if (desc.empty()) desc = fmt::format("UID: {}", this->mid);
    this->descLabel->setText(desc);
}

void SpaceActivity::applyUser(const std::string& name, const std::string& face, const std::string& sign,
                              uint64_t mid) {
    if (mid != 0) this->mid = std::to_string(mid);
    if (!face.empty()) ImageHelper::with(this->avatar)->load(face + ImageHelper::face_large_ext);
    if (!name.empty()) this->nameLabel->setText(name);
    this->userSign = sign;
    this->updateDesc();
}

void SpaceActivity::requestUserInfo() {
    {
        ASYNC_RETAIN
        BILI::get_space_user_info(
            this->mid,
            [ASYNC_TOKEN](const bilibili::UserSpaceInfo& user) {
                brls::sync([ASYNC_TOKEN, user]() {
                    ASYNC_RELEASE
                    this->applyUser(user.name, user.face, user.sign, user.mid);
                });
            },
            [ASYNC_TOKEN](BILI_ERR) {
                brls::sync([ASYNC_TOKEN, error]() {
                    ASYNC_RELEASE
                    brls::Logger::warning("space info(wbi) failed: {}, fallback to card", error);
                    this->requestUserCard();
                });
            });
    }

    {
        ASYNC_RETAIN
        BILI::get_user_relation(
            this->mid,
            [ASYNC_TOKEN](const bilibili::UserRelationStat& stat) {
                brls::sync([ASYNC_TOKEN, stat]() {
                    ASYNC_RELEASE
                    this->follower  = stat.follower;
                    this->following = stat.following;
                    this->updateDesc();
                });
            },
            [](BILI_ERR) {});
    }
}

void SpaceActivity::requestUserCard() {
    ASYNC_RETAIN
    BILI::get_user_card(
        this->mid,
        [ASYNC_TOKEN](const bilibili::UserCardInfo& card) {
            brls::sync([ASYNC_TOKEN, card]() {
                ASYNC_RELEASE
                this->applyUser(card.name, card.face, card.sign, card.mid);
            });
        },
        [ASYNC_TOKEN](BILI_ERR) {
            brls::sync([ASYNC_TOKEN, error]() {
                ASYNC_RELEASE
                brls::Logger::error("space info(card) failed: {}", error);
            });
        });
}

void SpaceActivity::requestVideos(bool refresh) {
    if (refresh) {
        this->page    = 1;
        this->hasMore = true;
    }
    if (!this->hasMore) return;

    uint64_t uid = 0;
    try {
        uid = std::stoull(this->mid);
    } catch (const std::exception&) {
        brls::Logger::error("SpaceActivity: invalid mid {}", this->mid);
        return;
    }

    const int requestPage = this->page;
    {
        ASYNC_RETAIN
        BILI::get_space_videos(
            uid, requestPage, 30,
            [ASYNC_TOKEN, requestPage](const bilibili::SpaceArchiveWrapper& result) {
                brls::sync([ASYNC_TOKEN, requestPage, result]() {
                    ASYNC_RELEASE
                    if (requestPage != this->page) return;
                    bilibili::UserUploadedVideoListResult list;
                    list.reserve(result.item.size());
                    for (const auto& i : result.item) list.push_back(spaceItemToUploaded(i));
                    this->hasMore = result.has_next;
                    this->page    = requestPage + 1;
                    this->onVideos(list, this->page > 2);
                });
            },
            [ASYNC_TOKEN, uid, requestPage](BILI_ERR) {
                brls::sync([ASYNC_TOKEN, uid, requestPage, error, code]() {
                    ASYNC_RELEASE
                    brls::Logger::warning("space videos(app) failed: {} {}, fallback to web", code, error);
                    this->requestVideosWeb(uid, requestPage);
                });
            });
    }
}

void SpaceActivity::requestVideosWeb(uint64_t uid, int requestPage) {
    ASYNC_RETAIN
    BILI::get_user_videos(
        uid, requestPage, 30,
        [ASYNC_TOKEN, requestPage](const bilibili::UserUploadedVideoResultWrapper& result) {
            brls::sync([ASYNC_TOKEN, requestPage, result]() {
                ASYNC_RELEASE
                if (requestPage != this->page) return;
                this->hasMore = result.list.size() >= 30;
                this->page    = requestPage + 1;
                this->onVideos(result.list, this->page > 2);
            });
        },
        [ASYNC_TOKEN](BILI_ERR) {
            brls::sync([ASYNC_TOKEN, error]() {
                ASYNC_RELEASE
                brls::Application::notify(error);
                this->grid->setError(error);
            });
        });
}

void SpaceActivity::onVideos(const bilibili::UserUploadedVideoListResult& list, bool append) {
    auto* datasource = dynamic_cast<DataSourceSpaceVideoList*>(this->grid->getDataSource());
    if (append && datasource) {
        if (!list.empty()) {
            datasource->appendData(list);
            this->grid->notifyDataChanged();
        }
    } else {
        this->grid->setDataSource(new DataSourceSpaceVideoList(list));
    }
}
