// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "controller.h"
#include "gui/loadingwidget.h"
#include "gui/navigationbar.h"
#include "gui/pluginsui.h"
#include "gui/windowstatusbar.h"
#include "gui/workspacewidget.h"
#include "services/window/windowservice.h"
#include "services/window/windowcontroller.h"
#include "services/project/projectservice.h"
#include "modules/abstractmodule.h"
#include "modules/pluginmanagermodule.h"
#include "modules/documentfindmodule.h"
#include "modules/contextmodule.h"
#include "modules/notificationmodule.h"
#include "locator/locatormanager.h"
#include "find/placeholdermanager.h"
#include "common/util/utils.h"

#include <DFrame>
#include <DFileDialog>
#include <DTitlebar>
#include <DStackedWidget>
#include <DSearchEdit>
#include <DFontSizeManager>

#include <QDebug>
#include <QShortcut>
#include <QDesktopServices>
#include <QMenuBar>
#include <QDesktopWidget>
#include <QScreen>
#include <QApplication>

static Controller *ins { nullptr };

//WN = window name
inline const QString WN_CONTEXTWIDGET = "contextWidget";
inline const QString WN_LOADINGWIDGET = "loadingWidget";
inline const QString WN_WORKSPACE = "workspaceWidget";

// MW = MainWindow
inline constexpr int MW_WIDTH { 1280 };
inline constexpr int MW_HEIGHT { 860 };

inline constexpr int MW_MIN_WIDTH { 1280 };
inline constexpr int MW_MIN_HEIGHT { 600 };
using namespace dpfservice;

DWIDGET_USE_NAMESPACE

struct WidgetInfo
{
    QString name;
    DWidget *widget;
    Position pos;
    bool replace;
    bool isVisible;
};

class ControllerPrivate
{
    MainWindow *mainWindow { nullptr };
    loadingWidget *loadingwidget { nullptr };
    WorkspaceWidget *workspace { nullptr };
    bool showWorkspace { nullptr };

    QMap<QString, DWidget *> widgetWaitForAdd;
    QMap<QString, DWidget *> addedWidget;

    DWidget *navigationToolBar { nullptr };
    NavigationBar *navigationBar { nullptr };
    QMap<QString, QAction *> navigationActions;

    DWidget *leftTopToolBar { nullptr };
    DSearchEdit *locatorBar { nullptr };
    DWidget *rightTopToolBar { nullptr };
    QMap<QAction *, DToolButton *> topToolBtn;

    QMap<QString, DWidget *> contextWidgets;
    QMap<QString, DPushButton *> tabButtons;
    DWidget *contextWidget { nullptr };
    DStackedWidget *stackContextWidget { nullptr };
    DFrame *contextTabBar { nullptr };
    QHBoxLayout *contextButtonLayout { nullptr };
    bool contextWidgetAdded { false };

    WindowStatusBar *statusBar { nullptr };

    DMenu *menu { nullptr };

    QStringList hiddenWidgetList;

    QStringList validModeList { CM_EDIT, CM_DEBUG, CM_RECENT };
    QMap<QString, QString> modePluginMap { { CM_EDIT, MWNA_EDIT }, { CM_RECENT, MWNA_RECENT }, { CM_DEBUG, MWNA_DEBUG } };
    QString mode { "" };   //mode: CM_EDIT/CM_DEBUG/CM_RECENT
    QMap<QString, QList<WidgetInfo>> modeInfo;
    QString currentNavigation { "" };

    QMap<QString, AbstractModule *> modules;

    friend class Controller;
};

Controller *Controller::instance()
{
    if (!ins)
        ins = new Controller;
    return ins;
}

void Controller::registerModule(const QString &moduleName, AbstractModule *module)
{
    Q_ASSERT(module && !moduleName.isEmpty());

    d->modules.insert(moduleName, module);
}

MainWindow *Controller::mainWindow() const
{
    return d->mainWindow;
}

Controller::Controller(QObject *parent)
    : QObject(parent), d(new ControllerPrivate)
{
    initMainWindow();
    initNavigationBar();
    initContextWidget();
    initStatusBar();
    initWorkspaceWidget();
    initTopToolBar();
    registerService();

    registerModule("pluginManagerModule", new PluginManagerModule());
    registerModule("docFindModule", new DocumentFindModule());
    registerModule("contextModule", new ContextModule());
    registerModule("notifyModule", new NotificationModule());
    initModules();
}

