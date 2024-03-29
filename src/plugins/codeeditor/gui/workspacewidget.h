// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef WORKSPACEWIDGET_H
#define WORKSPACEWIDGET_H

#include <QWidget>
#include <QSplitter>

class WorkspaceWidgetPrivate;
class WorkspaceWidget : public QWidget
{
    Q_OBJECT
public:
    explicit WorkspaceWidget(QWidget *parent = nullptr);

    QString selectedText() const;
    QString cursorBeforeText() const;
    QString cursorBehindText() const;
    QStringList modifiedFiles() const;
    void saveAll() const;
    void saveAs(const QString &from, const QString &to = "");
    void replaceSelectedText(const QString &text);
    void showTips(const QString &tips);
    void setCompletion(const QString &info, const QIcon &icon, const QKeySequence &key);
    void insertText(const QString &text);
    void undo();
    void reloadFile(const QString &fileName);
    void setFileModified(const QString &fileName, bool isModified);
    void closeFileEditor(const QString &fileName);

protected:
    bool event(QEvent *event) override;

private:
    QSharedPointer<WorkspaceWidgetPrivate> d { nullptr };
};

#endif   // WORKSPACEWIDGET_H
