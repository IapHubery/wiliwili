//
// Created by fang on 2023/1/3.
//

#include <utility>
#include <borealis/core/thread.hpp>
#include <borealis/core/touch/tap_gesture.hpp>
#include <borealis/views/applet_frame.hpp>
#include <borealis/views/dialog.hpp>
#include <borealis/views/image.hpp>

#include "activity/player_activity.hpp"
#include "fragment/player_collection.hpp"
#include "fragment/player_coin.hpp"
#include "fragment/player_single_comment.hpp"
#include "utils/config_helper.hpp"
#include "utils/dialog_helper.hpp"
#include "utils/number_helper.hpp"
#include "utils/local_store.hpp"
#include "utils/toast_helper.hpp"
#include "utils/image_helper.hpp"
#include "presenter/comment_related.hpp"
#include "utils/shortcut_helper.hpp"
#include "view/qr_image.hpp"
#include "view/video_view.hpp"
#include "view/grid_dropdown.hpp"
#include "view/subtitle_core.hpp"
#include "view/mpv_core.hpp"

#ifdef ABSOLUTE
#undef ABSOLUTE
#endif

class DataSourceCommentList : public RecyclingGridDataSource, public CommentAction {
public:
    DataSourceCommentList(bilibili::VideoCommentListResult result, uint64_t aid, int mode, std::function<void(void)> cb)
        : dataList(std::move(result)), aid(aid), commentMode(mode), switchModeCallback(cb) {}
    RecyclingGridItem* cellForRow(RecyclingGrid* recycler, size_t index) override {
        if (index == 0) {
            VideoCommentSort* item = (VideoCommentSort*)recycler->dequeueReusableCell("Sort");
            item->setHeight(30);
            item->setFocusable(false);
            if (commentMode == 3) {
                item->hintLabel->setText("wiliwili/player/comment_sort/top"_i18n);
                item->sortLabel->setText("wiliwili/player/comment_sort/sort_top"_i18n);
            } else {
                item->hintLabel->setText("wiliwili/player/comment_sort/new"_i18n);
                item->sortLabel->setText("wiliwili/player/comment_sort/sort_new"_i18n);
            }
            return item;
        }
        if (index == 1) {
            VideoCommentReply* item = (VideoCommentReply*)recycler->dequeueReusableCell("Reply");
            item->setHeight(40);
            return item;
        }

        //从缓存列表中取出 或者 新生成一个表单项
        VideoComment* item = (VideoComment*)recycler->dequeueReusableCell("Cell");

        item->setData(this->dataList[index - 2]);

        // 直接点击卡片上的点赞 / 点踩 / 回复图标，不再依赖操作菜单
        item->setLikeClickCallback([this, item, index]() { this->onCommentLike(item, index, 1); });
        item->setDislikeClickCallback([this, item, index]() { this->onCommentLike(item, index, 2); });
        item->setReplyClickCallback([this, recycler, index]() {
            if (!DialogHelper::checkLogin()) return;
            this->onItemSelected(recycler, index);
        });
        item->setNextDownResolver([this, recycler, index]() -> brls::View* {
            for (size_t i = index + 1; i < this->getItemCount(); i++) {
                auto* next = recycler->getGridItemByIndex(i);
                if (dynamic_cast<VideoComment*>(next)) return next;
            }
            return nullptr;
        });
        item->setPrevResolver([this, recycler, index]() -> brls::View* {
            for (size_t i = index; i > 0; i--) {
                auto* prev = recycler->getGridItemByIndex(i - 1);
                if (dynamic_cast<VideoComment*>(prev)) return prev;
            }
            return nullptr;
        });
        return item;
    }

    void onCommentLike(VideoComment* item, size_t index, size_t clicked) {
        if (index < 2 || index - 2 >= this->dataList.size()) return;
        if (!DialogHelper::checkLogin()) return;
        auto& itemData = this->dataList[index - 2];
        size_t action  = itemData.action == clicked ? 0 : clicked;
        bool isLike    = action == 1 || (action == 0 && itemData.action == 1);

        if (action == 1) {
            itemData.like++;
        } else if (itemData.action == 1) {
            itemData.like--;
        }
        itemData.action = action;
        item->setLiked(itemData.action);
        item->setLikeNum(itemData.like);

        if (isLike) {
            this->commentLike(std::to_string(itemData.oid), itemData.rpid, itemData.action, 1);
        } else {
            this->commentDislike(std::to_string(itemData.oid), itemData.rpid, itemData.action, 1);
        }
    }

    size_t getItemCount() override { return dataList.size() + 2; }