void Controller::registerService()
{
    auto &ctx = dpfInstance.serviceContext();
    WindowService *windowService = ctx.service<WindowService>(WindowService::name());

    using namespace std::placeholders;
    if (!windowService->raiseMode) {
        windowService->raiseMode = std::bind(&Controller::raiseMode, this, _1);
    }
    if (!windowService->replaceWidget) {
        windowService->replaceWidget = std::bind(&Controller::replaceWidget, this, _1, _2);
    }
    if (!windowService->insertWidget) {
        windowService->insertWidget = std::bind(&Controller::insertWidget, this, _1, _2, _3);
    }
    if (!windowService->hideWidget) {
        windowService->hideWidget = std::bind(&Controller::hideWidget, this, _1);
    }
    if (!windowService->registerWidgetToMode) {
        windowService->registerWidgetToMode = std::bind(&Controller::registerWidgetToMode, this, _1, _2, _3, _4, _5, _6);
    }
    if (!windowService->registerWidget) {
        windowService->registerWidget = std::bind(&Controller::registerWidget, this, _1, _2);
    }
    if (!windowService->showWidgetAtPosition) {
        windowService->showWidgetAtPosition = std::bind(&Controller::showWidgetAtPosition, this, _1, _2, _3);
    }
    if (!windowService->setDockHeaderName) {
        windowService->setDockHeaderName = std::bind(&MainWindow::setDockHeadername, d->mainWindow, _1, _2);
    }
    if (!windowService->deleteDockHeader) {
        windowService->deleteDockHeader = std::bind(&MainWindow::deleteDockHeader, d->mainWindow, _1);
    }
    if (!windowService->addToolBtnToDockHeader) {
        windowService->addToolBtnToDockHeader = std::bind(&MainWindow::addToolBtnToDockHeader, d->mainWindow, _1, _2);
    }
    if (!windowService->setDockWidgetFeatures) {
        windowService->setDockWidgetFeatures = std::bind(&MainWindow::setDockWidgetFeatures, d->mainWindow, _1, _2);
    }
    if (!windowService->splitWidgetOrientation) {
        windowService->splitWidgetOrientation = std::bind(&MainWindow::splitWidgetOrientation, d->mainWindow, _1, _2, _3);
    }
    if (!windowService->addNavigationItem) {
        windowService->addNavigationItem = std::bind(&Controller::addNavigationItem, this, _1, _2);
    }
    if (!windowService->addNavigationItemToBottom) {
        windowService->addNavigationItemToBottom = std::bind(&Controller::addNavigationItemToBottom, this, _1, _2);
    }
    if (!windowService->switchWidgetNavigation) {
        windowService->switchWidgetNavigation = std::bind(&Controller::switchWidgetNavigation, this, _1);
    }
    if (!windowService->getAllNavigationItemName) {
        windowService->getAllNavigationItemName = std::bind(&NavigationBar::getAllNavigationItemName, d->navigationBar);
    }
    if (!windowService->getPriorityOfNavigationItem) {
        windowService->getPriorityOfNavigationItem = std::bind(&NavigationBar::getPriorityOfNavigationItem, d->navigationBar, _1);
    }
    if (!windowService->addContextWidget) {
        windowService->addContextWidget = std::bind(&Controller::addContextWidget, this, _1, _2, _3);
    }
    if (!windowService->hasContextWidget) {
        windowService->hasContextWidget = std::bind(&Controller::hasContextWidget, this, _1);
    }
    if (!windowService->showContextWidget) {
        windowService->showContextWidget = std::bind(&Controller::showContextWidget, this);
    }
    if (!windowService->hideContextWidget) {
        windowService->hideContextWidget = std::bind(&Controller::hideContextWidget, this);
    }
    if (!windowService->switchContextWidget) {
        windowService->switchContextWidget = std::bind(&Controller::switchContextWidget, this, _1);
    }
    if (!windowService->addChildMenu) {
        windowService->addChildMenu = std::bind(&Controller::addChildMenu, this, _1);
    }
    if (!windowService->insertAction) {
        windowService->insertAction = std::bind(&Controller::insertAction, this, _1, _2, _3);
    }
    if (!windowService->addAction) {
        windowService->addAction = std::bind(&Controller::addAction, this, _1, _2);
    }
    if (!windowService->removeActions) {
        windowService->removeActions = std::bind(&Controller::removeActions, this, _1);
    }
    if (!windowService->addOpenProjectAction) {
        windowService->addOpenProjectAction = std::bind(&Controller::addOpenProjectAction, this, _1, _2);
    }
    if (!windowService->addWidgetToTopTool) {
        windowService->addWidgetToTopTool = std::bind(&Controller::addWidgetToTopTool, this, _1, _2, _3, _4);
    }
    if (!windowService->addTopToolItem) {
        windowService->addTopToolItem = std::bind(&Controller::addTopToolItem, this, _1, _2, _3);
    }
    if (!windowService->addTopToolItemToRight) {
        windowService->addTopToolItemToRight = std::bind(&Controller::addTopToolItemToRight, this, _1, _2, _3);
    }
    if (!windowService->showTopToolBar) {
        windowService->showTopToolBar = std::bind(&Controller::showTopToolBar, this);
    }
    if (!windowService->removeTopToolItem) {
        windowService->removeTopToolItem = std::bind(&Controller::removeTopToolItem, this, _1);
    }
    if (!windowService->hideTopToolBar) {
        windowService->hideTopToolBar = std::bind(&MainWindow::hideTopTollBar, d->mainWindow);
    }
    if (!windowService->showStatusBar) {
        windowService->showStatusBar = std::bind(&Controller::showStatusBar, this);
    }
    if (!windowService->hideStatusBar) {
        windowService->hideStatusBar = std::bind(&Controller::hideStatusBar, this);
    }
    if (!windowService->addStatusBarItem) {
        windowService->addStatusBarItem = std::bind(&Controller::addStatusBarItem, this, _1);
    }
    if (!windowService->addWidgetWorkspace) {
        windowService->addWidgetWorkspace = std::bind(&WorkspaceWidget::addWorkspaceWidget, d->workspace, _1, _2, _3);
    }
    if (!windowService->registerToolBtnToWorkspaceWidget) {
        windowService->registerToolBtnToWorkspaceWidget = std::bind(&WorkspaceWidget::registerToolBtnToWidget, d->workspace, _1, _2);
    }
    if (!windowService->createFindPlaceHolder) {
        windowService->createFindPlaceHolder = std::bind(&PlaceHolderManager::createPlaceHolder, PlaceHolderManager::instance(), _1, _2);
    }
    if (!windowService->getCurrentDockName) {
        windowService->getCurrentDockName = std::bind(&MainWindow::getCurrentDockName, d->mainWindow, _1);
    }
}

