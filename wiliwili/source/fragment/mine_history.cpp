//
// Created by fang on 2022/7/28.
//

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <functional>
#include <set>
#include <string>
#include <utility>
#include <fmt/format.h>
#include <vector>

#include <nanovg.h>
#include <borealis/core/application.hpp>
#include <borealis/core/thread.hpp>
#include <borealis/core/touch/tap_gesture.hpp>
#include <borealis/views/dialog.hpp>

#include "fragment/mine_history.hpp"
#include "view/video_card.hpp"
#include "utils/number_helper.hpp"
#include "utils/activity_helper.hpp"
#include "utils/image_helper.hpp"
#include "utils/config_helper.hpp"
#include "utils/dialog_helper.hpp"
#include "utils/local_store.hpp"
#include "utils/toast_helper.hpp"
#include "utils/shortcut_helper.hpp"

using namespace brls::literals;

namespace {

/// 历史记录网格的列数，与 xml 中 @style/wiliwili/grid/span/4 保持一致
constexpr int HISTORY_GRID_SPAN = 4;

std::string localDayKey(int64_t timestamp) {
    time_t t = (time_t)timestamp;
    struct tm date {};
#ifdef _WIN32
    localtime_s(&date, &t);
#else
    localtime_r(&t, &date);
#endif
    char buf[24];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d", date.tm_year + 1900, date.tm_mon + 1, date.tm_mday);
    return buf;
}

std::string historyDayLabel(int64_t timestamp) {
    std::string key       = localDayKey(timestamp);
    std::string today     = localDayKey((int64_t)wiliwili::getUnixTime());
    std::string yesterday = localDayKey((int64_t)wiliwili::getUnixTime() - 86400);
    if (key == today) return "wiliwili/mine/local_today"_i18n;
    if (key == yesterday) return "wiliwili/mine/local_yesterday"_i18n;
    return key.size() >= 10 ? key.substr(5) : key;
}

void openHistoryItem(const bilibili::HistoryVideoResult& item) {
    const auto& data = item.history;
    if (data.business == "archive") {
        Intent::openBV(data.bvid, data.cid, -1);
    } else if (data.business == "pgc") {
        Intent::openSeasonByEpId(data.epid, -1);
    } else if (data.business == "live") {
        if (item.live_status) {
            Intent::openLive(data.oid, item.title, "-");
        }
    } else if (data.business == "article" || data.business == "article-list") {
        auto cvid = data.oid;
        if (data.cid != 0) cvid = data.cid;
        auto url = fmt::format("https://www.bilibili.com/read/cv{}", cvid);

        auto* container = new brls::Box(brls::Axis::COLUMN);
        container->setJustifyContent(brls::JustifyContent::CENTER);
        container->setAlignItems(brls::AlignItems::CENTER);
        auto* l1 = new brls::Label();
        l1->setFontSize(24);
        l1->setText("wiliwili/mine/article"_i18n);
        auto* l2 = new brls::Label();
        l2->setMarginTop(10);
        l2->setTextColor(brls::Application::getTheme().getColor("color/link"));
        l2->setText(url);
        container->setHeight(200);
        container->addView(l1);
        container->addView(l2);
        auto* dialog = new brls::Dialog(container);
        dialog->addButton("hints/ok"_i18n, [url]() { brls::Application::getPlatform()->openBrowser(url); });
        dialog->open();
    }
}

/// 占位单元格：用于按天分组时补满当前行，使下一天从新的一行开始
class BlankHistoryCell : public RecyclingGridItem {
public:
    BlankHistoryCell() { this->setFocusable(false); }
};

/// 带勾选标记的历史卡片
class SelectableHistoryCard : public RecyclingGridItemHistoryVideoCard {
public:
    SelectableHistoryCard() {
        this->checkBox = new brls::Box();
        this->checkBox->setPositionType(brls::PositionType::ABSOLUTE);
        this->checkBox->setPositionTop(8);
        this->checkBox->setPositionRight(8);
        this->checkBox->setWidth(34);
        this->checkBox->setHeight(34);
        this->checkBox->setCornerRadius(17);
        this->checkBox->setBackgroundColor(nvgRGBA(0, 161, 214, 255));
        this->checkBox->setJustifyContent(brls::JustifyContent::CENTER);
        this->checkBox->setAlignItems(brls::AlignItems::CENTER);
        auto* label = new brls::Label();
        label->setText("✓");
        label->setFontSize(22);
        label->setTextColor(nvgRGB(255, 255, 255));
        this->checkBox->addView(label);
        this->checkBox->setVisibility(brls::Visibility::GONE);
        this->addView(this->checkBox);
    }

