//
// Created by fang on 2022/7/18.
//

#include <algorithm>
#include <chrono>

#include <borealis/core/touch/tap_gesture.hpp>
#include <borealis/views/image.hpp>
#include <pystring.h>

#include "view/video_comment.hpp"
#include "view/text_box.hpp"
#include "view/nav_box.hpp"
#include "view/svg_image.hpp"
#include "utils/number_helper.hpp"
#include "utils/string_helper.hpp"
#include "utils/config_helper.hpp"
#include <borealis/core/touch/tap_gesture.hpp>

#include "utils/activity_helper.hpp"

using namespace brls::literals;

enum class CommentElementType {
    EMOTE,
    USER,
    TOPIC,
    JUMP,
    NONE,
};

/// 为评论中的表情包添加分辨率后缀，不加后缀则直接加载原图
static inline std::string parseUrl(const std::string& url, const std::string& extra) {
    // url 中可能已经包含了指定的尺寸，需要移除指定的尺寸并重新添加
    // 示例: http://i0.hdslb.com/bfs/garb/emote_diy/ee38500008e72e5623f8972a6ea3d922.png@162w
    // 绝大多数情况下，额外的尺寸都是不存在的
    for (auto it = url.rbegin(); it != url.rend(); ++it) {
        if (*it == '.') {
            return url + extra;
        } else if (*it == '@') {
            return url.substr(0, url.size() - (it - url.rbegin()) - 1) + extra;
        }
    }
    return url + extra;
}

class CommentElement {
public:
    CommentElement(const std::string& title, CommentElementType type) : title(title), type(type) {}
    std::string title;
    CommentElementType type;
    bool matchDone = false;  // 为真则不参与匹配
};

class CommentElementEmote : public CommentElement {
public:
    CommentElementEmote(const std::string& title, const std::string& url, int size)
        : CommentElement(title, CommentElementType::EMOTE), url(url), size(size) {}
    std::string url;
    int size = 1;
};

class CommentElementUser : public CommentElement {
public:
    CommentElementUser(const std::string& name, uint64_t id)
        : CommentElement("@" + name + " ", CommentElementType::USER), name(name), id(id) {}
    std::string name;
    uint64_t id;
};

class CommentElementTopic : public CommentElement {
public:
    CommentElementTopic(const std::string& topic, const std::string& uri)
        : CommentElement("#" + topic + "#", CommentElementType::TOPIC), topic(topic), uri(uri) {}
    std::string topic;
    std::string uri;
};

enum class CommentElementJumpType {
    SEARCH,
    NONE,
};

class CommentElementJump : public CommentElement {
public:
    CommentElementJump(const std::string& title, const std::string& show, const std::string& icon, int position,
                       CommentElementJumpType type)
        : CommentElement(title, CommentElementType::JUMP),
          showTitle(show),
          icon(icon),
          position(position),
          type(type) {}
    std::string showTitle;
    std::string icon;
    int position                = 1;
    CommentElementJumpType type = CommentElementJumpType::NONE;
};

typedef std::unordered_map<std::string, std::shared_ptr<CommentElement>> CEMap;

VideoComment::VideoComment() {
    brls::Logger::verbose("View VideoComment: create");
    this->inflateFromXMLRes("xml/views/video_comment.xml");

    this->registerColorXMLAttribute("mainTextColor", [this](NVGcolor value) { this->setMainTextColor(value); });

    this->registerFloatXMLAttribute("maxRows", [this](float value) { this->setMaxRows((size_t)value); });

    // 评论卡片上的点赞 / 点踩 / 回复：点击 / Enter / 手柄确定触发，高亮短暂闪现后消失
    auto bindAction = [this](brls::Box* box, std::function<void()>* action, brls::Animatable* flash) {
        auto activate = [this, box, action, flash]() {
            if (action && *action) (*action)();
            this->flashAction(box, *flash);
        };
        box->registerClickAction([activate](brls::View*) {
            activate();
            return true;
        });
        box->addGestureRecognizer(new brls::TapGestureRecognizer(box, activate));
    };
    bindAction(this->likeBox, &this->likeCallback, &this->likeFlash);
    bindAction(this->dislikeBox, &this->dislikeCallback, &this->dislikeFlash);
    bindAction(this->replyBox, &this->replyCallback, &this->replyFlash);

    // 点击头像 / 昵称进入用户空间（未登录也可以）
    this->userInfo->registerClickAction([this](brls::View*) {
        Intent::openUserSpace(this->comment_data.member.mid);
        return true;
    });
    this->userInfo->addGestureRecognizer(new brls::TapGestureRecognizer(this->userInfo));
}

