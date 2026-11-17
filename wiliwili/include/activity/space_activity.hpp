//
// 用户空间（用户主页）
//

#pragma once

#include <string>

#include <borealis/core/activity.hpp>
#include <borealis/core/bind.hpp>
#include <borealis/views/image.hpp>
#include <borealis/views/label.hpp>

#include "view/recycling_grid.hpp"
#include "presenter/presenter.h"
#include "bilibili/result/mine_result.h"

class SpaceActivity : public brls::Activity, public Presenter {
public:
    explicit SpaceActivity(std::string mid);

    CONTENT_FROM_XML_RES("activity/space_activity.xml");

    void onContentAvailable() override;

    BRLS_BIND(brls::Box, headerBox, "space/header");
    BRLS_BIND(brls::Image, avatar, "space/avatar");
    BRLS_BIND(brls::Label, nameLabel, "space/name");
    BRLS_BIND(brls::Label, descLabel, "space/desc");
    BRLS_BIND(RecyclingGrid, grid, "space/grid");

    void requestUserInfo();
    void requestUserCard();
    void requestVideos(bool refresh = false);
    void requestVideosWeb(uint64_t uid, int requestPage);
    void onVideos(const bilibili::UserUploadedVideoListResult& list, bool append);
    void applyUser(const std::string& name, const std::string& face, const std::string& sign, uint64_t mid);
    void updateDesc();

private:
    std::string mid;
    std::string userSign;
    uint64_t follower  = 0;
    uint64_t following = 0;
    int page           = 1;
    bool hasMore       = true;
};
