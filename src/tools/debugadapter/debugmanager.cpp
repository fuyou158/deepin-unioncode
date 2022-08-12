﻿/*
 * Copyright (C) 2022 Uniontech Software Technology Co., Ltd.
 *
 * Author:     zhouyi<zhouyi1@uniontech.com>
 *
 * Maintainer:
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "debugmanager.h"
#include "locker.h"

#include "debugger/debugger.h"
#include "debugger/gdbmi/gdbdebugger.h"

#include <QProcess>

class DebugManagerPrivate {
    friend class DebugManager;

    QSharedPointer<Debugger> debugger;
    QSharedPointer<QProcess> process;
    QString tempBuffer;
    ConditionLockEx locker;
    QHash<int, DebugManager::ResponseEntry> resposeExpected;
    int tokenCounter = 0;
};

DebugManager::DebugManager(QObject *parent)
    : QObject(parent)
    , d(new DebugManagerPrivate())
{
    d->process.reset(new QProcess(this));

    connect(d->process.data(), &QProcess::readyReadStandardOutput, [this]() {
        for (const auto& c: QString{d->process->readAllStandardOutput()})
            switch (c.toLatin1()) {
            case '\r':
            case '\n':
            {
                d->tempBuffer.append(c);
                d->debugger->handleOutputRecord(d->tempBuffer);
                d->tempBuffer.clear();
                break;
            }
            default:
            {
                d->tempBuffer.append(c);
                break;
            }
            }
    });

    connect(d->process.data(), &QProcess::started, [this]() {
        d->tokenCounter = 0;
        d->tempBuffer.clear();
        d->resposeExpected.clear();
    });

    connect(d->process.data(), QOverload<int>::of(&QProcess::finished),
            this, &DebugManager::gdbProcessTerminated);
}

DebugManager::~DebugManager()
{
    if (d) {
        delete d;
    }
}

DebugManager *DebugManager::instance()
{
    static DebugManager ins;
    return &ins;
}

void DebugManager::initDebugger(const QString &program, const QStringList &arguments)
{
    d->process->setArguments(arguments);
    if (program.contains("gdb")) {
        d->debugger.reset(new GDBDebugger());
    }
}

qint64 DebugManager::getProcessId()
{
    return d->process->processId();
}

void DebugManager::waitLocker()
{
    d->locker.wait();
}

void DebugManager::fireLocker()
{
    d->locker.fire();
}

bool DebugManager::isExecuting() const
{
    return d->process->state() != QProcess::NotRunning;
}

void DebugManager::execute()
{
    if (isExecuting())
        return;

    auto a = d->process->arguments();
    foreach (QString arg, d->debugger->preArguments()) {
        a.prepend(arg);
    }
    d->process->setArguments(a);
    d->process->setProgram(d->debugger->program());
    d->process->start();
}

void DebugManager::command(const QString &cmd)
{
    auto tokStr = QString{"%1"}.arg(d->tokenCounter, 6, 10, QChar{'0'});
    auto line = QString{"%1%2%3"}.arg(tokStr, cmd, "\n");
    d->tokenCounter = (d->tokenCounter + 1) % 999999;
    d->process->write(line.toLocal8Bit());
    d->process->waitForBytesWritten();
    QString sOut;
    QTextStream(&sOut) << "Command:" << line << "\n";
    d->debugger->handleOutputStreamText(sOut);
}

void DebugManager::commandAndResponse(const QString& cmd,
                                      const ResponseEntry::ResponseHandler_t& handler,
                                      ResponseEntry::ResponseAction_t action)
{
    d->resposeExpected.insert(d->tokenCounter, { action, handler });
    command(cmd);
}

void DebugManager::launchLocal()
{
    command(d->debugger->launchLocal());
}

void DebugManager::quit()
{
    command(d->debugger->quit());
}

void DebugManager::kill()
{
    command(d->debugger->kill());
}

void DebugManager::breakRemoveAll()
{
    commandAndResponse(d->debugger->breakRemoveAll(), [this](const QVariant&) {
        d->debugger->clearBreakPoint();
    });
}

void DebugManager::breakInsert(const QString &path)
{
    commandAndResponse(d->debugger->breakInsert(path), [this](const QVariant& r) {
        d->debugger->parseBreakPoint(r);
    });
}

void DebugManager::updateExceptResponse(const int token, const QVariant& payload)
{
    if (d->resposeExpected.contains(token)) {
        auto& expect = d->resposeExpected.value(token);
        expect.handler(payload);
        if (expect.action == ResponseEntry::ResponseEntry::ResponseAction_t::Temporal)
            d->resposeExpected.remove(token);
    }
}

void DebugManager::removeBreakpointInFile(const QString &filePath)
{
    auto breakPointIds = d->debugger->breakpointsForFile(filePath);
    foreach(auto bpid, breakPointIds) {
        breakRemove(bpid);
    }
}

void DebugManager::breakRemove(int bpid)
{
    commandAndResponse(d->debugger->breakRemove(bpid), [this, bpid](const QVariant&) {
        d->debugger->removeBreakPoint(bpid);
    });
}

void DebugManager::stackListFrames()
{
    command(d->debugger->stackListFrames());
    waitLocker();
}

void DebugManager::stackListVariables()
{
    command(d->debugger->stackListVariables());
    waitLocker();
}

void DebugManager::threadInfo()
{
    command(d->debugger->threadInfo());
    waitLocker();
}

void DebugManager::commandPause()
{
    command(d->debugger->commandPause());
}

void DebugManager::commandContinue()
{
    command(d->debugger->commandContinue());
}

void DebugManager::commandNext()
{
    command(d->debugger->commandNext()); //step over
}

void DebugManager::commandStep()
{
    command(d->debugger->commandStep()); //step in
}

void DebugManager::commandFinish()
{
    command(d->debugger->commandFinish()); //step out
}

void DebugManager::threadSelect(const int threadId)
{
    command(d->debugger->threadSelect(threadId));
}

void DebugManager::listSourceFiles()
{
    command(d->debugger->listSourceFiles());
}

dap::array<dap::StackFrame> DebugManager::allStackframes()
{
    return d->debugger->allStackframes();
}

dap::array<dap::Thread> DebugManager::allThreadList()
{
    return d->debugger->allThreadList();
}

dap::array<dap::Variable> DebugManager::allVariableList()
{
    return d->debugger->allVariableList();
}

