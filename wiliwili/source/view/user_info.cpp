//
// Created by fang on 2023/5/18.
//

#include <nanovg.h>
#include <borealis/core/box.hpp>
#include <borealis/core/application.hpp>
#include <borealis/views/label.hpp>

#include "view/user_info.hpp"
#include "view/svg_image.hpp"
#include "utils/image_helper.hpp"

UserInfoView::UserInfoView() {
    this->inflateFromXMLRes("xml/views/user_info.xml");
    this->registerColorXMLAttribute("mainTextColor", [this](NVGcolor value) { this->setMainTextColor(value); });

    // 关注按钮支持键盘 / 手柄聚焦，聚焦时高亮
    this->boxHint->setFocusable(true);
    this->boxHint->getFocusEvent()->subscribe([this](bool focused) {
        bool notFollowed = this->hintType == InfoHintType::UP_NOT_FOLLOWED ||
                           this->hintType == InfoHintType::BANGUMI_NOT_FOLLOWED ||
                           this->hintType == InfoHintType::CINEMA_NOT_FOLLOWED;
        this->boxHint->setBackgroundColor(
            focused ? (notFollowed ? nvgRGB(230, 90, 130) : nvgRGB(0, 161, 214)) : this->hintBackground);
        this->labeHint->setTextColor(focused ? nvgRGB(255, 255, 255) : this->hintTextColor);
    });
}

UserInfoView::~UserInfoView() { ImageHelper::clear(this->avatarView); }

void UserInfoView::setUserInfo(const std::string& avatar, const std::string& username, const std::string& misc) {
    this->labeMisc->setText(misc);

    if (username.empty()) {
        this->labelUsername->setHeight(30);
    } else {
        this->labelUsername->setHeight(brls::View::AUTO);
        this->labelUsername->setText(username);
    }

    if (avatar.empty()) {
        this->avatarView->getParent()->setVisibility(brls::Visibility::GONE);
    } else {
        this->avatarView->getParent()->setVisibility(brls::Visibility::VISIBLE);
        ImageHelper::with(this->avatarView)->load(avatar);
    }
}

void UserInfoView::setMainTextColor(NVGcolor color) { this->labelUsername->setTextColor(color); }

brls::View* UserInfoView::create() { return new UserInfoView(); }

brls::Image* UserInfoView::getAvatar() { return this->avatarView; }

brls::Label* UserInfoView::getLabelName() { return this->labelUsername; }

brls::Label* UserInfoView::getLabelMisc() { return this->labeMisc; }

brls::Box* UserInfoView::getHintBox() { return this->boxHint; }

brls::View* UserInfoView::getNextFocus(brls::FocusDirection direction, brls::View* currentView) {
    if (this->nextFocusOverride) {
        if (auto* view = this->nextFocusOverride(direction)) return view;
    }
    if (direction == brls::FocusDirection::RIGHT && this->boxHint->getVisibility() == brls::Visibility::VISIBLE) {
        return this->boxHint;
    }
    return brls::Box::getNextFocus(direction, currentView);
}

void UserInfoView::setHintType(InfoHintType type) {
    this->hintType = type;
    if (type == InfoHintType::NONE) {
        this->boxHint->setVisibility(brls::Visibility::GONE);
        return;
    }

    this->boxHint->setVisibility(brls::Visibility::VISIBLE);
    auto theme = brls::Application::getTheme();

    switch (type) {
        case InfoHintType::UP_FOLLOWING:
            this->hintBackground = theme.getColor("color/grey_1");
            this->boxHint->setBackgroundColor(this->hintBackground);
            this->svgHint->setImageFromSVGRes("svg/bpx-svg-sprite-sort.svg");
            this->labeHint->setText("已关注");
            this->hintTextColor = theme.getColor("font/grey");
            this->labeHint->setTextColor(this->hintTextColor);
            break;
        case InfoHintType::UP_NOT_FOLLOWED:
            this->hintBackground = nvgRGB(251, 114, 153);
            this->boxHint->setBackgroundColor(this->hintBackground);
            this->svgHint->setImageFromSVGRes("svg/bpx-svg-sprite-add.svg");
            this->labeHint->setText("关注");
            this->hintTextColor = theme.getColor("color/white");
            this->labeHint->setTextColor(this->hintTextColor);
            break;
        case InfoHintType::BANGUMI_FOLLOWING:
            this->hintBackground = theme.getColor("color/grey_1");
            this->boxHint->setBackgroundColor(this->hintBackground);
            this->svgHint->setImageFromSVGRes("svg/bpx-svg-sprite-sort.svg");
            this->labeHint->setText("已追番");
            this->hintTextColor = theme.getColor("font/grey");
            this->labeHint->setTextColor(this->hintTextColor);
            break;
        case InfoHintType::BANGUMI_NOT_FOLLOWED:
            this->hintBackground = nvgRGB(251, 114, 153);
            this->boxHint->setBackgroundColor(this->hintBackground);
            this->svgHint->setImageFromSVGRes("svg/bpx-svg-sprite-add.svg");
            this->labeHint->setText("追番");
            this->hintTextColor = theme.getColor("color/white");
            this->labeHint->setTextColor(this->hintTextColor);
            break;
        case InfoHintType::CINEMA_FOLLOWING:
            this->hintBackground = theme.getColor("color/grey_1");
            this->boxHint->setBackgroundColor(this->hintBackground);
            this->svgHint->setImageFromSVGRes("svg/bpx-svg-sprite-sort.svg");
            this->labeHint->setText("已追剧");
            this->hintTextColor = theme.getColor("font/grey");
            this->labeHint->setTextColor(this->hintTextColor);
            break;
        case InfoHintType::CINEMA_NOT_FOLLOWED:
            this->hintBackground = nvgRGB(251, 114, 153);
            this->boxHint->setBackgroundColor(this->hintBackground);
            this->svgHint->setImageFromSVGRes("svg/bpx-svg-sprite-add.svg");
            this->labeHint->setText("追剧");
            this->hintTextColor = theme.getColor("color/white");
            this->labeHint->setTextColor(this->hintTextColor);
            break;

        default:
            break;
    }
}