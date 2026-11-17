//
// 可自定义方向焦点跳转的 Box
//

#include "view/nav_box.hpp"

brls::View* NavBox::create() { return new NavBox(); }

void NavBox::setNextFocus(brls::FocusDirection direction, brls::View* view) {
    this->nextFocus[direction] = view;
}

brls::View* NavBox::getNextFocus(brls::FocusDirection direction, brls::View* currentView) {
    auto it = this->nextFocus.find(direction);
    if (it != this->nextFocus.end() && it->second) return it->second;
    return brls::Box::getNextFocus(direction, currentView);
}
