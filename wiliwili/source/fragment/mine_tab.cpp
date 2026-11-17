//
// Created by fang on 2022/6/9.
//

#include <nanovg.h>
#include <borealis/core/application.hpp>
#include <borealis/core/thread.hpp>
#include <borealis/core/touch/tap_gesture.hpp>
#include <borealis/views/dialog.hpp>

#include "fragment/mine_tab.hpp"
#include "fragment/mine_qr_login.hpp"
#include "utils/image_helper.hpp"
#include "utils/config_helper.hpp"
#include "utils/number_helper.hpp"

#include "fragment/mine_collection.hpp"
#include "fragment/mine_history.hpp"
#include "fragment/mine_bangumi.hpp"
#include "fragment/dynamic_tab.hpp"
#include "fragment/mine_later.hpp"

#include "bilibili/result/mine_result.h"
#include "utils/shortcut_helper.hpp"

#ifdef ABSOLUTE
#undef ABSOLUTE
#endif

using namespace brls;
using namespace brls::literals;

MineTab::MineTab() {
    this->inflateFromXMLRes("xml/fragment/mine_tab.xml");
    brls::Logger::debug("Fragment MineTab: create");

    this->loginCb.subscribe([this](bilibili::LoginInfo status) {
        if (status == bilibili::LoginInfo::SUCCESS) {
            if (this->mineHistory && this->mineHistory->isEditing()) this->toggleHistoryEdit();
            if (this->editBox) this->editBox->setVisibility(brls::Visibility::GONE);
            brls::Logger::debug("{}", "wiliwili/mine/login/success"_i18n);
            this->requestData();
            try {
                this->mineHistory->requestData(true);
            } catch (...) {
            }
            try {
                this->mineCollection->requestData(true);
            } catch (...) {
            }
            try {
                this->mineSubscription->requestData(true);
            } catch (...) {
            }
            try {
                this->mineAnime->requestData(true);
            } catch (...) {
            }
            try {
                this->mineSeries->requestData(true);
            } catch (...) {
            }
            try {
                this->mineLater->requestData();
            } catch (...) {
            }
            try {
                //动态页刷新
                auto mainTab = dynamic_cast<AutoTabFrame*>(this->getParent());
                auto* tab    = (DynamicTab*)mainTab->getTab(1)->getAttachedView();
                if (!tab) {
                    brls::sync([mainTab]() {
                        auto tab = dynamic_cast<DynamicTab*>(mainTab->getTab(1)->createAttachedView());
                        tab->requestUpList();
                        tab->requestDynamicVideoList(1, "");
                    });
                } else {
                    tab->requestUpList();
                    tab->requestDynamicVideoList(1, "");
                }
            } catch (...) {
                brls::Logger::error("error: cannot refresh activity page");
            }
        }
    });

    // 历史记录编辑按钮：与顶部标签栏同高、水平居右（仅未登录的本地历史可用）
    if (!ProgramConfig::instance().hasLoginInfo() && this->mineHistory) {
        auto* sidebar = this->tabFrame->getSidebar();
        if (sidebar) {
            this->editBox = new brls::Box();
            this->editBox->setPositionType(brls::PositionType::ABSOLUTE);
            this->editBox->setPositionRight(20);
            this->editBox->setPositionTop(14);
            this->editBox->setHeight(36);
            this->editBox->setCornerRadius(18);
            this->editBox->setBackgroundColor(brls::Application::getTheme().getColor("color/bilibili"));
            this->editBox->setPaddingLeft(16);
            this->editBox->setPaddingRight(16);
            this->editBox->setJustifyContent(brls::JustifyContent::CENTER);
            this->editBox->setAlignItems(brls::AlignItems::CENTER);
            this->editBox->setFocusable(true);
            this->editLabel = new brls::Label();
            this->editLabel->setText("wiliwili/mine/local_edit"_i18n);
            this->editLabel->setFontSize(16);
            this->editLabel->setTextColor(nvgRGB(255, 255, 255));
            this->editBox->addView(this->editLabel);
            this->editBox->registerClickAction([this](brls::View*) {
                this->toggleHistoryEdit();
                return true;
            });
            this->editBox->addGestureRecognizer(new brls::TapGestureRecognizer(this->editBox));
            this->editBox->setVisibility(brls::Visibility::GONE);
            sidebar->addView(this->editBox);
        }

        this->mineHistory->onHistoryDataChanged = [this](bool hasData) {
            if (!this->editBox) return;
            if (!hasData && this->mineHistory->isEditing()) this->toggleHistoryEdit();
            this->editBox->setVisibility(hasData ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
        };
        if (this->editBox && this->mineHistory) {
            this->editBox->setVisibility(this->mineHistory->hasData() ? brls::Visibility::VISIBLE
                                                                     : brls::Visibility::GONE);
        }
    }

    // 初始化用户区域内容
    this->onUserNotLogin();
    // 在用户登录的情况获取到之前先清空点击事件
    boxGotoUserSpace->registerClickAction([](...) -> bool { return true; });
    this->requestData();

    GA("user", {{"id", ProgramConfig::instance().getUserID()}})
    GA("user", {{"user_id", ProgramConfig::instance().getUserID()}})
}

void MineTab::onCreate() {
    this->registerTabAction("wiliwili/mine/login/refresh"_i18n, brls::ControllerButton::BUTTON_X,
                            ShortcutHelper::getRefresh(),
                            [this](brls::View* view) -> bool {
                                this->requestData();
                                return true;
                            });
    this->boxGotoUserSpace->addGestureRecognizer(new TapGestureRecognizer(this->boxGotoUserSpace));

    this->registerTabAction(
        "上一项", brls::ControllerButton::BUTTON_LB,
        ShortcutHelper::getLast(),
        [this](brls::View* view) -> bool {
            tabFrame->focus2LastTab();
            return true;
        },
        true);

    this->registerTabAction(
        "下一项", brls::ControllerButton::BUTTON_RB,
        ShortcutHelper::getNext(),
        [this](brls::View* view) -> bool {
            tabFrame->focus2NextTab();
            return true;
        },
        true);
}

MineTab::~MineTab() { brls::Logger::debug("Fragment MineTabActivity: delete"); }

void MineTab::onUserNotLogin() {
    boxGotoUserSpace->registerAction(
        "hints/ok"_i18n, BUTTON_A,
        [this](View*) -> bool {
            auto dialog = new brls::Dialog(MineQrLogin::create(this->loginCb));
            dialog->addButton("hints/cancel"_i18n, []() {
            });
            dialog->open();
            return true;
        },
        false, false, SOUND_CLICK);

    ASYNC_RETAIN
    brls::sync([ASYNC_TOKEN]() {
        ASYNC_RELEASE
        labelUserName->setText("wiliwili/mine/login/click"_i18n);
        imageUserAvater->setImageFromRes("pictures/default_avatar.png");
        labelSign->setText("");
        labelCoins->setText("0");
        labelFollower->setText("0");
        labelFollowing->setText("0");
    });
}

void MineTab::toggleHistoryEdit() {
    if (!this->mineHistory) return;
    bool editing = this->mineHistory->isEditing();
    this->mineHistory->setEditMode(!editing);
    if (this->editLabel) {
        this->editLabel->setText(editing ? "wiliwili/mine/local_edit"_i18n : "wiliwili/mine/local_done"_i18n);
    }
    if (this->boxGotoUserSpace) {
        this->boxGotoUserSpace->setVisibility(editing ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
    }
}

void MineTab::onUserInfo(const bilibili::UserResult& data) {
    boxGotoUserSpace->registerAction(
        "hints/ok"_i18n, BUTTON_A,
        [](View*) -> bool {
            auto dialog = new brls::Dialog("wiliwili/mine/login/logout"_i18n);
            dialog->addButton("hints/back"_i18n, []() {
            });
            dialog->addButton("hints/ok"_i18n, []() {
                ProgramConfig::instance().setCookie({});
                brls::Application::getPlatform()->exitToHomeMode(false);
                Application::quit();
            });
            dialog->open();
            return true;
        },
        false, false, SOUND_CLICK);

    ASYNC_RETAIN
    brls::sync([ASYNC_TOKEN, data]() {
        ASYNC_RELEASE
        labelUserName->setText(data.name);
        ImageHelper::with(this->imageUserAvater)->load(data.face);
        if (data.sign.empty()) {
            labelSign->setText("这个人很神秘，什么都没有写");
        } else {
            labelSign->setText(data.sign);
        }

        labelCoins->setText(fmt::format("{}", data.coins));
    });
}

void MineTab::onUserDynamicStat(const bilibili::UserDynamicCount& result) {
    ASYNC_RETAIN
    brls::sync([ASYNC_TOKEN, result]() {
        ASYNC_RELEASE
        this->labelDynamic->setText(wiliwili::num2w(result.dyn_num));
    });
}

void MineTab::onUserRelationStat(const bilibili::UserRelationStat& result) {
    ASYNC_RETAIN
    brls::sync([ASYNC_TOKEN, result]() {
        ASYNC_RELEASE
        this->labelFollower->setText(wiliwili::num2w(result.follower));
        this->labelFollowing->setText(wiliwili::num2w(result.following));
    });
}

View* MineTab::create() { return new MineTab(); }