Controller::~Controller()
{
    if (d->mainWindow)
        delete d->mainWindow;
    foreach (auto module, d->modules.values()) {
        delete module;
        module = nullptr;
    }
    if (d)
        delete d;
}

void Controller::raiseMode(const QString &mode)
{
    if (!d->validModeList.contains(mode)) {
        qWarning() << "mode can only choose CM_RECENT / CM_EDIT / CM_DEBUG";
        return;
    }

    auto widgetInfoList = d->modeInfo[mode];
    foreach (auto widgetInfo, widgetInfoList) {
        if (widgetInfo.replace)
            d->mainWindow->hideWidget(widgetInfo.pos);
        d->mainWindow->showWidget(widgetInfo.name);
        //widget in mainWindow is Dock(widget), show dock and hide widget.
        if (!widgetInfo.isVisible)
            widgetInfo.widget->hide();
    }

    if (mode == CM_RECENT) {
        d->mode = mode;
        return;
    }

    if (mode == CM_EDIT)
        showWorkspace();

    showTopToolBar();
    showContextWidget();
    showStatusBar();

    d->mode = mode;
}

void Controller::replaceWidget(const QString &name, Position pos)
{
    showWidgetAtPosition(name, pos, true);
}

void Controller::insertWidget(const QString &name, Position pos, Qt::Orientation orientation)
{
    if (d->widgetWaitForAdd.contains(name)) {
        d->mainWindow->addWidget(name, d->widgetWaitForAdd[name], pos, orientation);
    } else if (d->addedWidget.contains(name)) {
        d->mainWindow->showWidget(name);
    } else {
        qWarning() << "no widget named:" << name;
        return;
    }

    d->addedWidget.insert(name, d->widgetWaitForAdd[name]);
    d->widgetWaitForAdd.remove(name);
}

void Controller::hideWidget(const QString &name)
{
    d->mainWindow->hideWidget(name);
}

void Controller::registerWidgetToMode(const QString &name, AbstractWidget *abstractWidget, const QString &mode, Position pos, bool replace, bool isVisible)
{
    if (!d->validModeList.contains(mode)) {
        qWarning() << "mode can only choose CM_RECENT / CM_EDIT / CM_DEBUG";
        return;
    }

    DWidget *qWidget = static_cast<DWidget *>(abstractWidget->qWidget());
    if (!qWidget->parent())
        qWidget->setParent(d->mainWindow);

    WidgetInfo widgetInfo;
    widgetInfo.name = name;
    widgetInfo.pos = pos;
    widgetInfo.replace = replace;
    widgetInfo.widget = qWidget;
    widgetInfo.isVisible = isVisible;

    d->addedWidget.insert(name, qWidget);
    d->mainWindow->addWidget(name, qWidget, pos);
    d->mainWindow->hideWidget(name);

    d->modeInfo[mode].append(widgetInfo);
}

