//
// Created by fang on 2023/4/26.
//

#include <chrono>
#include <exception>
#include <fstream>
#include <functional>
#include <thread>

#include <fmt/format.h>
#include <cpr/cpr.h>
#include <cpr/filesystem.h>
#include <borealis/core/application.hpp>
#include <borealis/core/gesture.hpp>
#include <borealis/core/thread.hpp>
#include <borealis/views/image.hpp>

#include "activity/gallery_activity.hpp"
#include "view/gallery_view.hpp"
#include "utils/config_helper.hpp"
#include "utils/image_helper.hpp"
#include "utils/number_helper.hpp"
#include "bilibili/util/json.hpp"

using namespace brls::literals;

namespace {

/// 简易长按手势：按住 700ms 触发一次
class LongPressGesture : public brls::GestureRecognizer {
public:
    explicit LongPressGesture(std::function<void()> cb) : callback(std::move(cb)) {}

    brls::GestureState recognitionLoop(brls::TouchState touch, brls::MouseState mouse, brls::View* view,
                                       brls::Sound* soundToPlay) override {
        brls::TouchPhase phase = touch.phase;
        if (phase == brls::TouchPhase::NONE) phase = mouse.leftButton;

        switch (phase) {
            case brls::TouchPhase::START:
                this->startTime = std::chrono::steady_clock::now();
                this->fired     = false;
                return brls::GestureState::UNSURE;
            case brls::TouchPhase::STAY:
                if (!this->fired &&
                    std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() -
                                                                         this->startTime)
                            .count() >= 700) {
                    this->fired = true;
                    if (this->callback) this->callback();
                    if (soundToPlay) *soundToPlay = brls::SOUND_NONE;
                    return brls::GestureState::START;
                }
                return brls::GestureState::UNSURE;
            case brls::TouchPhase::END:
                this->fired = false;
                return brls::GestureState::FAILED;
            default:
                return brls::GestureState::UNSURE;
        }
    }

private:
    std::function<void()> callback;
    std::chrono::steady_clock::time_point startTime{};
    bool fired = false;
};

std::string imageSaveDir() {
    auto custom = ProgramConfig::instance().getSettingItem(SettingItem::IMAGE_SAVE_PATH, std::string{});
    if (!custom.empty()) return custom;
    // 默认保存到缓存路径
    return ProgramConfig::instance().getConfigDir() + "/cache";
}

std::string imageFileName(const std::string& url) {
    std::string name = url;
    auto query       = name.find('?');
    if (query != std::string::npos) name = name.substr(0, query);
    auto pos = name.find_last_of('/');
    if (pos != std::string::npos) name = name.substr(pos + 1);
    if (name.empty()) name = "image";
    if (name.find('.') == std::string::npos) name += ".jpg";
    return name;
}

void saveImageAsync(const std::string& url) {
    std::string link = bilibili::parseLink(url);
    std::thread([link]() {
        cpr::Response r =
            cpr::Get(cpr::Url{link},
                     cpr::Header{{"User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 Chrome/120.0"},
                                 {"Referer", "https://www.bilibili.com/"}},
                     cpr::Timeout{15000});
        if (r.status_code != 200 || r.text.empty()) {
            brls::Logger::error("saveImage: download failed {} {}", r.status_code, link);
            brls::sync([] { brls::Application::notify("wiliwili/image/save_fail"_i18n); });
            return;
        }

        std::string dir  = imageSaveDir();
        std::string path = fmt::format("{}/{}_{}", dir, wiliwili::getUnixTime(), imageFileName(link));
        try {
            cpr::fs::create_directories(dir);
            std::ofstream out(path, std::ios::binary);
            if (!out) {
                brls::Logger::error("saveImage: cannot write {}", path);
                brls::sync([] { brls::Application::notify("wiliwili/image/save_fail"_i18n); });
                return;
            }
            out.write(r.text.data(), (std::streamsize)r.text.size());
            out.close();
        } catch (const std::exception& e) {
            brls::Logger::error("saveImage: {} ({})", e.what(), path);
            brls::sync([] { brls::Application::notify("wiliwili/image/save_fail"_i18n); });
            return;
        }
        brls::sync([path] { brls::Application::notify(fmt::format("{}: {}", "wiliwili/image/saved"_i18n, path)); });
    }).detach();
}

}  // namespace

const std::string ImageGalleryItemXML = R"xml(
    <brls:Box
        width="100%"
        height="100%"
        axis="row"
        grow="1"
        justifyContent="center"
        alignItems="center">

        <brls:Image
                margin="40"
                id="gallery/image"/>
    </brls:Box>
)xml";

class NetImageGalleryItem : public GalleryItem {
public:
    explicit NetImageGalleryItem(const std::string& url) : url(url) {
        this->inflateFromXMLString(ImageGalleryItemXML);
        this->image->setImageFromRes("icon/bilibili_video.png");
        ImageHelper::with(this->image)->load(url);
        // 长按保存图片（默认保存到缓存目录，可在设置中修改保存路径）
        this->addGestureRecognizer(new LongPressGesture([url]() { saveImageAsync(url); }));
    }
    ~NetImageGalleryItem() override { ImageHelper::clear(this->image); }

private:
    BRLS_BIND(brls::Image, image, "gallery/image");
    std::string url;
};

GalleryActivity::GalleryActivity(const std::vector<std::string>& data) : galleryData(data) {
    brls::Logger::debug("GalleryActivity: create");
}

void GalleryActivity::onContentAvailable() {
    brls::Logger::debug("GalleryActivity: onContentAvailable");
    gallery->setIndicatorPosition(0.97);
    for (auto& i : galleryData) {
        gallery->addCustomView(new NetImageGalleryItem(i));
    }
}

GalleryActivity::~GalleryActivity() { brls::Logger::debug("GalleryActivity: delete"); }

bool GalleryActivity::isTranslucent() { return true; }