    void setChecked(bool checked) {
        this->checkBox->setVisibility(checked ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
    }

private:
    brls::Box* checkBox = nullptr;
};

}  // namespace

class DataSourceMineHistoryVideoList : public RecyclingGridDataSource {
public:
    explicit DataSourceMineHistoryVideoList(bilibili::HistoryVideoListResult result) : list(std::move(result)) {
        this->sortAndBuild();
    }

    bool selecting = false;
    std::set<std::string> selected;

    bool isSelecting() const { return selecting; }

    void setSelecting(bool value) {
        selecting = value;
        if (!value) selected.clear();
    }

    const std::set<std::string>& getSelected() const { return selected; }

    void toggleSelected(const std::string& bvid) {
        if (selected.count(bvid)) {
            selected.erase(bvid);
        } else {
            selected.insert(bvid);
        }
    }

    void selectAll() {
        for (const auto& item : list) selected.insert(item.history.bvid);
    }

    void clearSelection() { selected.clear(); }

    /// 0=今天 1=昨天 2=更早；整组已选中时再次点击则取消整组
    void selectRange(int which) {
        std::string today     = localDayKey((int64_t)wiliwili::getUnixTime());
        std::string yesterday = localDayKey((int64_t)wiliwili::getUnixTime() - 86400);

        std::vector<std::string> targets;
        for (const auto& item : list) {
            const std::string& bvid = item.history.bvid;
            if (bvid.empty()) continue;
            std::string key = localDayKey(item.view_at);
            bool match      = false;
            if (which == 0) match = key == today;
            else if (which == 1) match = key == yesterday;
            else match = key != today && key != yesterday;
            if (match) targets.push_back(bvid);
        }
        if (targets.empty()) return;

        bool allSelected = true;
        for (const auto& bvid : targets) {
            if (!selected.count(bvid)) {
                allSelected = false;
                break;
            }
        }
        for (const auto& bvid : targets) {
            if (allSelected) selected.erase(bvid);
            else selected.insert(bvid);
        }
    }

    RecyclingGridItem* cellForRow(RecyclingGrid* recycler, size_t index) override {
        if (index >= rows.size()) return nullptr;
        if (rows[index].blank) return recycler->dequeueReusableCell("Blank");

        auto* item = (SelectableHistoryCard*)recycler->dequeueReusableCell("Cell");

        bilibili::HistoryVideoResult& r = this->list[rows[index].dataIndex];
        auto badge                      = r.badge;
        if (badge.empty() && r.progress == -1) {
            badge = "wiliwili/mine/done"_i18n;
        }
        if (rows[index].firstOfDay) {
            std::string dayLabel = historyDayLabel(r.view_at);
            badge                = badge.empty() ? dayLabel : dayLabel + " · " + badge;
        }
        auto author     = r.author_name;
        bool showUpName = true;
        if (author.empty()) {
            showUpName = false;
            author     = r.show_title;
        } else if (!r.show_title.empty()) {
            author += " - " + r.show_title;
        }
        auto time = wiliwili::sec2TimeDate(r.view_at);

        std::string duration;
        float progress = -1;
        if (r.duration >= 0 && r.progress >= 0 && (r.history.business == "pgc" || r.history.business == "archive")) {
            duration = wiliwili::sec2Time(r.progress) + "/" + wiliwili::sec2Time(r.duration);
            progress = r.progress * 1.0 / r.duration;
        } else if (r.progress < 0) {
            progress = 1.0;
        }

        item->setCard(r.cover + ImageHelper::h_ext, r.title, author, time, duration, badge, r.history.dt, progress,
                      showUpName);

        auto rowData = r;
        item->setChecked(this->selecting && this->selected.count(rowData.history.bvid) > 0);
        item->registerClickAction([this, item, rowData](brls::View*) {
            if (this->selecting) {
                std::string bvid = rowData.history.bvid;
                if (bvid.empty()) return true;
                this->toggleSelected(bvid);
                item->setChecked(this->selected.count(bvid) > 0);
                return true;
            }
            openHistoryItem(rowData);
            return true;
        });

        return item;
    }