VideoComment::~VideoComment() { brls::Logger::verbose("View VideoComment: delete"); }

RecyclingGridItem* VideoComment::create() { return new VideoComment(); }

void VideoComment::setMainTextColor(NVGcolor color) {
    this->commentContent->setTextColor(color);
    this->userInfo->setMainTextColor(color);
}

void VideoComment::setMaxRows(size_t value) { this->commentContent->setMaxRows(value); }

void VideoComment::setData(bilibili::VideoCommentResult data) {
    this->comment_data = data;

    std::string subtitle = wiliwili::sec2date(data.ctime);
    if (!data.reply_control.location.empty()) {
        subtitle += "  " + data.reply_control.location;
    }

    // 结尾加个空格用来正确识别尾部的@
    RichTextData d;
    std::string msg    = data.content.message + " ";
    auto theme         = brls::Application::getTheme();
    NVGcolor textColor = theme.getColor("brls/text");
    NVGcolor linkColor = theme.getColor("color/link");
    NVGcolor biliColor = theme.getColor("color/bilibili");

    if (data.top) {
        auto top      = std::make_shared<RichTextSpan>("置顶", biliColor);
        top->r_margin = 10;
        d.emplace_back(top);
    }

    // todo: 识别进度跳转

    // 将表情包、@、跳转、话题 整合进一个map里
    CEMap commentElement;
    for (auto& i : data.content.emote) {
        commentElement.insert({i.first, std::make_shared<CommentElementEmote>(i.first, i.second.url, i.second.size)});
    }
    for (auto& i : data.content.at_name_to_mid) {
        commentElement.insert({"@" + i.first + " ", std::make_shared<CommentElementUser>(i.first, i.second)});
    }
    for (auto& i : data.content.jump_url) {
        commentElement.insert(
            {i.first, std::make_shared<CommentElementJump>(
                          i.first, i.second.title, i.second.prefix_icon, i.second.icon_position,
                          i.second.search ? CommentElementJumpType::SEARCH : CommentElementJumpType::NONE)});
    }
    for (auto& i : data.content.topics_uri) {
        commentElement.insert({"#" + i.first + "#", std::make_shared<CommentElementTopic>(i.first, i.second)});
    }

    // 识别评论组件
    size_t start = 0;
    for (size_t i = 0; i < msg.length(); i++) {
        std::shared_ptr<CommentElement> matchElement = nullptr;
        size_t nextMatch                             = -1;
        for (auto& key : commentElement) {
            if (key.second->matchDone) continue;
            size_t position = msg.find(key.first, i);
            if (position < nextMatch) {
                nextMatch    = position;
                matchElement = key.second;
            }
        }
        if (matchElement == nullptr) nextMatch = msg.length() - 1;
        if (start < nextMatch) {
            // 纯文本
            auto item = std::make_shared<RichTextSpan>(msg.substr(start, nextMatch - start) + "\r", textColor);
            d.emplace_back(item);
        }
        if (matchElement == nullptr) break;
        // 根据 matchElement 类型判断
        switch (matchElement->type) {
            case CommentElementType::EMOTE: {
                auto* t = (CommentElementEmote*)matchElement.get();
                std::shared_ptr<RichTextImage> item;
                if (t->size == 2) {
                    item = std::make_shared<RichTextImage>(parseUrl(t->url, ImageHelper::emoji_size2_ext), 50, 50);
                    item->t_margin = 4;
                } else {
                    item = std::make_shared<RichTextImage>(parseUrl(t->url, ImageHelper::emoji_size1_ext), 30, 30);
                }
                item->v_align  = 4;
                item->l_margin = 2;
                item->r_margin = 2;
                d.emplace_back(item);
                break;
            }
            case CommentElementType::USER: {
                auto* t        = (CommentElementUser*)matchElement.get();
                auto item      = std::make_shared<RichTextSpan>(t->title, linkColor);
                item->l_margin = 8;
                item->r_margin = 8;
                d.emplace_back(item);
                break;
            }
            case CommentElementType::TOPIC: {
                auto* t   = (CommentElementTopic*)matchElement.get();
                auto item = std::make_shared<RichTextSpan>(t->title, linkColor);
                d.emplace_back(item);
                break;
            }
            case CommentElementType::JUMP: {
                auto* t = (CommentElementJump*)matchElement.get();
                if (!t->icon.empty() && t->position == 0) {
                    auto item     = std::make_shared<RichTextImage>(t->icon, 30, 30);
                    item->v_align = 5;
                    d.emplace_back(item);
                }
                d.emplace_back(std::make_shared<RichTextSpan>(t->showTitle, linkColor));
                if (!t->icon.empty() && t->position == 1) {
                    auto item      = std::make_shared<RichTextImage>(t->icon, 16, 30);
                    item->v_align  = 5;
                    item->r_margin = 2;
                    d.emplace_back(item);
                }
                // 关键字只匹配一次
                commentElement[t->title]->matchDone = true;
                break;
            }
            case CommentElementType::NONE:
                break;
        }

        i     = nextMatch + matchElement->title.length() - 1;
        start = i + 1;
    }

    // 图片改为下方的可聚焦缩略图（buildPictures），不再嵌入富文本

    // 设置富文本
    this->commentContent->setRichText(d);

    // 设置用户信息
    this->userInfo->setUserInfo(data.member.avatar + ImageHelper::face_ext, data.member.uname, subtitle);
    if (data.member.vip.nickname_color.empty()) {
        this->userInfo->setMainTextColor(brls::Application::getTheme().getColor("brls/text"));
    } else {
        // 如果是官方默认粉色 (#FB7299)，且用户设置了自定义主题色，则使用自定义色
        const std::string& nc = data.member.vip.nickname_color;
        const std::string& customColor = Register::getCustomThemeColorHex();
        if (!customColor.empty() && Register::isBilibiliDefaultPink(nc)) {
            this->userInfo->getLabelName()->applyXMLAttribute("textColor", customColor);
        } else {
            this->userInfo->getLabelName()->applyXMLAttribute("textColor", nc);
        }
    }

    // 设置用户等级
    int lv = data.member.level_info.current_level;
    if (lv < 0 || lv > 6) {
        this->userLevel->setVisibility(brls::Visibility::GONE);
    } else {
        this->userLevel->setVisibility(brls::Visibility::VISIBLE);
        if (data.member.is_uploader) {
            this->userLevel->setImageFromSVGRes(fmt::format("svg/user-up.svg", lv));
        } else if (lv == 6 && data.member.is_senior_member) {
            this->userLevel->setImageFromSVGRes("svg/user-lv6p.svg");
        } else {
            this->userLevel->setImageFromSVGRes(fmt::format("svg/user-lv{}.svg", lv));
        }
    }

    // 设置点赞状态
    // action, 0: 未点赞, 1: 已点赞, 2: 已点踩
    this->setLiked(data.action);
    this->setLikeNum(data.like);
    this->setReplyNum(data.rcount);

    this->buildPictures(data);

}

