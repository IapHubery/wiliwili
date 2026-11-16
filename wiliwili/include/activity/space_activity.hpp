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
#include "bilibili/result/inbox_result.h"

class SpaceActivity : public brls::Activity, public Presenter {
public:
    CONTENT_FROM_XML_RES("activity/space_activity.xml");

    explicit SpaceActivity(std::string mid);

    void onContentAvailable() override;

private:
    std::string mid;
    int page     = 1;
    bool hasMore = true;

    BRLS_BIND(brls::Image, avatar, "space/avatar");
    BRLS_BIND(brls::Label, nameLabel, "space/name");
    BRLS_BIND(brls::Label, descLabel, "space/desc");
    BRLS_BIND(RecyclingGrid, grid, "space/grid");

    void requestUserInfo();
    void requestVideos(bool refresh = false);
    void onVideos(const bilibili::UserUploadedVideoResultWrapper& result);
};
