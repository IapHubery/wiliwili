//
// 轻量 Toast 提示（居中 / 底部居中），用于替代 Alert 弹窗
//

#pragma once

#include <string>

class ToastHelper {
public:
    /// 屏幕居中显示
    static void show(const std::string& text);

    /// 底部居中显示（用于失败提示）
    static void showBottom(const std::string& text);
};
