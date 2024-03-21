// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef WORKSPACEWIDGET_P_H
#define WORKSPACEWIDGET_P_H

#include "gui/workspacewidget.h"
#include "gui/tabwidget.h"

#include "common/util/eventdefinitions.h"

class WorkspaceWidgetPrivate : public QObject
{
    Q_OBJECT
public:
    explicit WorkspaceWidgetPrivate(WorkspaceWidget *qq);

    void initUI();
    void initConnection();
    void connectTabWidgetSignals(TabWidget *tabWidget);

    TabWidget *currentTabWidget() const;
    void doSplit(QSplitter *spliter, int index, const QString &fileName, int pos, int scroll);

    int showFileChangedConfirmDialog(const QString &fileName);
    int showFileRemovedConfirmDialog(const QString &fileName);

    void handleFileChanged();
    void handleFileRemoved();
    bool checkAndResetSaveState(const QString &fileName);

public slots:
    void checkFileState();
    void onSplitRequested(Qt::Orientation ori, const QString &fileName);
    void onCloseRequested();
    void onFocusChanged(QWidget *old, QWidget *now);
    void onZoomValueChanged();
    void onFileDeleted(const QString &fileName);
    void onFileModified(const QString &fileName);

    void handleOpenFile(const QString &workspace, const QString &fileName);
    void handleAddBreakpoint(const QString &fileName, int line);
    void handleRemoveBreakpoint(const QString &fileName, int line);
    void handleBack();
    void handleForward();
    void handleSetDebugLine(const QString &fileName, int line);
    void handleRemoveDebugLine();
    void handleGotoLine(const QString &fileName, int line);
    void handleGotoPosition(const QString &fileName, int line, int column);
    void handleCloseCurrentEditor();
    void handleSwitchHeaderSource();

public:
    WorkspaceWidget *q;

    TabWidget *focusTabWidget { nullptr };
    QList<TabWidget *> tabWidgetList;

    int zoomValue { 0 };
    QStringList modifiedFileList;
    QStringList removedFileList;
    QTimer fileCheckTimer;
};

#endif   // WORKSPACEWIDGET_P_H
