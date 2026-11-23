// Copyright 2026 LingmoOS Contributors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "shell/stage/stage_window.h"

#include <algorithm>
#include <chrono>

#include "shell/shell_window.h"
#include "shell/shell_data.h"
#include "shell/common/task_manager.h"
#include "ui/button.h"
#include "ui/animation.h"
#include "ui/image_view.h"
#include "ui/icon.h"
#include "ui/label.h"
#include "ui/painter.h"
#include "ui/scroll_view.h"
#include "ui/theme.h"
#include "ui/view.h"
#include "wm/stage/stage_manager.h"
#include "wm/window.h"
#include "wm/window_manager.h"

namespace Lingmo::Shell {

namespace {

constexpr int kWidth = 220;
constexpr int kCardHeight = 112;
constexpr int kGap = 12;
constexpr int kMargin = 12;
constexpr int kEdgeInset = 10;

class Card final : public ui::View
{
public:
    Card()
    {
        setLayered(true);
        m_image = addChild<ui::ImageView>();
        m_image->setFillMode(ui::ImageView::FillMode::PreserveAspectCrop);
        m_image->setRendering(ui::ImageView::Rendering::MaskedLayer);
        m_image->setCornerRadius(8);
        m_title = addChild<ui::Label>();
        m_title->setVerticallyCentered(true);
        m_title->setElide(ui::Label::Elide::Right);
        m_title->setColor(ui::Color {255, 255, 255, 255});
        m_icon = addChild<ui::ThemeIcon>("application-x-executable", 26);
        m_icon->setColor(ui::Color {255, 255, 255, 255});
        setTracksHover(true);
    }

    std::function<void()> onClicked;
    std::function<void()> onDragOut;

    bool acceptsPointer() const override { return true; }

    void setThumbnail(const ui::Bitmap &bitmap)
    {
        if (bitmap.isNull())
            return;
        m_image->setPixels(bitmap.width(), bitmap.height(),
                           reinterpret_cast<const uint32_t *>(bitmap.constBits()),
                           bitmap.stride(), true);
        requestPaint();
    }

    void clearThumbnail()
    {
        m_image->setSource({});
    }

    void setSelected(bool selected)
    {
        if (selected == m_selected)
            return;
        m_selected = selected;
        requestPaint();
    }

    void setTitle(std::string title) { m_title->setText(std::move(title)); }

    void setApplicationIcon(std::string icon)
    {
        m_icon->setIconName(icon.empty() ? "application-x-executable" : std::move(icon));
    }

    void setStackCount(size_t count)
    {
        m_stackCount = std::min<size_t>(count, 3);
        requestPaint();
    }

protected:
    void pointerPress(const ui::PointerEvent &event) override
    {
        m_press = event.position;
        m_dragged = false;
    }

    void pointerMove(const ui::PointerEvent &event) override
    {
        if (m_press && !m_dragged) {
            const double dx = event.position.x - m_press->x;
            const double dy = event.position.y - m_press->y;
            if (std::hypot(dx, dy) >= 12)
                m_dragged = true;
        }
        if (m_dragged)
            setOpacity(0.78);
    }

    void pointerRelease(const ui::PointerEvent &event) override
    {
        const bool dragged = m_dragged;
        m_press.reset();
        m_dragged = false;
        setOpacity(1);
        if (dragged) {
            if (event.position.x < 0 && onDragOut)
                onDragOut();
        } else if (event.button == ui::MouseButton::Left && onClicked) {
            onClicked();
        }
    }

    void layout() override
    {
        m_image->setGeometry({8, 8, geometry().width - 16, geometry().height - 34});
        m_title->setGeometry({44, geometry().height - 27, geometry().width - 52, 20});
        m_icon->setGeometry({10, geometry().height - 32, 26, 26});
    }