    size_t getItemCount() override { return rows.size(); }

    void onItemSelected(RecyclingGrid* recycler, size_t index) override {
        if (index >= rows.size() || rows[index].blank) return;
        auto& item = this->list[rows[index].dataIndex];
        if (this->selecting) {
            if (item.history.bvid.empty()) return;
            this->toggleSelected(item.history.bvid);
            auto* card = dynamic_cast<SelectableHistoryCard*>(recycler->getGridItemByIndex(index));
            if (card) card->setChecked(this->selected.count(item.history.bvid) > 0);
            return;
        }
        openHistoryItem(item);
    }

    void appendData(const bilibili::HistoryVideoListResult& data) {
        this->list.insert(this->list.end(), data.begin(), data.end());
        this->sortAndBuild();
    }

    void clearData() override {
        this->list.clear();
        this->rows.clear();
        this->selected.clear();
    }

private:
    struct Row {
        bool blank;
        size_t dataIndex;
        bool firstOfDay;
    };

    void sortAndBuild() {
        std::stable_sort(this->list.begin(), this->list.end(),
                         [](const bilibili::HistoryVideoResult& a, const bilibili::HistoryVideoResult& b) {
                             return a.view_at > b.view_at;
                         });
        this->rows.clear();
        int column       = 0;
        std::string last;
        for (size_t i = 0; i < this->list.size(); i++) {
            std::string day = localDayKey(this->list[i].view_at);
            if (i > 0 && day != last && column != 0) {
                while (column != 0) {
                    this->rows.push_back({true, 0, false});
                    column = (column + 1) % HISTORY_GRID_SPAN;
                }
            }
            this->rows.push_back({false, i, i == 0 || day != last});
            column = (column + 1) % HISTORY_GRID_SPAN;
            last   = day;
        }
    }