bilibili::VideoCommentResult VideoComment::getData() { return this->comment_data; }

void VideoComment::setReplyNum(size_t num) {
    this->comment_data.rcount = num;
    this->labelReply->setText(wiliwili::num2w(num));
}

void VideoComment::setLikeNum(size_t num) {
    this->comment_data.like = num;
    this->labelLike->setText(wiliwili::num2w(num));
}

void VideoComment::setLiked(size_t action) {
    this->comment_data.action = action;
    if (action == 1) {
        this->svgLike->setImageFromSVGRes("svg/comment-agree-active.svg");
        this->svgDislike->setImageFromSVGRes("svg/comment-disagree-grey.svg");
    } else if (action == 2) {
        this->svgLike->setImageFromSVGRes("svg/comment-agree-grey.svg");
        this->svgDislike->setImageFromSVGRes("svg/comment-disagree-active.svg");
    } else {
        this->svgLike->setImageFromSVGRes("svg/comment-agree-grey.svg");
        this->svgDislike->setImageFromSVGRes("svg/comment-disagree-grey.svg");
    }
}

void VideoComment::prepareForReuse() { this->userInfo->getAvatar()->setImageFromRes("pictures/default_avatar.png"); }

void VideoComment::flashAction(brls::Box* box, brls::Animatable& anim) {
    if (!box) return;
    box->setBackgroundColor(nvgRGBA(0, 161, 214, 70));
    anim.reset(1.0f);
    anim.addStep(1.0f, 220, brls::EasingFunction::linear);
    anim.addStep(0.0f, 180, brls::EasingFunction::quadraticOut);
    anim.setTickCallback([box, &anim]() {
        box->setBackgroundColor(nvgRGBA(0, 161, 214, (unsigned char)(70 * anim.getValue())));
    });
    anim.setEndCallback([box](bool) { box->setBackgroundColor(nvgRGBA(0, 0, 0, 0)); });
    anim.start();
}