void Controller::registerWidget(const QString &name, AbstractWidget *abstractWidget)
{
    if (d->widgetWaitForAdd.contains(name) || d->addedWidget.contains(name))
        return;

    auto widget = static_cast<DWidget *>(abstractWidget->qWidget());
    if (!widget->parent())
        widget->setParent(d->mainWindow);

    d->widgetWaitForAdd.insert(name, widget);
}

void Controller::showWidgetAtPosition(const QString &name, Position pos, bool replace)
{
    if (replace)
        d->mainWindow->hideWidget(pos);

    if (d->widgetWaitForAdd.contains(name)) {
        d->mainWindow->addWidget(name, d->widgetWaitForAdd[name], pos);
        d->addedWidget.insert(name, d->widgetWaitForAdd[name]);
        d->widgetWaitForAdd.remove(name);
    } else if (d->addedWidget.contains(name)) {
        d->mainWindow->showWidget(name);
    } else {
        qWarning() << "no widget named:" << name;
        return;
    }
}

void Controller::addNavigationItem(AbstractAction *action, quint8 priority)
{
    if (!action)
        return;
    auto inputAction = action->qAction();

    d->navigationBar->addNavItem(inputAction, NavigationBar::top, priority);
    d->navigationActions.insert(inputAction->text(), inputAction);
}

void Controller::addNavigationItemToBottom(AbstractAction *action, quint8 priority)
{
    if (!action)
        return;
    auto inputAction = action->qAction();

    d->navigationBar->addNavItem(inputAction, NavigationBar::bottom, priority);
    d->navigationActions.insert(inputAction->text(), inputAction);
}

void Controller::switchWidgetNavigation(const QString &navName)
{
    d->navigationBar->setNavActionChecked(navName, true);
    if (d->currentNavigation == navName)
        return;
    d->currentNavigation = navName;

    d->mainWindow->hideAllWidget();
    d->mainWindow->hideTopTollBar();
    hideStatusBar();

    if (d->modePluginMap.values().contains(navName))
        raiseMode(d->modePluginMap.key(navName));
    d->navigationActions[navName]->trigger();

    //send event
    uiController.switchToWidget(navName);
}

void Controller::addContextWidget(const QString &title, AbstractWidget *contextWidget, bool isVisible)
{
    DWidget *qWidget = static_cast<DWidget *>(contextWidget->qWidget());
    if (!qWidget) {
        return;
    }
    d->contextWidgets.insert(title, qWidget);

    d->stackContextWidget->addWidget(qWidget);
    DPushButton *tabBtn = new DPushButton(title);
    tabBtn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    tabBtn->setCheckable(true);
    tabBtn->setFixedHeight(28);
    tabBtn->setFlat(true);
    tabBtn->setFocusPolicy(Qt::NoFocus);
    DFontSizeManager *fontSizeManager = DFontSizeManager::instance();
    QFont font = fontSizeManager->t7();
    tabBtn->setFont(font);
    if (!isVisible)
        tabBtn->hide();

    d->contextButtonLayout->addWidget(tabBtn);
    connect(tabBtn, &DPushButton::clicked, qWidget, [=] {
        switchContextWidget(title);
    });

    d->tabButtons.insert(title, tabBtn);
}

void Controller::showContextWidget()
{
    if (!d->contextWidgetAdded) {
        d->mainWindow->addWidget(WN_CONTEXTWIDGET, d->contextWidget, Position::Bottom);
        d->addedWidget.insert(WN_CONTEXTWIDGET, d->contextWidget);
        d->mainWindow->deleteDockHeader(WN_CONTEXTWIDGET);
        d->contextWidgetAdded = true;
    } else {
        d->mainWindow->showWidget(WN_CONTEXTWIDGET);
    }
}

bool Controller::hasContextWidget(const QString &title)
{
    return d->contextWidgets.contains(title);
}

void Controller::hideContextWidget()
{
    d->mainWindow->hideWidget(WN_CONTEXTWIDGET);
}

void Controller::switchContextWidget(const QString &title)
{
    qInfo() << __FUNCTION__;
    d->stackContextWidget->setCurrentWidget(d->contextWidgets[title]);
    if (d->stackContextWidget->isHidden()) {
        d->contextWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        d->stackContextWidget->show();
    }
    if (d->tabButtons.contains(title))
        d->tabButtons[title]->show();

    for (auto it = d->tabButtons.begin(); it != d->tabButtons.end(); ++it) {
        it.value()->setChecked(false);
        if (it.key() == title)
            it.value()->setChecked(true);
    }
}

