//
// Created by fang on 2022/7/28.
//

// register this fragment in main.cpp
//#include "fragment/mine_history.hpp"
//    brls::Application::registerXMLView("MineHistory", MineHistory::create);
// <brls:View xml=@res/xml/fragment/mine_history.xml

#pragma once

#include <borealis/core/box.hpp>
#include "view/auto_tab_frame.hpp"
#include "view/recycling_grid.hpp"
#include "presenter/mine_history.hpp"

class DataSourceMineHistoryVideoList;

class MineHistory : public AttachedView, public MineHistoryRequest {
public:
    MineHistory();

    ~MineHistory();

    static View *create();

    void onCreate() override;

    void onHistoryList(const bilibili::HistoryVideoResultWrapper &result) override;

    void onError(const std::string &error) override;

private:
    BRLS_BIND(RecyclingGrid, recyclingGrid, "mine/history/recyclingGrid");
    BRLS_BIND(brls::Box, topBar, "mine/history/header");
    BRLS_BIND(brls::Box, btnToday, "mine/history/today");
    BRLS_BIND(brls::Box, btnYesterday, "mine/history/yesterday");
    BRLS_BIND(brls::Box, btnOlder, "mine/history/older");
    BRLS_BIND(brls::Box, btnDone, "mine/history/done");
    BRLS_BIND(brls::Box, bottomBar, "mine/history/footer");
    BRLS_BIND(brls::Box, btnSelectAll, "mine/history/selectall");
    BRLS_BIND(brls::Box, btnDelete, "mine/history/delete");

    DataSourceMineHistoryVideoList *getDataSource();

    void enterSelection();
    void exitSelection();
    void selectRange(int which);  // 0=今天 1=昨天 2=更早
    void deleteSelected();
};