void VideoComment::setNextDownResolver(std::function<brls::View*()> resolver) {
    this->downResolver = std::move(resolver);
}

void VideoComment::setActionDownResolver(std::function<brls::View*()> resolver) {
    brls::Box* boxes[] = {this->likeBox.getView(), this->dislikeBox.getView(), this->replyBox.getView()};
    for (auto* box : boxes) {
        if (auto* nav = dynamic_cast<NavBox*>(box)) {
            nav->setNextFocusResolver(brls::FocusDirection::DOWN, resolver);
        }
    }
}

brls::View* VideoComment::getNextFocus(brls::FocusDirection direction, brls::View* currentView) {
    if (direction == brls::FocusDirection::DOWN && this->focusIntoActions) {
        if (this->picturesBox && this->picturesBox->getVisibility() == brls::Visibility::VISIBLE &&
            !this->picturesBox->getChildren().empty()) {
            return this->picturesBox->getChildren().front();
        }
        if (this->likeBox) return this->likeBox.getView();
    }
    if (direction == brls::FocusDirection::DOWN && this->downResolver) {
        if (auto* next = this->downResolver()) return next;
    }
    return RecyclingGridItem::getNextFocus(direction, currentView);
}

void VideoComment::buildPictures(const bilibili::VideoCommentResult& data) {
    this->picturesBox->clearViews(true);
    if (data.content.pictures.empty()) {
        this->picturesBox->setVisibility(brls::Visibility::GONE);
        return;
    }
    this->picturesBox->setVisibility(brls::Visibility::VISIBLE);

    std::vector<std::string> rawPictures;
    rawPictures.reserve(data.content.pictures.size());
    for (const auto& picture : data.content.pictures) {
        rawPictures.push_back(ImageHelper::parseRawImageUrl(picture.img_src));
    }

    std::vector<NavBox*> holders;
    for (size_t i = 0; i < data.content.pictures.size(); i++) {
        const auto& picture = data.content.pictures[i];

        auto* holder = new NavBox();
        holder->setFocusable(true);
        holder->setCornerRadius(8);
        holder->setPadding(3);
        holder->setMarginRight(8);
        holder->setMarginTop(8);
        holder->setBackgroundColor(nvgRGBA(0, 0, 0, 0));

        auto* image = new brls::Image();
        image->setWidth(150);
        image->setHeight(95);
        image->setCornerRadius(6);
        ImageHelper::with(image)->load(ImageHelper::parseNoteImageUrl(picture.img_src, 300, 190));
        holder->addView(image);

        auto openGallery = [rawPictures, i]() {
            std::vector<std::string> list = rawPictures;
            if (i < list.size()) std::rotate(list.begin(), list.begin() + i, list.end());
            Intent::openGallery(list);
        };

        holder->registerClickAction([openGallery](brls::View*) {
            openGallery();
            return true;
        });

        // 单击选中并高亮，双击打开大图
        auto lastClick = std::make_shared<std::chrono::steady_clock::time_point>();
        holder->addGestureRecognizer(new brls::TapGestureRecognizer(holder, [holder, openGallery, lastClick]() {
            brls::Application::giveFocus(holder);
            auto now = std::chrono::steady_clock::now();
            if (lastClick->time_since_epoch().count() > 0 &&
                std::chrono::duration_cast<std::chrono::milliseconds>(now - *lastClick).count() < 400) {
                openGallery();
            }
            *lastClick = now;
        }));

        holder->getFocusEvent()->subscribe([holder](bool focused) {
            holder->setBackgroundColor(focused ? nvgRGBA(0, 161, 214, 90) : nvgRGBA(0, 0, 0, 0));
        });

        this->picturesBox->addView(holder);
        holders.push_back(holder);
    }

    // 图片之间左右移动，向下进入点赞按钮
    for (size_t i = 0; i < holders.size(); i++) {
        if (i + 1 < holders.size()) holders[i]->setNextFocus(brls::FocusDirection::RIGHT, holders[i + 1]);
        if (i > 0) holders[i]->setNextFocus(brls::FocusDirection::LEFT, holders[i - 1]);
        holders[i]->setNextFocus(brls::FocusDirection::DOWN, this->likeBox);
    }
}

