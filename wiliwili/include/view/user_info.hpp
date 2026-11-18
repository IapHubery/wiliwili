//
// Created by fang on 2022/5/13.
//

#pragma once

#include <functional>
#include <nanovg.h>
#include <borealis/core/box.hpp>
#include <borealis/core/bind.hpp>

enum class InfoHintType {
    UP_FOLLOWING,          // 已关注
    UP_NOT_FOLLOWED,       //关注
    BANGUMI_FOLLOWING,     //已追番
    BANGUMI_NOT_FOLLOWED,  //追番
    CINEMA_FOLLOWING,      //已追剧
    CINEMA_NOT_FOLLOWED,   //追剧
    NONE                   // 看自己的视频不显示
};

namespace brls {
class Image;
class Label;
class Box;
};  // namespace brls
class SVGImage;

class UserInfoView : public brls::Box {
public:
    UserInfoView();

    ~UserInfoView() override;

    void setUserInfo(const std::string& avatar, const std::string& username, const std::string& misc);

    void setMainTextColor(NVGcolor color);

    static brls::View* create();

    brls::Image* getAvatar();

    brls::Label* getLabelName();

    brls::Label* getLabelMisc();

    brls::Box* getHintBox();

    void setHintType(InfoHintType type);

    brls::View* getNextFocus(brls::FocusDirection direction, brls::View* currentView) override;

    void setNextFocusOverride(std::function<brls::View*(brls::FocusDirection)> cb) {
        this->nextFocusOverride = std::move(cb);
    }

    NVGcolor hintBackground = {};
    std::function<brls::View*(brls::FocusDirection)> nextFocusOverride = nullptr;

private:
    BRLS_BIND(brls::Image, avatarView, "userinfo/avatar");
    BRLS_BIND(brls::Label, labelUsername, "userinfo/label/username");
    BRLS_BIND(brls::Label, labeMisc, "userinfo/label/misc");
    BRLS_BIND(brls::Label, labeHint, "userinfo/label/hint");
    BRLS_BIND(SVGImage, svgHint, "userinfo/svg/hint");
    BRLS_BIND(brls::Box, boxHint, "userinfo/box/hint");
};