    void paint(ui::Painter &painter) override
    {
        const auto &theme = ui::Theme::current();
        const ui::Color fill = m_selected ? theme.highlight.withAlpha(0.24)
                                         : theme.secondBackground.withAlpha(0.88);
        for (size_t i = m_stackCount; i > 1; --i) {
            const double offset = double(i - 1) * 4;
            const ui::RectF back {offset, offset, geometry().width - offset,
                                  geometry().height - offset};
            painter.drawShadow(back, theme.mediumRadius, 8,
                               ui::Color {0, 0, 0, 90}, {0, 2});
            painter.fillRoundedRect(back, theme.mediumRadius,
                                    theme.secondBackground.withAlpha(0.68));
        }
        painter.drawShadow(localRect(), theme.mediumRadius, 12,
                           ui::Color {0, 0, 0, 100}, {0, 3});
        painter.fillRoundedRect(localRect(), theme.mediumRadius, fill);
        if (m_selected)
            painter.drawRoundedRect(localRect(), {theme.mediumRadius, 0, ui::Corners::Continuous},
                                    {}, 1, theme.highlight.withAlpha(0.65));
    }

private:
    bool m_selected = false;
    ui::ImageView *m_image = nullptr;
    ui::Label *m_title = nullptr;
    ui::ThemeIcon *m_icon = nullptr;
    size_t m_stackCount = 1;
    std::optional<ui::PointF> m_press;
    bool m_dragged = false;
};

} // namespace

class StageWindow::View final : public ui::View
{
public:
    explicit View(StageWindow *owner) : m_owner(owner)
    {
        setName("stage-manager-strip");
        m_scroll = addChild<ui::ScrollView>();
        auto content = std::make_unique<ui::View>();
        m_content = content.get();
        m_scroll->setContentView(std::move(content));
    }

    void sync(const wm::StageManager::Snapshot &snapshot)
    {
        m_snapshot = snapshot;
        m_visibleGroups.clear();
        for (const auto &group : m_snapshot.groups) {
            const auto active = m_snapshot.active.find(group.output);
            if (active == m_snapshot.active.end() || active->second != group.id)
                m_visibleGroups.push_back(group);
        }
        while (m_cards.size() < m_visibleGroups.size()) {
            auto *card = m_content->addChild<Card>();
            m_cards.push_back(card);
        }
        for (size_t i = 0; i < m_cards.size(); ++i) {
            const bool visible = i < m_visibleGroups.size();
            m_cards[i]->setVisible(visible);
            if (!visible)
                continue;
            const auto &group = m_visibleGroups[i];
            std::string title = "Stage " + std::to_string(i + 1);
            std::string icon = "application-x-executable";
            if (!group.windows.empty()) {
                if (wm::Window *window = m_owner->m_shell->windowManager()->windowById(group.windows.front())) {
                    if (!window->title().empty())
                        title = window->title();
                    if (!window->applicationId().empty())
                        icon = window->applicationId();
                    if (const auto entry = m_owner->m_shell->tasks().entryForWindow(window->id())) {
                        if (!entry->name.empty())
                            title = entry->name;
                        if (!entry->icon.empty())
                            icon = entry->icon;
                    }
                }
            }
            m_cards[i]->setTitle(std::move(title));
            m_cards[i]->setApplicationIcon(std::move(icon));
            m_cards[i]->setStackCount(group.windows.size());
            m_cards[i]->setSelected(false);
            m_cards[i]->onClicked = [this, id = group.id] {
                m_owner->m_shell->windowManager()->stageManager()->switchTo(id);
            };
            m_cards[i]->onDragOut = [this, id = group.id, groupIndex = i] {
                if (groupIndex < m_visibleGroups.size()
                    && !m_visibleGroups[groupIndex].windows.empty()) {
                    m_owner->m_shell->windowManager()->stageManager()->separateWindow(
                            m_visibleGroups[groupIndex].windows.front());
                }
            };
        }
        requestLayout();
        requestPaint();
    }