void VideoComment::setLikeClickCallback(std::function<void()> cb) { this->likeCallback = std::move(cb); }

void VideoComment::setDislikeClickCallback(std::function<void()> cb) { this->dislikeCallback = std::move(cb); }

void VideoComment::setReplyClickCallback(std::function<void()> cb) { this->replyCallback = std::move(cb); }

void VideoComment::hideReplyIcon(bool hide) {
    if (hide) {
        this->labelReply->setVisibility(brls::Visibility::GONE);
        this->svgReply->setVisibility(brls::Visibility::GONE);
    } else {
        this->svgReply->setVisibility(brls::Visibility::VISIBLE);
        this->labelReply->setVisibility(brls::Visibility::VISIBLE);
    }
}

void VideoComment::cacheForReuse() { ImageHelper::clear(this->userInfo->getAvatar()); }

/// GridHintView

GridHintView::GridHintView() {
    this->setFocusable(false);
    hintLabel = new brls::Label();
    hintLabel->setFontSize(16);
    hintLabel->setMarginLeft(8);
    hintLabel->setTextColor(brls::Application::getTheme().getColor("font/grey"));
    this->addView(hintLabel);
}

RecyclingGridItem* GridHintView::create() { return new GridHintView(); }

/// VideoCommentReply

VideoCommentReply::VideoCommentReply() {
    auto theme = brls::Application::getTheme();
    this->setFocusable(true);
    this->setHeight(40);
    this->setJustifyContent(brls::JustifyContent::FLEX_START);
    this->setCornerRadius(8);
    this->setBackgroundColor(theme.getColor("color/grey_2"));
    hintLabel = new brls::Label();
    this->hintLabel->setMarginLeft(20);
    this->hintLabel->setFontSize(18);
    this->hintLabel->setText("wiliwili/player/single_comment/hint"_i18n);
    this->hintLabel->setTextColor(theme.getColor("font/grey"));
    this->addView(hintLabel);
}

RecyclingGridItem* VideoCommentReply::create() { return new VideoCommentReply(); }

/// VideoCommentSort

VideoCommentSort::VideoCommentSort() {
    auto theme = brls::Application::getTheme();
    this->setFocusable(true);
    this->setHeight(30);
    this->setJustifyContent(brls::JustifyContent::SPACE_BETWEEN);
    hintLabel = new brls::Label();
    this->hintLabel->setFontSize(18);
    this->addView(hintLabel);

    auto* rightBox = new brls::Box();
    rightBox->setAlignItems(brls::AlignItems::CENTER);
    svgImage = new SVGImage();
    this->svgImage->setSize(brls::Size(16, 16));
    this->svgImage->setImageFromSVGRes("svg/bpx-svg-sprite-sort.svg");
    rightBox->addView(svgImage);
    sortLabel = new brls::Label();
    this->sortLabel->setMarginLeft(4);
    this->sortLabel->setFontSize(16);
    this->sortLabel->setTextColor(theme.getColor("font/grey"));
    rightBox->addView(sortLabel);
    this->addView(rightBox);
    this->setPaddingBottom(6);
    this->setHideClickAnimation(true);
    this->addGestureRecognizer(new brls::TapGestureRecognizer(this));
}

RecyclingGridItem* VideoCommentSort::create() { return new VideoCommentSort(); }