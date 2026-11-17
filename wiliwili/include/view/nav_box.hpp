//
// 可自定义方向焦点跳转的 Box，用于键盘/手柄导航
//

#pragma once

#include <functional>
#include <map>

#include <borealis/core/box.hpp>

class NavBox : public brls::Box {
public:
    NavBox() = default;

    static brls::View* create();

    /// 指定某个方向上的下一个聚焦目标，未指定的方向保持默认几何寻焦
    void setNextFocus(brls::FocusDirection direction, brls::View* view);

    /// 动态解析下一个聚焦目标（优先于静态目标）
    void setNextFocusResolver(brls::FocusDirection direction, std::function<brls::View*()> resolver);

    brls::View* getNextFocus(brls::FocusDirection direction, brls::View* currentView) override;

private:
    std::map<brls::FocusDirection, brls::View*> nextFocus;
    std::map<brls::FocusDirection, std::function<brls::View*()>> nextFocusResolvers;
};