    void setThumbnail(wm::StageManager::Id group, const ui::Bitmap &bitmap)
    {
        const auto it = std::find_if(m_visibleGroups.begin(), m_visibleGroups.end(),
                                     [group](const auto &candidate) { return candidate.id == group; });
        if (it == m_visibleGroups.end())
            return;
        const size_t index = size_t(it - m_visibleGroups.begin());
        if (index < m_cards.size())
            m_cards[index]->setThumbnail(bitmap);
    }

    size_t visibleGroupCount() const { return m_visibleGroups.size(); }

protected:
    void layout() override
    {
        const double width = geometry().width;
        const double visibleHeight = geometry().height - 2 * kMargin;
        m_scroll->setGeometry({kMargin, kMargin, width - 2 * kMargin, visibleHeight});
        m_content->setGeometry({0, 0, width - 2 * kMargin,
                                double(m_visibleGroups.size()) * (kCardHeight + kGap)});
        for (size_t i = 0; i < m_cards.size(); ++i) {
            if (!m_cards[i]->isVisible())
                continue;
            const double y = i * (kCardHeight + kGap);
            m_cards[i]->setGeometry({0, y, width - 4 * kMargin, kCardHeight});
        }
    }

    void paint(ui::Painter &painter) override
    {
        const auto &theme = ui::Theme::current();
        painter.fillRoundedRect(localRect(), theme.hugeRadius,
                                theme.background.withAlpha(0.82));
    }

private:
    StageWindow *m_owner;
    wm::StageManager::Snapshot m_snapshot;
    std::vector<wm::StageManager::Group> m_visibleGroups;
    ui::ScrollView *m_scroll = nullptr;
    ui::View *m_content = nullptr;
    std::vector<Card *> m_cards;
};

StageWindow::StageWindow(Shell *shell, std::unique_ptr<ScreenGrabber> grabber)
    : m_shell(shell)
    , m_grabber(std::move(grabber))
{
    m_window = std::make_unique<ShellWindow>(shell, wm::WindowRole::Panel);
    m_window->setFocusPolicy(wm::FocusPolicy::NoFocus);
    m_window->setPanelBehavior(wm::PanelBehavior::StayAbove);
    auto view = std::make_unique<View>(this);
    m_view = view.get();
    m_window->setContentView(std::move(view));
    m_window->setSize({kWidth, kCardHeight + 2 * kMargin});
    m_window->setBackgroundBlur({0, 0, kWidth, kCardHeight + 2 * kMargin}, 18,
                                popupGlass(ui::Theme::current().darkMode ? .10 : .92,
                                           ui::Theme::current().darkMode ? .10 : .92,
                                           ui::Theme::current().darkMode ? .12 : .94, .78));

    auto *manager = m_shell->windowManager()->stageManager();
    m_subscriptions.push_back(manager->changed.connect([this](wm::StageManager::Snapshot snapshot) {
        sync(snapshot);
    }));
    m_subscriptions.push_back(m_shell->outputsChanged.connect([this] { place(); }));
    m_snapshot = manager->snapshot();
    manager->setEnabled(true);
    sync(m_snapshot);
    refreshThumbnails();
    place();
    updateVisibility();
}

StageWindow::~StageWindow()
{
    *m_alive = false;
    m_subscriptions.clear();
    if (m_visibilityAnimation)
        m_visibilityAnimation->stop();
    m_visibilityAnimation.reset();
    m_window.reset();
}

bool StageWindow::isShown() const
{
    return m_window->isShown();
}

void StageWindow::toggle()
{
    m_userVisible = !m_userVisible;
    updateVisibility();
}

void StageWindow::sync(const wm::StageManager::Snapshot &snapshot)
{
    m_snapshot = snapshot;
    if (m_view)
        m_view->sync(snapshot);
    place();
    updateVisibility();
    refreshThumbnails();
}

void StageWindow::refreshThumbnails()
{
    if (!m_grabber || m_capturePending || !m_view)
        return;
    const auto group = std::find_if(m_snapshot.groups.begin(), m_snapshot.groups.end(),
                                    [this](const auto &item) {
                                        return !item.windows.empty()
                                        && (!m_captured.contains(item.id)
                                            || m_captured[item.id] != item.contentRevision);
                                    });
    if (group == m_snapshot.groups.end())
        return;
    wm::Window *window = m_shell->windowManager()->windowById(group->windows.front());
    if (!window)
        return;
    const auto id = group->id;
    const uint64_t revision = group->contentRevision;
    m_capturePending = true;
    m_grabber->grabWindow(window, false,
                          [this, alive = m_alive, id, revision](std::vector<ui::Bitmap> images) {
                              if (!*alive)
                                  return;
                              m_capturePending = false;
                              const auto current = std::find_if(
                                      m_snapshot.groups.begin(), m_snapshot.groups.end(),
                                      [id](const auto &item) { return item.id == id; });
                              if (current != m_snapshot.groups.end()
                                  && current->contentRevision == revision
                                  && !images.empty() && !images.front().isNull()) {
                                  if (images.front().width() > 320
                                      || images.front().height() > 180) {
                                      const base::Size size = ui::scaledSize(
                                              images.front().size(), {320, 180},
                                              ui::Bitmap::Scaling::Fit);
                                      images.front() = images.front().scaled(size, true);
                                  }
                                  m_captured[id] = revision;
                                  m_view->setThumbnail(id, images.front());
                              }
                              refreshThumbnails();
                          });
}

void StageWindow::place()
{
    const Shell::Output *output = m_shell->output({});
    if (!output)
        return;
    const base::Rect workArea = m_shell->workArea(*output);
    if (workArea.isEmpty())
        return;
    const int height = std::clamp(kCardHeight + 2 * kMargin,
                                  kCardHeight + 2 * kMargin,
                                  workArea.height() - 2 * kEdgeInset);
    m_window->setOutput(output->name);
    const int stripHeight = std::min(workArea.height() - 2 * kEdgeInset,
                                     std::max(height, int(m_view->visibleGroupCount())
                                                              * (kCardHeight + kGap)
                                                              + 2 * kMargin));
    m_window->setGeometry({workArea.left() + kEdgeInset, workArea.top() + kEdgeInset,
                           kWidth, stripHeight});
}

void StageWindow::updateVisibility()
{
    const bool shouldShow = m_userVisible && m_snapshot.enabled && !m_snapshot.blocked
            && m_view && m_view->visibleGroupCount() > 0;
    if (m_visibilityAnimation)
        m_visibilityAnimation->stop();

    double from = m_view->opacity();
    const double to = shouldShow ? 1.0 : 0.0;
    if (shouldShow && !m_window->isShown()) {
        from = 0;
        m_view->setOpacity(0);
        m_window->show();
        m_view->setEnabled(true);
    } else if (shouldShow) {
        m_view->setEnabled(true);
    } else {
        m_view->setEnabled(false);
        if (!m_window->isShown()) {
            m_view->setOpacity(0);
            return;
        }
    }
    from = m_view->opacity();
    if (!shouldShow && from == 0) {
        m_window->hide();
        return;
    }
    if (from == to)
        return;

    if (!m_visibilityAnimation)
        m_visibilityAnimation = std::make_unique<ui::Animation>(m_view->window());
    m_visibilityAnimation->setDuration(std::chrono::milliseconds(150));
    m_visibilityAnimation->setEasing(ui::Animation::Easing::OutCubic);
    m_visibilityAnimation->onUpdate = [this, from, to](double progress) {
        m_view->setOpacity(from + (to - from) * progress);
    };
    m_visibilityAnimation->onFinished = [this, shouldShow] {
        m_view->setOpacity(shouldShow ? 1.0 : 0.0);
        if (!shouldShow && m_window->isShown())
            m_window->hide();
    };
    m_visibilityAnimation->start();
}

} // namespace Lingmo::Shell
