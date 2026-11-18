//
// Created by fang on 2022/7/18.
//

// register this view in main.cpp
//#include "view/video_comment.hpp"
//    brls::Application::registerXMLView("VideoComment", VideoComment::create);
// <brls:View xml=@res/xml/views/video_comment.xml

#pragma once

#include <functional>

#include <borealis/core/animation.hpp>

#include "view/recycling_grid.hpp"
#include "view/user_info.hpp"
#include "bilibili/result/video_detail_result.h"

class SVGImage;
class TextBox;

/// GridHintView

class GridHintView : public RecyclingGridItem {
public:
    GridHintView();

    static RecyclingGridItem* create();

    brls::Label* hintLabel;
};

class VideoCommentReply : public RecyclingGridItem {
public:
    VideoCommentReply();

    static RecyclingGridItem* create();

    brls::Label* hintLabel;
};

class VideoCommentSort : public RecyclingGridItem {
public:
    VideoCommentSort();

    static RecyclingGridItem* create();

    brls::Label* hintLabel;
    brls::Label* sortLabel;
    SVGImage* svgImage;
};

class VideoComment : public RecyclingGridItem {
public:
    VideoComment();

    ~VideoComment() override;

    static RecyclingGridItem* create();

    void setMaxRows(size_t value);

    void setData(bilibili::VideoCommentResult data);

    void setReplyNum(size_t num);

    void setLikeNum(size_t num);

    void setLiked(size_t action);

    bilibili::VideoCommentResult getData();

    void setMainTextColor(NVGcolor color);

    void hideReplyIcon(bool hide);

    /// 直接点击评论卡片上的图标时触发（不再弹出操作菜单）
    void setLikeClickCallback(std::function<void()> cb);
    void setDislikeClickCallback(std::function<void()> cb);
    void setReplyClickCallback(std::function<void()> cb);

    /// 是否允许按 Enter 进入评论内部（评论详情页启用）
    void setFocusIntoActions(bool value);

    /// 上一条评论（子评论向上回到父评论/上一条子评论）
    void setPrevResolver(std::function<brls::View*()> resolver);

    /// 非详情模式（或子评论）按下方向键时的动态目标：通常是下一条评论
    void setNextDownResolver(std::function<brls::View*()> resolver);

    /// 详情页主评论的点赞/点踩/回复按下方向键时的动态目标：第一条子评论
    void setActionDownResolver(std::function<brls::View*()> resolver);

    void prepareForReuse() override;

    void cacheForReuse() override;

protected:
    BRLS_BIND(TextBox, commentContent, "comment/label/content");
    BRLS_BIND(brls::Label, labelLike, "comment/label/like");
    BRLS_BIND(brls::Label, labelReply, "comment/label/reply");
    BRLS_BIND(UserInfoView, userInfo, "comment/userinfo");
    BRLS_BIND(brls::Box, picturesBox, "comment/pictures");
    BRLS_BIND(brls::Box, likeBox, "comment/action/like");
    BRLS_BIND(brls::Box, dislikeBox, "comment/action/dislike");
    BRLS_BIND(brls::Box, replyBox, "comment/action/reply");
    BRLS_BIND(SVGImage, userLevel, "comment/user/level");
    BRLS_BIND(SVGImage, svgReply, "comment/svg/reply");
    BRLS_BIND(SVGImage, svgLike, "comment/svg/like");
    BRLS_BIND(SVGImage, svgDislike, "comment/svg/dislike");
    bilibili::VideoCommentResult comment_data;
    std::function<void()> likeCallback     = nullptr;
    std::function<void()> dislikeCallback  = nullptr;
    std::function<void()> replyCallback    = nullptr;
    bool focusIntoActions                  = false;
    bool internal                          = false;
    bool actionsRegistered                 = false;
    std::function<brls::View*()> downResolver = nullptr;
    std::function<brls::View*()> prevResolver = nullptr;

    brls::Animatable likeFlash;
    brls::Animatable dislikeFlash;
    brls::Animatable replyFlash;

    void flashAction(brls::Box* box, brls::Animatable& anim);

    void enterInternal();
    void exitInternal();
    void applyChildFocus(bool value);
    brls::View* lastPicture();

    brls::View* getNextFocus(brls::FocusDirection direction, brls::View* currentView) override;

private:
    void buildPictures(const bilibili::VideoCommentResult& data);
};