    void onItemSelected(RecyclingGrid* recycler, size_t index) override {
        if (index == 0) {
            if (switchModeCallback) switchModeCallback();
            return;
        }
        if (index == 1) {
            if (!DialogHelper::checkLogin()) return;
            // 回复评论
            brls::Application::getImeManager()->openForText(
                [this, recycler](const std::string& text) {
                    if (text.empty()) return;
                    this->commentReply(text, std::to_string(aid), 0, 0, 1,
                                       [this, recycler](const bilibili::VideoCommentAddResult& result) {
                                           this->dataList.insert(dataList.begin(), result.reply);
                                           recycler->reloadData();
                                       });
                },
                "wiliwili/player/single_comment/hint"_i18n, "", 500, "", 0);
            return;
        }

        auto* item = dynamic_cast<VideoComment*>(recycler->getGridItemByIndex(index));
        if (!item) return;

        auto* view = new PlayerSingleComment();
        view->setCommentData(dataList[index - 2], item->getY(), 1);
        auto container = new brls::AppletFrame(view);
        container->setHeaderVisibility(brls::Visibility::GONE);
        container->setFooterVisibility(brls::Visibility::GONE);
        container->setInFadeAnimation(true);
        brls::Application::pushActivity(new brls::Activity(container));

        view->likeStateEvent.subscribe([this, item, index](size_t value) {
            auto& itemData  = dataList[index - 2];
            itemData.action = value;
            item->setLiked(value);
        });
        view->likeNumEvent.subscribe([this, item, index](size_t value) {
            auto& itemData = dataList[index - 2];
            itemData.like  = value;
            item->setLikeNum(value);
        });
        view->replyNumEvent.subscribe([this, item, index](size_t value) {
            auto& itemData  = dataList[index - 2];
            itemData.rcount = value;
            item->setReplyNum(value);
        });
        view->deleteEvent.subscribe([this, recycler, index]() {
            dataList.erase(dataList.begin() + index - 2);
            recycler->reloadData();
            // 重新设置一下焦点到 recycler 的默认 cell （顶部）
            brls::Application::giveFocus(recycler);
        });
    }

    void appendData(const bilibili::VideoCommentListResult& data) {
        bool skip = false;
        for (auto& i : data) {
            skip = false;
            for (auto& j : this->dataList) {
                if (j.rpid == i.rpid) {
                    skip = true;
                    break;
                }
            }
            if (!skip) this->dataList.push_back(i);
        }
    }

    void clearData() override { this->dataList.clear(); }

private:
    bilibili::VideoCommentListResult dataList;
    uint64_t aid;
    int commentMode                              = 3;  // 2: 按时间；3: 按热度
    std::function<void(void)> switchModeCallback = nullptr;
};

class QualityCell : public RecyclingGridItem {
public:
    QualityCell() { this->inflateFromXMLRes("xml/views/player_quality_cell.xml"); }

    void setSelected(bool selected) {
        brls::Theme theme = brls::Application::getTheme();

        this->selected = selected;
        this->checkbox->setVisibility(selected ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
        this->title->setTextColor(selected ? theme["brls/list/listItem_value_color"] : theme["brls/text"]);
    }

    bool getSelected() { return this->selected; }

    BRLS_BIND(brls::Label, title, "cell/title");
    BRLS_BIND(brls::Box, loginLabel, "cell/login");
    BRLS_BIND(brls::Box, vipLabel, "cell/vip");
    BRLS_BIND(brls::CheckBox, checkbox, "cell/checkbox");

    static RecyclingGridItem* create() { return new GridRadioCell(); }

private:
    bool selected = false;
};

class QualityDataSource : public DataSourceDropdown {
public:
    QualityDataSource(bilibili::VideoUrlResult result, BaseDropdown* view)
        : DataSourceDropdown(view), data(std::move(result)) {
        login = !ProgramConfig::instance().getCSRF().empty();
    }

    RecyclingGridItem* cellForRow(RecyclingGrid* recycler, size_t index) override {
        QualityCell* item = (QualityCell*)recycler->dequeueReusableCell("Cell");

        int quality = data.accept_quality[index];

        item->loginLabel->setVisibility(brls::Visibility::GONE);
        item->vipLabel->setVisibility(brls::Visibility::GONE);

        if (quality > 80) {
            item->vipLabel->setVisibility(brls::Visibility::VISIBLE);
            // 免登录1080P（参考 PiliPlus try_look）：未登录最高可选 1080P，
            // 超过 1080P 的画质才需要登录
            if (!login) item->loginLabel->setVisibility(brls::Visibility::VISIBLE);
        }

        auto r = this->data.accept_description[index];
        item->title->setText(this->data.accept_description[index]);
        item->setSelected(index == dropdown->getSelected());
        return item;
    }

    size_t getItemCount() override { return (std::min)(data.accept_quality.size(), data.accept_description.size()); }

