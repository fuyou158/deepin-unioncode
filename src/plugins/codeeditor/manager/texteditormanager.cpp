// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "texteditormanager.h"
#include "private/texteditormanager_p.h"

TextEditorManagerPrivate::TextEditorManagerPrivate(TextEditorManager *qq)
    : q(qq)
{
}

TextEditorManager::TextEditorManager(QObject *parent)
    : QObject(parent),
      d(new TextEditorManagerPrivate(this))
{
}

TextEditor *TextEditorManager::createEditor(QWidget *parent, const QString &fileName)
{
    TextEditor *editor = new TextEditor(parent);
    connect(editor, &TextEditor::cursorPositionChanged, this, &TextEditorManager::onLinePositionChanged, Qt::QueuedConnection);
    connect(editor, &TextEditor::destroyed, this, [this, fileName] { onEditorDestroyed(fileName); });

    editor->setFile(fileName);
    d->editorHash.insert(fileName, editor);

    return editor;
}

TextEditor *TextEditorManager::findEditor(const QString &fileName)
{
    return d->editorHash.value(fileName, nullptr);
}

void TextEditorManager::gotoNextPosition()
{
    // TODO
}

void TextEditorManager::gotoPreviousPsontion()
{
    // TODO
}

void TextEditorManager::onLinePositionChanged(int line, int pos)
{
    Q_UNUSED(line)
    // TODO
}

void TextEditorManager::onEditorDestroyed(const QString &fileName)
{
    d->editorHash.remove(fileName);
}