void Controller::addChildMenu(AbstractMenu *abstractMenu)
{
    DMenu *inputMenu = abstractMenu->qMenu();
    if (!d->mainWindow || !inputMenu)
        return;

    foreach (AbstractAction *action, abstractMenu->actionList()) {
        if (action && action->hasShortCut()) {
            registerActionShortCut(action);
            addMenuShortCut(action->qAction());
        }
    }

    //make the `helper` menu at the last
    for (QAction *action : d->menu->actions()) {
        if (action->text() == MWM_TOOLS) {
            d->menu->insertMenu(action, inputMenu);
            return;
        }
    }

    //add menu to last
    d->menu->addMenu(inputMenu);
}

void Controller::insertAction(const QString &menuName, const QString &beforActionName, AbstractAction *action)
{
    if (!action)
        return;

    QAction *inputAction = action->qAction();
    if (!inputAction)
        return;

    if (action->hasShortCut())
        registerActionShortCut(action);

    for (QAction *qAction : d->mainWindow->menuBar()->actions()) {
        if (qAction->text() == menuName) {
            for (auto childAction : qAction->menu()->actions()) {
                if (childAction->text() == beforActionName) {
                    qAction->menu()->insertAction(childAction, inputAction);
                    break;
                } else if (qAction->text() == MWM_FILE
                           && childAction->text() == MWMFA_QUIT) {
                    qAction->menu()->insertAction(qAction, inputAction);
                    break;
                }
            }
        }
    }
}

void Controller::addAction(const QString &menuName, AbstractAction *action)
{
    QAction *inputAction = action->qAction();
    if (!inputAction)
        return;

    if (action->hasShortCut())
        registerActionShortCut(action);

    //防止与edit和debug界面的topToolBar快捷键冲突
    if (menuName != MWM_DEBUG && menuName != MWM_BUILD)
        addMenuShortCut(inputAction);

    if (menuName == MWMFA_NEW_FILE_OR_PROJECT) {
        for (QAction *qAction : d->menu->actions()) {
            if (qAction->text() == MWM_BUILD) {
                d->menu->insertAction(qAction, inputAction);
                d->menu->insertSeparator(qAction);
                return;
            }
        }
    }

    for (QAction *qAction : d->menu->actions()) {
        if (qAction->text() == menuName) {
            if (qAction->text() == MWM_FILE) {
                auto endAction = *(qAction->menu()->actions().rbegin());
                if (endAction->text() == MWMFA_QUIT) {
                    return qAction->menu()->insertAction(endAction, inputAction);
                }
            }
            qAction->menu()->addAction(inputAction);
        }
    }

    if (!inputAction->parent())
        inputAction->setParent(this);
}

void Controller::removeActions(const QString &menuName)
{
    for (QAction *qAction : d->mainWindow->menuBar()->actions()) {
        if (qAction->text() == menuName) {
            foreach (QAction *action, qAction->menu()->actions()) {
                qAction->menu()->removeAction(action);
            }
            break;
        }
    }
}

void Controller::addOpenProjectAction(const QString &name, AbstractAction *action)
{
    if (!action || !action->qAction())
        return;

    if (action->hasShortCut())
        registerActionShortCut(action);

    QAction *inputAction = action->qAction();

    foreach (QAction *action, d->menu->actions()) {
        if (action->text() == MWMFA_OPEN_PROJECT) {
            for (auto langAction : action->menu()->menuAction()->menu()->actions()) {
                if (name == langAction->text()) {
                    langAction->menu()->addAction(inputAction);
                    return;
                }
            }
            auto langMenu = new DMenu(name);
            action->menu()->addMenu(langMenu);
            langMenu->addAction(inputAction);
            return;
        }
    }
}

void Controller::addWidgetToTopTool(AbstractWidget *abstractWidget, bool addSeparator, bool addToLeft, quint8 priority)
{
    if (!abstractWidget)
        return;
    auto widget = static_cast<DWidget *>(abstractWidget->qWidget());
    if (!widget)
        return;

    QHBoxLayout *hlayout { nullptr };

    if (!addToLeft) {
        hlayout = qobject_cast<QHBoxLayout *>(d->rightTopToolBar->layout());
    } else {
        hlayout = qobject_cast<QHBoxLayout *>(d->leftTopToolBar->layout());
    }
    
    //sort
    auto index = 0;
    widget->setProperty("toptool_priority", priority);
    for (; index < hlayout->count(); index++) {
        if (hlayout->itemAt(index)->isEmpty())
            continue;
        auto w = hlayout->itemAt(index)->widget();
        if (priority <= w->property("toptool_priority").toInt())
            break;
    }

    if (addSeparator) {
        DWidget* separator = new DWidget(d->mainWindow);
        DVerticalLine *line = new DVerticalLine(d->mainWindow);
        auto separatorLayout = new QHBoxLayout(separator);
        separator->setProperty("toptool_priority", priority - 1);
        line->setFixedHeight(20);
        line->setFixedWidth(1);

        separatorLayout->setContentsMargins(5, 0, 5, 0);
        separatorLayout->addWidget(line);

        hlayout->insertWidget(index++, separator);
    }

    hlayout->insertWidget(index, widget);
}

