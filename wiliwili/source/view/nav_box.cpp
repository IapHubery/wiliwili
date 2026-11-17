//
// 可自定义方向焦点跳转的 Box
//

#include "view/nav_box.hpp"

brls::View* NavBox::create() { return new NavBox(); }

void NavBox::setNextFocus(brls::FocusDirection direction, brls::View* view) {
    this->nextFocus[direction] = view;
}

void NavBox::setNextFocusResolver(brls::FocusDirection direction, std::function<brls::View*()> resolver) {
    this->nextFocusResolvers[direction] = std::move(resolver);
}

brls::View* NavBox::getNextFocus(brls::FocusDirection direction, brls::View* currentView) {
    auto resolver = this->nextFocusResolvers.find(direction);
    if (resolver != this->nextFocusResolvers.end() && resolver->second) {
        if (auto* view = resolver->second()) return view;
    }

    auto it = this->nextFocus.find(direction);
    if (it != this->nextFocus.end() && it->second) return it->second;
    return brls::Box::getNextFocus(direction, currentView);
}
