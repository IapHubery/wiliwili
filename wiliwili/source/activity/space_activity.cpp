//
// 用户空间（用户主页）
//

#include <utility>

#include <fmt/format.h>
#include <borealis/core/thread.hpp>
#include <borealis/views/image.hpp>

#include "activity/space_activity.hpp"
#include "bilibili.h"
#include "utils/activity_helper.hpp"
#include "utils/image_helper.hpp"
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
        return item;
    }

    size_t getItemCount() override { return this->list.size(); }

    void onItemSelected(RecyclingGrid* recycler, size_t index) override { Intent::openBV(this->list[index].bvid); }

    void appendData(const bilibili::UserUploadedVideoListResult& data) {
        this->list.insert(this->list.end(), data.begin(), data.end());
    }

    void clearData() override { this->list.clear(); }

private:
    bilibili::UserUploadedVideoListResult list;
};

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

void SpaceActivity::requestUserInfo() {
    {
        ASYNC_RETAIN
        BILI::get_space_user_info(
            this->mid,
            [ASYNC_TOKEN](const bilibili::UserSpaceInfo& user) {
                brls::sync([ASYNC_TOKEN, user]() {
                    ASYNC_RELEASE
                    if (!user.face.empty())
                        ImageHelper::with(this->avatar)->load(user.face + ImageHelper::face_large_ext);
                    this->nameLabel->setText(user.name);
                    this->userSign = user.sign;
                    this->updateDesc();
                });
            },
            [](BILI_ERR) {});
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

void SpaceActivity::requestVideos(bool refresh) {
    if (refresh) {
        this->page    = 1;
        this->hasMore = true;
    }
    if (!this->hasMore) return;

    uint64_t uid = 0;
    try {
        uid = std::stoull(this->mid);
    } catch (const std::exception& e) {
        brls::Logger::error("SpaceActivity: invalid mid {}", this->mid);
        return;
    }

    const int requestPage = this->page;
    ASYNC_RETAIN
    BILI::get_user_videos(
        uid, requestPage, 30,
        [ASYNC_TOKEN, requestPage](const bilibili::UserUploadedVideoResultWrapper& result) {
            brls::sync([ASYNC_TOKEN, requestPage, result]() {
                ASYNC_RELEASE
                if (requestPage != this->page) return;
                this->hasMore = result.list.size() >= 30;
                this->page    = requestPage + 1;
                this->onVideos(result);
            });
        },
        [ASYNC_TOKEN](BILI_ERR) {
            std::string msg = error;
            brls::sync([ASYNC_TOKEN, msg]() {
                ASYNC_RELEASE
                this->grid->setError(msg);
            });
        });
}

void SpaceActivity::onVideos(const bilibili::UserUploadedVideoResultWrapper& result) {
    auto* datasource = dynamic_cast<DataSourceSpaceVideoList*>(this->grid->getDataSource());
    if (datasource && this->page > 2) {
        datasource->appendData(result.list);
        this->grid->notifyDataChanged();
    } else {
        this->grid->setDataSource(new DataSourceSpaceVideoList(result.list));
    }
}