    void clearData() override {}

private:
    bilibili::VideoUrlResult data;
    bool login;
};

/// BasePlayerActivity

void BasePlayerActivity::onContentAvailable() { this->setCommonData(); }

void BasePlayerActivity::setCommonData() {
    // 视频评论
    recyclingGrid->registerCell("Cell", []() { return VideoComment::create(); });

    recyclingGrid->registerCell("Reply", []() { return VideoCommentReply::create(); });

    recyclingGrid->registerCell("Sort", []() { return VideoCommentSort::create(); });

    recyclingGrid->setDefaultCellFocus(1);

    recyclingGrid->registerAction("wiliwili/home/common/switch"_i18n, brls::ControllerButton::BUTTON_X,
                                  [this](brls::View* view) -> bool {
                                      this->setCommentMode();
                                      return true;
                                  });

    recyclingGrid->registerAction(ShortcutHelper::getRefresh(),
                                  [this](brls::View* view) -> bool {
                                      this->setCommentMode();
                                      return true;
                                  });

    // 切换右侧Tab
    this->registerAction(
        "上一项", brls::ControllerButton::BUTTON_LT,
        [this](brls::View* view) -> bool {
            tabFrame->focus2LastTab();
            return true;
        },
        true);
    this->registerAction(
        "上一项", brls::ControllerButton::BUTTON_LB,
        [this](brls::View* view) -> bool {
            tabFrame->focus2LastTab();
            return true;
        },
        true);

    this->registerAction(
        "下一项", brls::ControllerButton::BUTTON_RT,
        [this](brls::View* view) -> bool {
            tabFrame->focus2NextTab();
            return true;
        },
        true);
    this->registerAction(
        "下一项", brls::ControllerButton::BUTTON_RB,
        [this](brls::View* view) -> bool {
            tabFrame->focus2NextTab();
            return true;
        },
        true);

    this->registerAction(
        ShortcutHelper::getLast(),
        [this](brls::View* view) -> bool {
            tabFrame->focus2LastTab();
            return true;
        });
    this->registerAction(
        ShortcutHelper::getNext(),
        [this](brls::View* view) -> bool {
            tabFrame->focus2NextTab();
            return true;
        });
    video->registerCommonActions(this);

    // 调整清晰度
    this->registerAction("wiliwili/player/quality"_i18n, brls::ControllerButton::BUTTON_START,
                         [this](brls::View* view) -> bool {
                             this->setVideoQuality();
                             return true;
                         });

    this->btnQR->getParent()->addGestureRecognizer(new brls::TapGestureRecognizer(this->btnQR->getParent()));

    this->btnAgree->getParent()->addGestureRecognizer(new brls::TapGestureRecognizer(this->btnAgree->getParent()));

    this->btnCoin->getParent()->addGestureRecognizer(new brls::TapGestureRecognizer(this->btnCoin->getParent()));

    this->btnFavorite->getParent()->addGestureRecognizer(
        new brls::TapGestureRecognizer(this->btnFavorite->getParent()));

    this->videoUserInfo->addGestureRecognizer(new brls::TapGestureRecognizer(this->videoUserInfo));

    this->setRelationButton(false, false, false);

    eventSubscribeID = MPV_E->subscribe([this](MpvEventEnum event) {
        // 上一次报告历史记录的时间点
        static int64_t lastProgress = MPVCore::instance().video_progress;
        switch (event) {
            case MpvEventEnum::UPDATE_PROGRESS: {
                // 每15秒同步一次进度
                if (lastProgress + 15 < MPVCore::instance().video_progress) {
                    lastProgress = MPVCore::instance().video_progress;
                    this->reportCurrentProgress(lastProgress, MPVCore::instance().duration);
                } else if (MPVCore::instance().video_progress < lastProgress) {
                    // 当前播放时间小于上一次上传历史记录的时间点
                    // 发生于向前拖拽进度的时候，此时重置lastProgress的值
                    lastProgress = MPVCore::instance().video_progress;
                }
                // 检查视频链接是否有效
                auto timeNow = std::chrono::system_clock::now();
                if (timeNow > videoDeadline) {
                    // 设置视频加载后跳转的时间
                    setProgress(MPVCore::instance().video_progress);

                    // 暂停播放
                    MPVCore::instance().pause();

                    // 10s 后重新尝试
                    videoDeadline = timeNow + std::chrono::seconds(10);

                    // 有效期已过，重新请求视频链接
                    auto self = dynamic_cast<PlayerSeasonActivity*>(this);
                    if (self) {
                        this->requestSeasonVideoUrl(episodeResult.bvid, episodeResult.cid, false);
                    } else {
                        this->requestVideoUrl(videoDetailResult.bvid, videoDetailPage.cid, false);
                    }

                    //todo: 如果有选择的字幕加载对应的字幕
                }
                break;
            }
            case MpvEventEnum::END_OF_FILE:
                // 尝试自动加载下一分集
                // 如果当前最顶层是Dialog就放弃自动播放，因为有可能是用户点开了收藏或者投币对话框
                {
                    int64_t& progress = MPVCore::instance().video_progress;
                    int64_t& duration = MPVCore::instance().duration;
                    int clipEnd       = videoUrlResult.clipEnd;
                    auto seasonCustom = ProgramConfig::instance().getSeasonCustom(seasonInfo.season_id);
                    if (seasonCustom.custom_clip) {
                        clipEnd = duration - seasonCustom.clip_end;
                    }

                    // 播放到一半没网时也会触发EOF，这里简单判断一下结束播放时的播放条位置是否在片尾或视频结尾附近
                    if ((duration - progress > 5 || progress - duration > 5) && !(clipEnd > 0 && clipEnd - progress < 5)) {
                        brls::Logger::error("EOF: video: {} duration: {} clipEnd: {}", progress, duration, clipEnd);
                        return;
                    }
                    if (PLAYER_STRATEGY == PlayerStrategy::LOOP) {
                        MPVCore::instance().seek(0);
                        MPVCore::instance().resume();
                        return;
                    }
                    auto stack    = brls::Application::getActivitiesStack();
                    Activity* top = stack[stack.size() - 1];
                    if (!dynamic_cast<BasePlayerActivity*>(top) &&
                        !dynamic_cast<VideoView*>(top->getContentView()->getView("video"))) {
                        // 最顶层没有 video 组件，说明用户打开了评论或者其他菜单
                        // 在这种情况下不执行自动播放其他视频显示重播按钮
                        APP_E->fire(VideoView::REPLAY, nullptr);
                    } else if (PLAYER_STRATEGY == PlayerStrategy::NEXT || PLAYER_STRATEGY == PlayerStrategy::RCMD) {
                        this->onIndexChangeToNext();
                    } else {
                        // 对于其他情况，显示重播按钮
                        APP_E->fire(VideoView::REPLAY, nullptr);
                    }
                }
                break;
            case MpvEventEnum::RESTART:
                this->updateVideoLink();
                break;
            default:
                break;
        }
    });

    customEventSubscribeID = APP_E->subscribe([this](const std::string& event, void* data) {
        if (event == VideoView::QUALITY_CHANGE) {
            this->setVideoQuality();
        } else if (event == VideoView::SWITCH_TO_LAST) {
            // 历史播放进度储存在 SubtitleCore 中
            auto videoPage = SubtitleCore::instance().getSubtitleList();
            if (videoPage.last_play_cid == videoDetailPage.cid) {
                // 因为占用了切换弹幕的按键，所以在无效的情况下保持切换弹幕
                this->video->toggleDanmaku();
                return;
            }
            brls::Logger::debug("切换到历史播放进度：{}/{}", videoPage.last_play_cid, videoPage.last_play_time);
            for (auto& p : videoDetailResult.pages) {
                if (p.cid == videoPage.last_play_cid) {
                    this->onIndexChange(p.page - 1);
                    this->setProgress(videoPage.last_play_time / 1000);
                    break;
                }
            }
        } else if (event == "REQUEST_CAST_URL") {
            this->requestCastUrl();
        }
    });

    if (brls::Application::ORIGINAL_WINDOW_HEIGHT < 720) video->hideStatusLabel();

    video->hideOSDLockButton();
}

void BasePlayerActivity::showCollectionDialog(uint64_t id, int videoType) {
    if (!ProgramConfig::instance().hasLoginInfo()) {
        // 未登录：直接收藏到本地收藏夹，不请求官方接口
        LocalVideoItem item;
        item.aid      = id;
        item.bvid     = this->videoDetailResult.bvid;
        item.cid      = this->videoDetailPage.cid;
        item.title    = this->videoDetailResult.title;
        item.cover    = this->videoDetailResult.pic;
        item.author   = this->videoDetailResult.owner.name;
        item.duration = this->videoDetailResult.duration;
        item.view_at  = (int64_t)wiliwili::getUnixTime();
        if (item.bvid.empty()) {
            ToastHelper::showBottom("wiliwili/home/common/no_login"_i18n);
            return;
        }
        bool added = LocalStore::instance().toggleFavorite(item);
        ToastHelper::show(added ? "wiliwili/mine/fav_success"_i18n : "wiliwili/mine/fav_cancel"_i18n);
        return;
    }
    auto playerCollection = new PlayerCollection(id, videoType);
    auto dialog           = new brls::Dialog(playerCollection);
    dialog->addButton("wiliwili/home/common/save"_i18n, [this, id, videoType, playerCollection]() {
        this->addResource(id, videoType, playerCollection->isFavorite(), playerCollection->getAddCollectionList(),
                          playerCollection->getDeleteCollectionList());
    });
    playerCollection->registerAction(
        "", brls::ControllerButton::BUTTON_START,
        [this, id, videoType, playerCollection, dialog](...) {
            this->addResource(id, videoType, playerCollection->isFavorite(), playerCollection->getAddCollectionList(),
                              playerCollection->getDeleteCollectionList());
            dialog->dismiss();
            return true;
        },
        true);
    dialog->open();
}

void BasePlayerActivity::showCoinDialog(uint64_t aid) {
    if (!DialogHelper::checkLogin()) return;

    if (std::to_string(videoDetailResult.owner.mid) == ProgramConfig::instance().getUserID()) {
        DialogHelper::showDialog("wiliwili/player/coin/own"_i18n);
        return;
    }

    int coins = getCoinTolerate();
    if (coins <= 0) {
        DialogHelper::showDialog("wiliwili/player/coin/run_out"_i18n);
        return;
    }

    auto playerCoin = new PlayerCoin();
    if (coins == 1) playerCoin->hideTwoCoin();
    playerCoin->getSelectEvent()->subscribe(
        [this, playerCoin, aid](int value) { this->addCoin(aid, value, playerCoin->likeAtTheSameTime()); });
    auto dialog = new brls::Dialog(playerCoin);
    dialog->open();
}

void BasePlayerActivity::updateVideoLink() {
    // 设置视频加载后跳转的时间
    setProgress(MPVCore::instance().video_progress);

    // dash
    if (!this->videoUrlResult.dash.video.empty()) {
        // dash格式的视频无需重复请求视频链接，这里简单的设置清晰度即可
        videoUrlResult.quality = BasePlayerActivity::defaultQuality;
        this->onVideoPlayUrl(videoUrlResult);
        return;
    }

    // flv
    if (dynamic_cast<PlayerSeasonActivity*>(this)) {
        this->requestSeasonVideoUrl(episodeResult.bvid, episodeResult.cid);
    } else {
        this->requestVideoUrl(videoDetailResult.bvid, videoDetailPage.cid);
    }
}

void BasePlayerActivity::setVideoQuality() {
    if (this->videoUrlResult.accept_description.empty()) return;

    auto* dropdown = new BaseDropdown(
        "wiliwili/player/quality"_i18n,
        [this](int selected) {
            int code                           = this->videoUrlResult.accept_quality[selected];
#ifdef __PSV__
            if (code > 64) {
                code = 64;
            }
#endif
            // 免登录只能选择不超过 1080P 的画质，超过 1080P 仍需登录
            if (ProgramConfig::instance().getCSRF().empty() && code > 80) {
                ToastHelper::show("wiliwili/home/common/no_login"_i18n);
                return;
            }

            BasePlayerActivity::defaultQuality = code;
            ProgramConfig::instance().setSettingItem(SettingItem::VIDEO_QUALITY, code);

            this->updateVideoLink();
        },
        getQualityIndex());
    auto* recycler = dropdown->getRecyclingList();
    recycler->registerCell("Cell", []() { return new QualityCell(); });
    dropdown->setDataSource(new QualityDataSource(this->videoUrlResult, dropdown));
    dropdown->registerAction(
        "", brls::ControllerButton::BUTTON_START,
        [dropdown](...) {
            dropdown->dismiss();
            return true;
        },
        true);
    dropdown->registerAction(ShortcutHelper::getVideoQuality(), [dropdown](...) {
        dropdown->dismiss();
        return true;
    });

    // 因为触摸的问题 视频组件上开启新的 activity 需要同步执行
    // 不然在某些情况下焦点会错乱
    ASYNC_RETAIN
    brls::sync([ASYNC_TOKEN, dropdown]() {
        ASYNC_RELEASE
        brls::Application::pushActivity(new brls::Activity(dropdown));
    });
}

void BasePlayerActivity::setCommentMode() {
    this->recyclingGrid->estimatedRowHeight = 100;
    this->recyclingGrid->showSkeleton();
    tabFrame->focusTab(0);
    requestVideoComment(std::to_string(this->getAid()), 0, getVideoCommentMode() == 3 ? 2 : 3);
}

void BasePlayerActivity::onVideoPlayUrl(const bilibili::VideoUrlResult& result) {
    brls::Logger::debug("onVideoPlayUrl quality: {}", result.quality);

    if (result.accept_quality.empty() || result.accept_description.empty()) {
        // 通常是返回了其他报错信息, 比如验证码
        brls::Logger::error("onVideoPlayUrl: no video url available");
        auto dialog = new brls::Dialog("Error: No video url available");
        dialog->setCancelable(false);
        dialog->addButton("hints/ok"_i18n, []() {
            brls::sync([]() { brls::Application::popActivity(); });
        });
        dialog->open();
        return;
    }

    // 有效期 110 分钟
    videoDeadline = std::chrono::system_clock::now() + std::chrono::seconds(6600);

    // 未登录时把播放记录保存到本地（我的 - 历史记录）
    if (!ProgramConfig::instance().hasLoginInfo() && !this->videoDetailResult.bvid.empty()) {
        int savedProgress = 0;
        bool hasSaved     = false;
        for (const auto& h : LocalStore::instance().getHistory()) {
            if (h.type == "archive" && h.bvid == this->videoDetailResult.bvid) {
                savedProgress = h.progress;
                hasSaved      = true;
                break;
            }
        }
        LocalVideoItem item;
        item.type     = "archive";
        item.bvid     = this->videoDetailResult.bvid;
        item.aid      = this->videoDetailResult.aid;
        item.cid      = this->videoDetailPage.cid;
        item.title    = this->videoDetailResult.title;
        item.cover    = this->videoDetailResult.pic;
        item.author   = this->videoDetailResult.owner.name;
        item.duration = this->videoDetailResult.duration;
        // 已看完（-1）保留标记；未看完保留原进度，新视频从 0 开始
        item.progress = hasSaved ? savedProgress : 0;
        item.view_at  = (int64_t)wiliwili::getUnixTime();
        LocalStore::instance().addHistory(item);
    }

    // 获取预设的跳转位置；未登录时优先使用本地历史进度
    int start = this->getProgress();
    if (!ProgramConfig::instance().hasLoginInfo()) {
        start = 0;
        for (const auto& h : LocalStore::instance().getHistory()) {
            if (h.type == "archive" && h.bvid == this->videoDetailResult.bvid) {
                if (h.progress > 0 && h.progress != -1 &&
                    (h.duration <= 0 || h.progress < h.duration - 3)) {
                    start = h.progress;
                }
                break;
            }
        }
    }
    int end      = -1;
    int clipOpen = result.clipOpen, clipEnd = result.clipEnd;
    int time_sec = result.timelength / 1000;
    std::string customAspect;

    // 加载覆盖设置
    if (seasonInfo.season_id > 0) {
        auto seasonSetting = ProgramConfig::instance().getSeasonCustom(seasonInfo.season_id);
        customAspect       = seasonSetting.player_aspect;

        if (seasonSetting.custom_clip) {
            // 设置了自定义的片头片尾
            // 判断是否是正片
            for (auto& e : seasonInfo.episodes) {
                if (episodeResult.id == e.id) {
                    // 如果当前播放的是正片，则使用自定义的片头片尾
                    clipOpen = seasonSetting.clip_start;
                    if (clipOpen > time_sec - 5) clipOpen = 0;
                    clipEnd = time_sec - seasonSetting.clip_end;
                    if (seasonSetting.clip_end <= 0 || clipEnd < clipOpen) clipEnd = 0;
                    break;
                }
            }
        }
    }
    if (customAspect.empty()) {
        customAspect = ProgramConfig::instance().getSettingItem(SettingItem::PLAYER_ASPECT, std::string{"-1"});
    }
    MPVCore::instance().setAspect(customAspect);

    // 当要跳转的进度距离尾部只有 5s，就重新播放
    if (start > 0 && abs(time_sec - start) <= 5) start = 0;

    // 跳过片头片尾
    if (PLAYER_SKIP_OPENING_CREDITS) {
        start = std::max(start, clipOpen);
        if (clipEnd > 0) start = std::min(start, clipEnd);
        end = clipEnd;
        brls::Logger::debug("片头片尾: {}/{}", clipOpen, clipEnd);
    }

    // 针对用户上传的视频，尝试加载上一次播放的进度
    if (videoDetailPage.cid && start < 0) {
        auto data = SubtitleCore::instance().getSubtitleList();
        if (data.last_play_cid == videoDetailPage.cid && data.last_play_time > 0) {
            APP_E->fire(VideoView::LAST_TIME, (void*)&(data.last_play_time));
        }
    } else {
        // 设置为 POSITION_DISCARD 后，不会加载网络历史记录，而是直接使用 setProgress 指定的位置
        // 番剧视频或指定了进度的用户视频
        int64_t position = 0;
        APP_E->fire(VideoView::LAST_TIME, (void*)&(position));
        if (start > 0) {
            std::string hint = fmt::format("已为您定位至: {}", wiliwili::sec2Time(start));
            APP_E->fire(VideoView::HINT, (void*)hint.c_str());
        }
    }

    if (!result.dash.video.empty()) {
        // dash
        brls::Logger::debug("Video type: dash");

        // 以用户选择的清晰度（defaultQuality）为准来挑选 DASH 流。
        // 未登录且开启 try_look 时，服务器返回的 quality 字段可能低于 DASH 中实际包含的画质
        // （例如字段是 720P，但流里含 1080P），因此不能直接拿 quality 字段做目标；
        // 也不能按 DASH 最高流强制提升，否则用户选较低画质时会被自动改回高清。
        int desiredQualityBase = defaultQuality > 0 ? defaultQuality : result.quality;

        // 找到当前可用的清晰度
        for (const auto& i : result.dash.video) {
            int desiredQuality = desiredQualityBase;
            // 若设置了过高的清晰度, 自动切换到合适的清晰度, 默认为 128 (即无限制)
            if (i.height > i.width) {
                desiredQuality = std::min(desiredQuality, portraitQualityMax);
            } else {
                desiredQuality = std::min(desiredQuality, landscapeQualityMax);
            }

            if (desiredQuality >= i.id) {
                videoUrlResult.quality = i.id;
                break;
            }
        }

        // 找到当前清晰度下可用的视频
        std::vector<bilibili::DashMedia> codecs;
        for (const auto& i : result.dash.video) {
            if (i.id == videoUrlResult.quality) {
                codecs.emplace_back(i);
            }
        }
        if (codecs.empty()) {
            // 兜底：目标清晰度不在返回的流里（例如用户设置的清晰度低于视频最低画质），
            // 退回返回流中的最低清晰度，避免下面访问 codecs[0] 越界
            int lowestQuality = result.dash.video[0].id;
            for (const auto& i : result.dash.video) {
                lowestQuality = std::min(lowestQuality, i.id);
            }
            for (const auto& i : result.dash.video) {
                if (i.id == lowestQuality) {
                    codecs.emplace_back(i);
                }
            }
            videoUrlResult.quality = lowestQuality;
        }

        // 匹配当前设定的视频编码
        bilibili::DashMedia v = codecs[0];  // 默认是 AVC/H.264
        for (const auto& i : codecs) {
            if (BILI::VIDEO_CODEC == i.codecid) {
                v = i;
                break;
            }
        }

        // 将主音频和备份音频链接合并，当作不同的音轨传给播放器，可以实现在播放失败时自动切换
        std::vector<std::string> audios;
        if (!result.dash.audio.empty()) {
            // 选择音轨，支持杜比/无损优先和多级回退
            auto pickDolby = [&]() -> std::optional<bilibili::DashMedia> {
                if (result.dash.dolby_audio.empty()) return std::nullopt;
                bilibili::DashMedia best = result.dash.dolby_audio[0];
                for (auto& dm : result.dash.dolby_audio) if (dm.bandwidth > best.bandwidth) best = dm;
                return best;
            };
            auto pickFlac = [&]() -> std::optional<bilibili::DashMedia> {
                if (!result.dash.has_flac) return std::nullopt;
                return result.dash.flac_audio;
            };
            auto pickStandard = [&](int qid) -> std::optional<bilibili::DashMedia> {
                for (auto& i : result.dash.audio) if (i.id == qid) return i;
                return std::nullopt;
            };
            auto pickFirstAvailableStandard = [&]() -> std::optional<bilibili::DashMedia> {
                int candidates[] = {30280, 30232, 30216};
                for (int q : candidates) {
                    auto m = pickStandard(q);
                    if (m) return m;
                }
                // fallback to first item if none matched
                if (!result.dash.audio.empty()) return result.dash.audio[0];
                return std::nullopt;
            };

            bilibili::DashMedia a = result.dash.audio[0];
            bool selected = false;
            if (BILI::AUDIO_QUALITY == 30250) {
                // Dolby → Lossless → High → Medium → Low
                if (auto m = pickDolby()) { a = *m; selected = true; brls::Logger::debug("Picked Dolby audio (type {}), bw {}", result.dash.dolby_type, a.bandwidth);} else
                if (auto m = pickFlac()) { a = *m; selected = true; brls::Logger::debug("Picked FLAC audio, bw {}", a.bandwidth);} else
                if (auto m = pickFirstAvailableStandard()) { a = *m; selected = true; }
            } else if (BILI::AUDIO_QUALITY == 30251) {
                // Lossless → Dolby → High → Medium → Low
                if (auto m = pickFlac()) { a = *m; selected = true; brls::Logger::debug("Picked FLAC audio, bw {}", a.bandwidth);} else
                if (auto m = pickDolby()) { a = *m; selected = true; brls::Logger::debug("Picked Dolby audio (type {}), bw {}", result.dash.dolby_type, a.bandwidth);} else
                if (auto m = pickFirstAvailableStandard()) { a = *m; selected = true; }
            } else {
                // Try user-selected standard, then fallback High→Medium→Low
                if (auto m = pickStandard(BILI::AUDIO_QUALITY)) { a = *m; selected = true; }
                if (!selected) {
                    if (auto m = pickFirstAvailableStandard()) { a = *m; selected = true; }
                }
            }
            // 生成音频列表：只使用主音频地址。
            // 若把备用地址也作为 audio-file 传入，mpv 会同时打开多条音频流（各自带缓存），
            // 在弱网或高负载下更容易出现音频欠载，听感上就是间歇性的电流声 / 爆音；
            // 而且当前并没有切换备用音轨的入口，备用地址没有实际作用。
            audios.emplace_back(a.base_url);
            brls::Logger::debug("Dash quality: {}; video: {}; audio: {}", videoUrlResult.quality, v.codecid, a.id);
        }

        // 给播放器设置链接
        this->video->setUrl(v.base_url, start, end, audios);

        // 设置备份视频链接
        for (const auto& backup_url : v.backup_url) {
            this->video->setBackupUrl(backup_url, start, end, audios);
        }
    } else {
        // flv
        brls::Logger::debug("Video type: flv");
        if (result.durl.empty()) {
            brls::Logger::error("No media");
        } else if (result.durl.size() == 1) {
            this->video->setUrl(result.durl[0].url, start, end);
        } else {
            std::vector<EDLUrl> urls;
            urls.reserve(result.durl.size());
            for (auto& i : result.durl) {
                urls.emplace_back(i.url, i.length / 1000.0f);
            }
            this->video->setUrl(urls, start, end);
        }
    }

    // 设置mpv事件
    // 1.更新清晰度
    std::string quality = videoUrlResult.accept_description[getQualityIndex()];
    APP_E->fire(VideoView::SET_QUALITY, (void*)quality.c_str());
    // 2.绘制进度条标记点（例如：片头片尾）
    if (clipOpen > 0) {
        float data = clipOpen * 1.0f / time_sec;
        APP_E->fire(VideoView::CLIP_INFO, (void*)&data);
    }
    if (clipEnd > 0) {
        float data2 = clipEnd * 1.0f / time_sec;
        APP_E->fire(VideoView::CLIP_INFO, (void*)&data2);
    }
    // 3. 设置视频时长
    APP_E->fire(VideoView::REAL_DURATION, (void*)&time_sec);

    brls::Logger::debug("BasePlayerActivity::onVideoPlayUrl done");

    // 根据配置决定是否自动全屏
    if (ProgramConfig::instance().getBoolOption(SettingItem::PLAYER_AUTO_FULLSCREEN) &&
        !this->video->isFullscreen()) {
        this->video->setFullScreen(true);
    }
}

void BasePlayerActivity::onCommentInfo(const bilibili::VideoCommentResultWrapper& result) {
    auto* datasource = dynamic_cast<DataSourceCommentList*>(recyclingGrid->getDataSource());
    if (!datasource && result.requestIndex == 0) {
        // 第一页评论
        //整合置顶评论
        std::vector<bilibili::VideoCommentResult> comments(result.top_replies);
        comments.insert(comments.end(), result.replies.begin(), result.replies.end());
        // 为了加载骨架屏美观，设置为了100，在加载评论时手动修改回来
        // 这里限制的是评论的最大高度，实际评论高度还受评论组件的最大行数限制
        this->recyclingGrid->estimatedRowHeight = 600;
        this->recyclingGrid->setDataSource(new DataSourceCommentList(
            comments, this->getAid(), this->getVideoCommentMode(), [this]() { this->setCommentMode(); }));
        this->recyclingGrid->selectRowAt(comments.empty() ? 1 : 2, false);
        // 设置评论数量提示
        auto item = this->tabFrame->getTab("wiliwili/player/comment"_i18n);
        if (item) item->setSubtitle(wiliwili::num2w(result.cursor.all_count));
    } else if (datasource) {
        // 第N页评论
        if (!result.replies.empty()) {
            datasource->appendData(result.replies);
            recyclingGrid->notifyDataChanged();
        }
    } else {
        brls::Logger::error("onCommentInfo ds: {} index: {} end: {}", (bool)datasource, result.requestIndex,
                            result.cursor.is_end);
    }
}

void BasePlayerActivity::onRequestCommentError(const std::string& error) {
    brls::sync([this, error]() { this->recyclingGrid->setError(error); });
}

void BasePlayerActivity::onVideoOnlineCount(const bilibili::VideoOnlineTotal& result) {
    std::string count = result.total + "wiliwili/player/current"_i18n;
    this->videoPeopleLabel->setText(count);
    APP_E->fire(VideoView::SET_ONLINE_NUM, (void*)count.c_str());
}

void BasePlayerActivity::onVideoRelationInfo(const bilibili::VideoRelation& result) {
    brls::Logger::debug("onVideoRelationInfo: {} {} {}", result.like, result.coin, result.favorite);
    this->setRelationButton(result.like, result.coin, result.favorite);
}

void BasePlayerActivity::onHighlightProgress(const bilibili::VideoHighlightProgress& result) {
    brls::Logger::debug("highlight: {}/{}", result.step_sec, result.data.size());
    VideoHighlightData data{result.step_sec, result.data};
    APP_E->fire(VideoView::HIGHLIGHT_INFO, (void*)&data);
}

void BasePlayerActivity::setRelationButton(bool liked, bool coin, bool favorite) {
    if (liked) {
        btnAgree->setImageFromSVGRes("svg/bpx-svg-sprite-liked-active.svg");
    } else {
        btnAgree->setImageFromSVGRes("svg/bpx-svg-sprite-liked.svg");
    }
    if (coin) {
        btnCoin->setImageFromSVGRes("svg/bpx-svg-sprite-coin-active.svg");
    } else {
        btnCoin->setImageFromSVGRes("svg/bpx-svg-sprite-coin.svg");
    }
    if (favorite) {
        btnFavorite->setImageFromSVGRes("svg/bpx-svg-sprite-collection-active.svg");
    } else {
        btnFavorite->setImageFromSVGRes("svg/bpx-svg-sprite-collection.svg");
    }
}

void BasePlayerActivity::showErrorCover(const std::string& msg) {
    if (!this->video) return;
    if (!this->errorCoverBox) {
        float width  = this->video->getWidth() > 0 ? this->video->getWidth() : brls::Application::contentWidth;
        float height = this->video->getHeight() > 0 ? this->video->getHeight()
                                                   : brls::Application::contentHeight * 0.62f;

        this->errorCoverBox = new brls::Box();
        this->errorCoverBox->setPositionType(brls::PositionType::ABSOLUTE);
        this->errorCoverBox->setPositionTop(0);
        this->errorCoverBox->setPositionLeft(0);
        this->errorCoverBox->setWidth(width);
        this->errorCoverBox->setHeight(height);
        this->errorCoverBox->setBackgroundColor(nvgRGBA(0, 0, 0, 255));
        this->errorCoverBox->setAxis(brls::Axis::COLUMN);
        this->errorCoverBox->setJustifyContent(brls::JustifyContent::CENTER);
        this->errorCoverBox->setAlignItems(brls::AlignItems::CENTER);

        this->errorCoverImage = new brls::Image();
        this->errorCoverImage->setGrow(1);
        this->errorCoverImage->setScalingType(brls::ImageScalingType::FIT);

        this->errorCoverLabel = new brls::Label();
        this->errorCoverLabel->setFontSize(22);
        this->errorCoverLabel->setMarginTop(12);
        this->errorCoverLabel->setTextColor(nvgRGB(255, 255, 255));

        this->errorCoverBox->addView(this->errorCoverImage);
        this->errorCoverBox->addView(this->errorCoverLabel);
        this->video->addView(this->errorCoverBox);
    }

    this->errorCoverLabel->setText(msg);
    this->errorCoverBox->setVisibility(brls::Visibility::VISIBLE);
    std::string cover = this->getCoverUrl();
    if (!cover.empty()) {
        ImageHelper::with(this->errorCoverImage)->load(cover + ImageHelper::h_ext);
    }
}

void BasePlayerActivity::onError(const std::string& error) {
    if (!activityShown) return;
    MPVCore::instance().stop();
    MPVCore::instance().reset();
    bool forceClose = true;
    std::string msg = error;
    if (pystring::count(error, "87007") > 0 || pystring::count(error, "87008") > 0) {
        forceClose = false;
        msg        = "该视频为「充电」专属视频";
    } else if (pystring::count(error, "10403") > 0) {
        forceClose = false;
        msg        = "大会员专享限制";
    } else if (pystring::count(error, "404") > 0) {
        msg = "啥都木有";
    } else if (pystring::count(error, "62002") > 0) {
        msg = "稿件不可见";
    }

    // 大会员/付费/充电等限制：在播放区域显示状态，不弹窗，保证评论区可用
    bool inlineStatus = !forceClose || msg.find("大会员") != std::string::npos ||
                        msg.find("付费") != std::string::npos || msg.find("充电") != std::string::npos ||
                        msg.find("预览") != std::string::npos;
    if (inlineStatus) {
        this->video->hideOSD();
        this->showErrorCover(msg);
        ToastHelper::show(msg);
        return;
    }

    auto dialog = new brls::Dialog(msg);
    dialog->setCancelable(false);
    dialog->addButton("hints/ok"_i18n, [forceClose]() {
        if (forceClose) brls::sync([]() { brls::Application::popActivity(); });
    });
    dialog->open();
}

void BasePlayerActivity::willDisappear(bool resetState) {
    activityShown = false;
    brls::Activity::willDisappear(resetState);
}

void BasePlayerActivity::willAppear(bool resetState) {
    activityShown = true;
    brls::Activity::willAppear(resetState);
}

BasePlayerActivity::~BasePlayerActivity() {
    brls::Logger::debug("del BasePlayerActivity");
    // 取消监控mpv
    MPV_E->unsubscribe(eventSubscribeID);
    APP_E->unsubscribe(customEventSubscribeID);
    // 停止视频播放
    this->video->stop();
}