void Controller::addTopToolItem(AbstractAction *action, bool addSeparator, quint8 priority)
{
    if (!action || !action->qAction())
        return;

    if (action->hasShortCut())
        registerActionShortCut(action);

    auto iconBtn = createIconButton(action->qAction());
    addWidgetToTopTool(new AbstractWidget(iconBtn), addSeparator, true, priority);
}

void Controller::addTopToolItemToRight(AbstractAction *action, bool addSeparator, quint8 priority)
{
    if (!action || !action->qAction())
        return;

    if (action->hasShortCut())
        registerActionShortCut(action);

    auto iconBtn = createIconButton(action->qAction());
    addWidgetToTopTool(new AbstractWidget(iconBtn), addSeparator, false, priority);
}

void Controller::showTopToolBar()
{
    d->mainWindow->showTopToolBar();
}

void Controller::openFileDialog()
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    QString filePath = DFileDialog::getOpenFileName(nullptr, tr("Open Document"), dir);
    if (filePath.isEmpty() && !QFileInfo(filePath).exists())
        return;
    recent.saveOpenedFile(filePath);
    editor.openFile(QString(), filePath);
}

void Controller::loading()
{
    d->loadingwidget = new loadingWidget(d->mainWindow);
    d->mainWindow->addWidget(WN_LOADINGWIDGET, d->loadingwidget);

    QObject::connect(&dpf::Listener::instance(), &dpf::Listener::pluginsStarted,
                     this, [=]() {
                         d->mainWindow->removeWidget(WN_LOADINGWIDGET);

                         d->navigationToolBar->show();
                         d->mainWindow->setToolbar(Qt::ToolBarArea::LeftToolBarArea, d->navigationToolBar);
                     });
}

void Controller::initMainWindow()
{
    qInfo() << __FUNCTION__;
    if (!d->mainWindow) {
        d->mainWindow = new MainWindow;
        d->mainWindow->setMinimumSize(MW_MIN_WIDTH, MW_MIN_HEIGHT);
        d->mainWindow->resize(MW_WIDTH, MW_HEIGHT);

        initMenu();

        if (CommandParser::instance().getModel() != CommandParser::CommandLine) {
            d->mainWindow->showMaximized();
            loading();
        }

        auto desktop = QApplication::desktop();
        int currentScreenIndex = desktop->screenNumber(d->mainWindow);
        QList<QScreen *> screenList = QGuiApplication::screens();

        if (currentScreenIndex < screenList.count()) {
            QRect screenRect = screenList[currentScreenIndex]->geometry();
            int screenWidth = screenRect.width();
            int screenHeight = screenRect.height();
            d->mainWindow->move((screenWidth - d->mainWindow->width()) / 2, (screenHeight - d->mainWindow->height()) / 2);
        }
    }
}

void Controller::initNavigationBar()
{
    qInfo() << __FUNCTION__;
    if (d->navigationBar)
        return;
    d->navigationToolBar = new DWidget(d->mainWindow);
    auto vLayout = new QVBoxLayout(d->navigationToolBar);
    d->navigationBar = new NavigationBar(d->mainWindow);
    d->navigationToolBar->hide();
    vLayout->addWidget(d->navigationBar);
    vLayout->setContentsMargins(0, 0, 2, 0);
}

void Controller::initMenu()
{
    qInfo() << __FUNCTION__;
    if (!d->mainWindow)
        return;
    if (!d->menu)
        d->menu = new DMenu(d->mainWindow->titlebar());

    createFileActions();
    createBuildActions();
    createDebugActions();

    d->menu->addSeparator();

    createHelpActions();
    createToolsActions();

    d->mainWindow->titlebar()->setMenu(d->menu);
}

void Controller::createHelpActions()
{
    auto helpMenu = new DMenu(MWM_HELP, d->menu);
    d->menu->addMenu(helpMenu);

    QAction *actionReportBug = new QAction(MWM_REPORT_BUG, helpMenu);
    ActionManager::getInstance()->registerAction(actionReportBug, "Help.Report.Bug",
                                                 MWM_REPORT_BUG, QKeySequence());
    addMenuShortCut(actionReportBug);
    helpMenu->addAction(actionReportBug);

    QAction *actionHelpDoc = new QAction(MWM_HELP_DOCUMENTS, helpMenu);
    ActionManager::getInstance()->registerAction(actionHelpDoc, "Help.Help.Documents",
                                                 MWM_HELP_DOCUMENTS, QKeySequence());
    helpMenu->addAction(actionHelpDoc);
    addMenuShortCut(actionHelpDoc);

    helpMenu->addSeparator();

    QAction::connect(actionReportBug, &QAction::triggered, this, [=]() {
        QDesktopServices::openUrl(QUrl("https://github.com/linuxdeepin/deepin-unioncode/issues"));
    });
    QAction::connect(actionHelpDoc, &QAction::triggered, this, [=]() {
        QDesktopServices::openUrl(QUrl("https://ecology.chinauos.com/adaptidentification/doc_new/#document2?dirid=656d40a9bd766615b0b02e5e"));
    });
}

