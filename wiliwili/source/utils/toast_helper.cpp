//
// 轻量 Toast 提示（居中 / 底部居中）
//

#include <nanovg.h>
#include <borealis/core/animation.hpp>
#include <borealis/core/application.hpp>
#include <borealis/core/box.hpp>
#include <borealis/views/label.hpp>

#include "utils/toast_helper.hpp"

#ifdef ABSOLUTE
#undef ABSOLUTE
#endif

namespace {

class ToastView : public brls::Box {
public:
    ToastView(const std::string& text, bool bottom) {
        this->setPositionType(brls::PositionType::ABSOLUTE);
        this->setWidthPercentage(100);
        this->setAxis(brls::Axis::ROW);
        this->setJustifyContent(brls::JustifyContent::CENTER);
        this->setAlignItems(brls::AlignItems::CENTER);
        this->setFocusable(false);

        if (bottom) {
            this->setPositionBottomPercentage(10);
        } else {
            this->setPositionTopPercentage(45);
        }

        auto* content = new brls::Box();
        content->setAxis(brls::Axis::COLUMN);
        content->setJustifyContent(brls::JustifyContent::CENTER);
        content->setAlignItems(brls::AlignItems::CENTER);
        content->setBackgroundColor(nvgRGBA(0, 0, 0, 215));
        content->setCornerRadius(10);
        content->setPadding(16, 28, 16, 28);
        content->setMaxWidthPercentage(90);

        auto* label = new brls::Label();
        label->setText(text);
        label->setFontSize(22);
        label->setTextColor(nvgRGB(255, 255, 255));
        content->addView(label);

        this->addView(content);

        timer.reset(0.0f);
        timer.addStep(0.0f, 1600, brls::EasingFunction::linear);
        timer.addStep(1.0f, 250, brls::EasingFunction::quadraticOut);
        timer.setTickCallback([this]() { this->setAlpha(1.0f - timer.getValue()); });
        timer.setEndCallback([this](bool finished) {
            if (this->getParent()) this->getParent()->removeView(this, true);
        });
        timer.start();
    }

private:
    brls::Animatable timer;
};

void showToast(const std::string& text, bool bottom) {
    auto stack = brls::Application::getActivitiesStack();
    if (!stack.empty() && stack.back()) {
        auto* content = stack.back()->getContentView();
        if (auto* box = dynamic_cast<brls::Box*>(content)) {
            box->addView(new ToastView(text, bottom));
            return;
        }
    }
    // 无法获取当前页面时退回系统通知（右上角）
    brls::Application::notify(text);
}

}  // namespace

void ToastHelper::show(const std::string& text) { showToast(text, false); }

void ToastHelper::showBottom(const std::string& text) { showToast(text, true); }