    bilibili::HistoryVideoListResult list;
    std::vector<Row> rows;
};

MineHistory::MineHistory() {
    this->inflateFromXMLRes("xml/fragment/mine_history.xml");
    brls::Logger::debug("Fragment MineHistory: create");

    recyclingGrid->registerCell("Cell", []() { return new SelectableHistoryCard(); });
    recyclingGrid->registerCell("Blank", []() { return new BlankHistoryCell(); });
    recyclingGrid->onNextPage([this]() {
        // 未登录时列表数据全部来自本地，不需要分页，避免滚动到底部时反复刷新导致回弹
        if (!ProgramConfig::instance().hasLoginInfo()) return;
        this->requestData();
    });
    recyclingGrid->setRefreshAction([this]() {
        AutoTabFrame::focus2Sidebar(this);
        this->recyclingGrid->showSkeleton();
        this->requestData(true);
    });

    this->bottomBar->setVisibility(brls::Visibility::GONE);

    if (ProgramConfig::instance().hasLoginInfo()) {
        // 本地列表的筛选/删除功能只对未登录的本地数据开放
        this->topBar->setVisibility(brls::Visibility::GONE);
    } else {
        auto bind = [this](brls::Box* box, std::function<void()> action) {
            box->registerClickAction([action](brls::View*) {
                action();
                return true;
            });
            box->addGestureRecognizer(new brls::TapGestureRecognizer(box));
        };

        bind(this->btnToday, [this]() { this->selectRange(0); });
        bind(this->btnYesterday, [this]() { this->selectRange(1); });
        bind(this->btnOlder, [this]() { this->selectRange(2); });
        bind(this->btnDone, [this]() {
            if (auto* ds = this->getDataSource(); ds && ds->isSelecting()) {
                this->exitSelection();
            } else {
                this->enterSelection();
            }
        });
        bind(this->btnSelectAll, [this]() {
            if (auto* ds = this->getDataSource(); ds) {
                this->enterSelection();
                ds->selectAll();
                this->recyclingGrid->reloadData();
            }
        });
        bind(this->btnDelete, [this]() { this->deleteSelected(); });
    }

    this->requestData();
}

MineHistory::~MineHistory() { brls::Logger::debug("Fragment MineHistoryActivity: delete"); }

brls::View* MineHistory::create() { return new MineHistory(); }

void MineHistory::onCreate() {
    this->registerTabAction("wiliwili/mine/refresh_history"_i18n, brls::ControllerButton::BUTTON_X,
                            ShortcutHelper::getRefresh(),
                            [this](brls::View* view) -> bool {
                                this->recyclingGrid->refresh();
                                return true;
                            });
}

DataSourceMineHistoryVideoList* MineHistory::getDataSource() {
    return dynamic_cast<DataSourceMineHistoryVideoList*>(this->recyclingGrid->getDataSource());
}

void MineHistory::enterSelection() {
    auto* ds = this->getDataSource();
    if (!ds) return;
    ds->setSelecting(true);
    this->bottomBar->setVisibility(brls::Visibility::VISIBLE);
    this->recyclingGrid->reloadData();
}

void MineHistory::exitSelection() {
    auto* ds = this->getDataSource();
    if (ds) {
        ds->setSelecting(false);
        ds->clearSelection();
    }
    this->bottomBar->setVisibility(brls::Visibility::GONE);
    this->recyclingGrid->reloadData();
}

void MineHistory::selectRange(int which) {
    auto* ds = this->getDataSource();
    if (!ds) return;
    if (!ds->isSelecting()) this->enterSelection();
    ds->selectRange(which);
    this->recyclingGrid->reloadData();
}

void MineHistory::deleteSelected() {
    auto* ds = this->getDataSource();
    if (!ds) return;
    if (ds->getSelected().empty()) {
        ToastHelper::show("wiliwili/mine/local_no_selection"_i18n);
        return;
    }

    DialogHelper::showCancelableDialog("wiliwili/mine/local_delete_confirm"_i18n, [this]() {
        auto* source = this->getDataSource();
        if (!source) return;
        auto selected = source->getSelected();
        for (const auto& bvid : selected) {
            LocalStore::instance().removeHistory(bvid);
        }
        ToastHelper::show("wiliwili/mine/local_deleted"_i18n);
        this->exitSelection();
        this->recyclingGrid->showSkeleton();
        this->requestData(true);
    });
}

void MineHistory::onHistoryList(const bilibili::HistoryVideoResultWrapper& result) {
    for (const auto& i : result.list) {
        brls::Logger::verbose("history: {}: {}", i.title, i.progress);
    }

    int view_at = this->cursor.view_at;
    brls::Threading::sync([this, result, view_at]() {
        auto* datasource = dynamic_cast<DataSourceMineHistoryVideoList*>(recyclingGrid->getDataSource());
        if (datasource && view_at != 0) {
            if (!result.list.empty()) {
                datasource->appendData(result.list);
                recyclingGrid->notifyDataChanged();
            }
        } else {
            recyclingGrid->setDataSource(new DataSourceMineHistoryVideoList(result.list));
            this->bottomBar->setVisibility(brls::Visibility::GONE);
        }
    });
}

void MineHistory::onError(const std::string& error) {
    brls::Logger::error("MineHistory::onError: {}", error);
    brls::sync([this, error]() { this->recyclingGrid->setError(error); });
}