void Controller::createToolsActions()
{
    auto toolsMenu = new DMenu(MWM_TOOLS);
    d->menu->addMenu(toolsMenu);
}

void Controller::createDebugActions()
{
    auto *debugMenu = new DMenu(MWM_DEBUG);
    d->menu->addMenu(debugMenu);
}

void Controller::createBuildActions()
{
    auto *buildMenu = new DMenu(MWM_BUILD);
    d->menu->addMenu(buildMenu);
}

void Controller::createFileActions()
{
    QAction *actionOpenFile = new QAction(MWMFA_OPEN_FILE);
    ActionManager::getInstance()->registerAction(actionOpenFile, "File.Open.File",
                                                 MWMFA_OPEN_FILE, QKeySequence(Qt::Modifier::CTRL | Qt::Key::Key_O));

    QAction::connect(actionOpenFile, &QAction::triggered, this, &Controller::openFileDialog);
    d->menu->addAction(actionOpenFile);
    addMenuShortCut(actionOpenFile);

    DMenu *menuOpenProject = new DMenu(MWMFA_OPEN_PROJECT);
    d->menu->addMenu(menuOpenProject);
}

void Controller::initContextWidget()
{
    if (!d->stackContextWidget)
        d->stackContextWidget = new DStackedWidget(d->mainWindow);
    if (!d->contextTabBar)
        d->contextTabBar = new DFrame(d->mainWindow);
    if (!d->contextWidget)
        d->contextWidget = new DWidget(d->mainWindow);

    DStyle::setFrameRadius(d->contextTabBar, 0);
    d->contextTabBar->setLineWidth(0);
    d->contextTabBar->setFixedHeight(40);

    d->contextButtonLayout = new QHBoxLayout;
    d->contextButtonLayout->setSpacing(0);
    d->contextButtonLayout->setContentsMargins(12, 6, 12, 6);
    d->contextButtonLayout->setAlignment(Qt::AlignLeft);

    DToolButton *hideBtn = new DToolButton(d->contextTabBar);
    hideBtn->setFixedSize(35, 35);
    hideBtn->setIcon(QIcon::fromTheme("hide_dock"));
    hideBtn->setToolTip(tr("Hide ContextWidget"));
    connect(hideBtn, &DToolButton::clicked, d->contextWidget, [=](){
        if (d->stackContextWidget->isVisible()) {
            d->stackContextWidget->hide();
            d->contextWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
            d->mainWindow->resizeDock(WN_CONTEXTWIDGET, d->contextTabBar->size());
        } else {
            d->contextWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
            d->stackContextWidget->show();
        }
    });

    QHBoxLayout *tabbarLayout = new QHBoxLayout(d->contextTabBar);
    tabbarLayout->setContentsMargins(0, 0, 0, 0);
    tabbarLayout->addLayout(d->contextButtonLayout);
    tabbarLayout->addWidget(hideBtn, Qt::AlignRight);

    QVBoxLayout *contextVLayout = new QVBoxLayout;
    contextVLayout->setContentsMargins(0, 0, 0, 0);
    contextVLayout->setSpacing(0);
    contextVLayout->addWidget(d->contextTabBar);
    contextVLayout->addWidget(new DHorizontalLine);
    contextVLayout->addWidget(d->stackContextWidget);
    d->contextWidget->setLayout(contextVLayout);

    //add contextWidget after add centralWidget or it`s height is incorrect
    //d->mainWindow->addWidget(WN_CONTEXTWIDGET, d->contextWidget, Position::Bottom);
}

void Controller::initStatusBar()
{
    if (d->statusBar)
        return;
    d->statusBar = new WindowStatusBar(d->mainWindow);
    d->statusBar->hide();
    d->mainWindow->setStatusBar(d->statusBar);
}

void Controller::initWorkspaceWidget()
{
    if (d->workspace)
        return;
    d->workspace = new WorkspaceWidget(d->mainWindow);
}

