/*
    Copyright 2020-2021 natinusala

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#include <borealis/core/touch/tap_gesture.hpp>
#include <borealis/core/thread.hpp>

#include "activity/main_activity.hpp"
#include "utils/activity_helper.hpp"
#include "utils/dialog_helper.hpp"
#include "view/custom_button.hpp"
#include "view/auto_tab_frame.hpp"
#include "view/svg_image.hpp"
#include "bilibili/result/mine_result.h"
#include "fragment/mine_tab.hpp"

MainActivity::~MainActivity() { brls::Logger::debug("del MainActivity"); }

void MainActivity::onResume() {
    brls::Activity::onResume();
    if (!this->tabFrame) return;

    // 从私信/设置页返回：焦点栈会把焦点还给底部按钮，延后一帧再还给当前标签
    if (this->restoreTabFocus) {
        this->restoreTabFocus = false;
        brls::sync([this]() { this->focusActiveTab(); });
    } else {
        // 兜底：焦点仍停在底部按钮上时也恢复
        brls::View* focus = brls::Application::getCurrentFocus();
        if (focus == this->inboxBtn || focus == this->settingBtn) {
            this->focusActiveTab();
        }
    }

    // 从播放页/设置页返回时，用户空间的列表（历史等）可能已过期
    if (auto* mine = dynamic_cast<MineTab*>(this->tabFrame->getActiveTab())) {
        mine->refreshData();
    }
}

void MainActivity::focusActiveTab() {
    if (!this->tabFrame) return;
    if (auto* tab = this->tabFrame->getTab((size_t)this->tabFrame->getActiveIndex())) {
        brls::Application::giveFocus(tab);
    }
}

void MainActivity::onContentAvailable() {
    this->registerAction(
        "Settings", brls::ControllerButton::BUTTON_BACK,
        [](brls::View* view) -> bool {
            Intent::openSetting();
            return true;
        },
        true);

    this->registerAction(
        "Settings", brls::ControllerButton::BUTTON_START,
        [](brls::View* view) -> bool {
            Intent::openSetting();
            return true;
        },
        true);

    this->settingBtn->registerClickAction([this](brls::View* view) -> bool {
        this->restoreTabFocus = true;
        Intent::openSetting();
        return true;
    });

    this->settingBtn->getFocusEvent()->subscribe([this](bool value) {
        SVGImage* image = dynamic_cast<SVGImage*>(this->settingBtn->getChildren()[0]);
        if (!image) return;
        if (value) {
            image->setImageFromSVGRes("svg/ico-setting-activate.svg");
        } else {
            image->setImageFromSVGRes("svg/ico-setting.svg");
        }
    });

    this->inboxBtn->setCustomNavigation([this](brls::FocusDirection direction) {
        if (tabFrame->getSideBarPosition() == AutoTabBarPosition::LEFT) {
            if (direction == brls::FocusDirection::RIGHT) {
                return (brls::View*)this->tabFrame->getActiveTab();
            } else if (direction == brls::FocusDirection::UP) {
                return (brls::View*)this->tabFrame->getSidebar();
            }
        } else if (tabFrame->getSideBarPosition() == AutoTabBarPosition::TOP) {
            if (direction == brls::FocusDirection::DOWN) {
                return (brls::View*)this->tabFrame->getActiveTab();
            } else if (direction == brls::FocusDirection::LEFT) {
                return (brls::View*)this->tabFrame->getSidebar();
            }
        }
        return (brls::View*)nullptr;
    });
    this->settingBtn->setCustomNavigation([this](brls::FocusDirection direction) {
        if (tabFrame->getSideBarPosition() == AutoTabBarPosition::LEFT) {
            if (direction == brls::FocusDirection::RIGHT) {
                return (brls::View*)this->tabFrame->getActiveTab();
            } else if (direction == brls::FocusDirection::UP) {
                return (brls::View*)this->inboxBtn;
            }
        } else if (tabFrame->getSideBarPosition() == AutoTabBarPosition::TOP) {
            if (direction == brls::FocusDirection::DOWN) {
                return (brls::View*)this->tabFrame->getActiveTab();
            } else if (direction == brls::FocusDirection::LEFT) {
                return (brls::View*)this->inboxBtn;
            }
        }
        return (brls::View*)nullptr;
    });
    this->settingBtn->addGestureRecognizer(new brls::TapGestureRecognizer(this->settingBtn));

    this->inboxBtn->registerClickAction([this](brls::View* view) -> bool {
        if (DialogHelper::checkLogin()) {
            this->restoreTabFocus = true;
            Intent::openInbox();
        } else {
            // 未登录时只提示、不跳转，把焦点放回当前标签，避免按钮和标签同时高亮
            this->focusActiveTab();
        }
        return true;
    });

    this->inboxBtn->getFocusEvent()->subscribe([this](bool value) {
        SVGImage* image = dynamic_cast<SVGImage*>(this->inboxBtn->getChildren()[0]);
        if (!image) return;
        if (value) {
            image->setImageFromSVGRes("svg/ico-inbox-activate.svg");
        } else {
            image->setImageFromSVGRes("svg/ico-inbox.svg");
        }
    });
    this->inboxBtn->addGestureRecognizer(new brls::TapGestureRecognizer(this->inboxBtn));
}