void Controller::initTopToolBar()
{
    d->leftTopToolBar = new DWidget(d->mainWindow);
    auto hlayout = new QHBoxLayout(d->leftTopToolBar);
    hlayout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    hlayout->setSpacing(0);
    hlayout->setContentsMargins(0, 0, 0, 0);
    
    d->locatorBar = LocatorManager::instance()->getInputEdit();
    d->rightTopToolBar = new DWidget(d->mainWindow);

    QHBoxLayout *rtLayout = new QHBoxLayout(d->rightTopToolBar);
    rtLayout->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    rtLayout->setContentsMargins(0, 0, 0, 0);
    rtLayout->setSpacing(10);

    d->mainWindow->setLeftTopToolWidget(d->leftTopToolBar);
    d->mainWindow->setMiddleTopToolWidget(d->locatorBar);
    d->mainWindow->setRightTopToolWidget(d->rightTopToolBar);

    d->mainWindow->hideTopTollBar();
}

void Controller::initModules()
{
    for (auto module : d->modules.values()) {
        module->initialize(this);
    }
}

void Controller::addMenuShortCut(QAction *action, QKeySequence keySequence)
{
    QKeySequence key = keySequence;
    if (keySequence.isEmpty())
        key = action->shortcut();

    QShortcut *shortCut = new QShortcut(key, d->mainWindow);
    connect(action, &QAction::changed, this, [=]() {
        if (action->shortcut() != shortCut->key())
            shortCut->setKey(action->shortcut());
    });
    connect(shortCut, &QShortcut::activated, action, [=] {
        action->trigger();
    });
}

void Controller::showStatusBar()
{
    if (d->statusBar)
        d->statusBar->show();
}

void Controller::hideStatusBar()
{
    if (d->statusBar)
        d->statusBar->hide();
}

void Controller::addStatusBarItem(QWidget *item)
{
    if (d->statusBar && item) {
        if (!item->parent())
            item->setParent(d->statusBar);
        d->statusBar->insertPermanentWidget(0, item);
    }
}

void Controller::switchWorkspace(const QString &titleName)
{
    if (d->workspace)
        d->workspace->switchWidgetWorkspace(titleName);
}

void Controller::registerActionShortCut(AbstractAction *action)
{
    auto qAction = action->qAction();
    if (!qAction)
        return;
    ActionManager::getInstance()->registerAction(qAction, action->id(), action->description(), action->keySequence());
}

void Controller::showWorkspace()
{
    if (d->showWorkspace != true) {
        d->mainWindow->addWidget(WN_WORKSPACE, d->workspace, Position::Left);
        d->mainWindow->resizeDock(WN_WORKSPACE, QSize(300, 300));

        d->addedWidget.insert(WN_WORKSPACE, d->workspace);
        d->showWorkspace = true;

        for (auto btn : d->workspace->getAllToolBtn())
            d->mainWindow->addToolBtnToDockHeader(WN_WORKSPACE, btn);

        DToolButton *expandAll = new DToolButton(d->workspace);
        expandAll->setToolTip(tr("Expand All"));
        expandAll->setIcon(QIcon::fromTheme("expand_all"));
        d->mainWindow->addToolBtnToDockHeader(WN_WORKSPACE, expandAll);
        connect(expandAll, &DToolButton::clicked, this, [](){workspace.expandAll();});

        DToolButton *foldAll = new DToolButton(d->workspace);
        foldAll->setToolTip(tr("Fold All"));
        foldAll->setIcon(QIcon::fromTheme("collapse_all"));
        d->mainWindow->addToolBtnToDockHeader(WN_WORKSPACE, foldAll);
        connect(foldAll, &DToolButton::clicked, this, [](){workspace.foldAll();});

        expandAll->setVisible(d->workspace->getCurrentExpandState());
        foldAll->setVisible(d->workspace->getCurrentExpandState());

        d->mainWindow->setDockHeadername(WN_WORKSPACE, d->workspace->getCurrentTitle());
        connect(d->workspace, &WorkspaceWidget::expandStateChange, this, [=](bool canExpand){
            expandAll->setVisible(canExpand);
            foldAll->setVisible(canExpand);
        });
        connect(d->workspace, &WorkspaceWidget::workSpaceWidgeSwitched, this, [=](const QString &title){
            d->mainWindow->setDockHeadername(WN_WORKSPACE, title);
        });

        d->workspace->addedToController = true;
    }

    d->mainWindow->showWidget(WN_WORKSPACE);
}

DToolButton *Controller::createIconButton(QAction *action)
{
    auto iconBtn = utils::createIconButton(action, d->mainWindow);
    d->topToolBtn.insert(action, iconBtn);
    return iconBtn;
}

void Controller::removeTopToolItem(AbstractAction *action)
{
    if (!action || !action->qAction())
        return;

    auto iconBtn = d->topToolBtn.value(action->qAction());

    delete iconBtn;
    d->topToolBtn.remove(action->qAction());
}
