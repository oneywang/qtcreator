/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company.  For licensing terms and
** conditions see http://www.qt.io/terms-conditions.  For further information
** use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file.  Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, The Qt Company gives you certain additional
** rights.  These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/

#include "debuggerruncontrol.h"

#include "debuggeractions.h"
#include "debuggercore.h"
#include "debuggerengine.h"
#include "debuggerkitinformation.h"
#include "debuggerplugin.h"
#include "debuggerrunconfigurationaspect.h"
#include "debuggerstartparameters.h"
#include "debuggerstringutils.h"
#include "breakhandler.h"
#include "shared/peutils.h"

#include <projectexplorer/buildconfiguration.h>
#include <projectexplorer/devicesupport/deviceprocessesdialog.h>
#include <projectexplorer/devicesupport/deviceprocesslist.h>
#include <projectexplorer/environmentaspect.h> // For the environment
#include <projectexplorer/localapplicationrunconfiguration.h> // For LocalApplication*
#include <projectexplorer/project.h>
#include <projectexplorer/projectexplorer.h>
#include <projectexplorer/target.h>
#include <projectexplorer/taskhub.h>
#include <projectexplorer/toolchain.h>

#include <utils/checkablemessagebox.h>
#include <utils/fileutils.h>
#include <utils/qtcassert.h>
#include <utils/qtcprocess.h>
#include <coreplugin/icore.h>

#include <QTcpServer>

using namespace Debugger::Internal;
using namespace ProjectExplorer;
using namespace Utils;

enum { debug = 0 };

namespace Debugger {
namespace Internal {

DebuggerEngine *createCdbEngine(const DebuggerStartParameters &sp, QString *error);
DebuggerEngine *createGdbEngine(const DebuggerStartParameters &sp);
DebuggerEngine *createPdbEngine(const DebuggerStartParameters &sp);
DebuggerEngine *createQmlEngine(const DebuggerStartParameters &sp);
DebuggerEngine *createQmlCppEngine(const DebuggerStartParameters &sp, QString *error);
DebuggerEngine *createLldbEngine(const DebuggerStartParameters &sp);

} // namespace Internal

static const char *engineTypeName(DebuggerEngineType et)
{
    switch (et) {
    case Debugger::NoEngineType:
        break;
    case Debugger::GdbEngineType:
        return "Gdb engine";
    case Debugger::CdbEngineType:
        return "Cdb engine";
    case Debugger::PdbEngineType:
        return "Pdb engine";
    case Debugger::QmlEngineType:
        return "QML engine";
    case Debugger::QmlCppEngineType:
        return "QML C++ engine";
    case Debugger::LldbEngineType:
        return "LLDB command line engine";
    case Debugger::AllEngineTypes:
        break;
    }
    return "No engine";
}

DebuggerRunControl::DebuggerRunControl(RunConfiguration *runConfiguration, DebuggerEngine *engine)
    : RunControl(runConfiguration, DebugRunMode),
      m_engine(engine),
      m_running(false)
{
    setIcon(QLatin1String(ProjectExplorer::Constants::ICON_DEBUG_SMALL));
    connect(this, &RunControl::finished, this, &DebuggerRunControl::handleFinished);

    connect(engine, &DebuggerEngine::requestRemoteSetup,
            this, &DebuggerRunControl::requestRemoteSetup);
    connect(engine, &DebuggerEngine::stateChanged,
            this, &DebuggerRunControl::stateChanged);
    connect(engine, &DebuggerEngine::aboutToNotifyInferiorSetupOk,
            this, &DebuggerRunControl::aboutToNotifyInferiorSetupOk);
}

DebuggerRunControl::~DebuggerRunControl()
{
    disconnect();
    if (m_engine) {
        DebuggerEngine *engine = m_engine;
        m_engine = 0;
        engine->disconnect();
        delete engine;
    }
}

QString DebuggerRunControl::displayName() const
{
    QTC_ASSERT(m_engine, return QString());
    return m_engine->startParameters().displayName;
}

void DebuggerRunControl::start()
{
    QTC_ASSERT(m_engine, return);
    // User canceled input dialog asking for executable when working on library project.
    if (m_engine->startParameters().startMode == StartInternal
        && m_engine->startParameters().executable.isEmpty()) {
        appendMessage(tr("No executable specified.") + QLatin1Char('\n'), ErrorMessageFormat);
        emit started();
        emit finished();
        return;
    }

    if (m_engine->startParameters().startMode == StartInternal) {
        QStringList unhandledIds;
        foreach (Breakpoint bp, breakHandler()->allBreakpoints()) {
            if (bp.isEnabled() && !m_engine->acceptsBreakpoint(bp))
                unhandledIds.append(bp.id().toString());
        }
        if (!unhandledIds.isEmpty()) {
            QString warningMessage =
                    DebuggerPlugin::tr("Some breakpoints cannot be handled by the debugger "
                                       "languages currently active, and will be ignored.\n"
                                       "Affected are breakpoints %1")
                    .arg(unhandledIds.join(QLatin1String(", ")));

            Internal::showMessage(warningMessage, LogWarning);

            static bool checked = true;
            if (checked)
                CheckableMessageBox::information(Core::ICore::mainWindow(),
                                                 tr("Debugger"),
                                                 warningMessage,
                                                 tr("&Show this message again."),
                                                 &checked, QDialogButtonBox::Ok);
        }
    }

    Internal::runControlStarted(m_engine);

    // We might get a synchronous startFailed() notification on Windows,
    // when launching the process fails. Emit a proper finished() sequence.
    emit started();
    m_running = true;

    m_engine->startDebugger(this);

    if (m_running)
        appendMessage(tr("Debugging starts") + QLatin1Char('\n'), NormalMessageFormat);
}

void DebuggerRunControl::startFailed()
{
    appendMessage(tr("Debugging has failed") + QLatin1Char('\n'), NormalMessageFormat);
    m_running = false;
    emit finished();
    m_engine->handleStartFailed();
}

void DebuggerRunControl::notifyEngineRemoteServerRunning(const QByteArray &msg, int pid)
{
    m_engine->notifyEngineRemoteServerRunning(msg, pid);
}

void DebuggerRunControl::notifyEngineRemoteSetupFinished(const RemoteSetupResult &result)
{
    m_engine->notifyEngineRemoteSetupFinished(result);
}

void DebuggerRunControl::handleFinished()
{
    appendMessage(tr("Debugging has finished") + QLatin1Char('\n'), NormalMessageFormat);
    if (m_engine)
        m_engine->handleFinished();
    Internal::runControlFinished(m_engine);
}

bool DebuggerRunControl::promptToStop(bool *optionalPrompt) const
{
    QTC_ASSERT(isRunning(), return true);

    if (optionalPrompt && !*optionalPrompt)
        return true;

    const QString question = tr("A debugging session is still in progress. "
            "Terminating the session in the current"
            " state can leave the target in an inconsistent state."
            " Would you still like to terminate it?");
    return showPromptToStopDialog(tr("Close Debugging Session"), question,
                                  QString(), QString(), optionalPrompt);
}

RunControl::StopResult DebuggerRunControl::stop()
{
    QTC_ASSERT(m_engine, return StoppedSynchronously);
    m_engine->quitDebugger();
    return AsynchronousStop;
}

void DebuggerRunControl::debuggingFinished()
{
    m_running = false;
    emit finished();
}

void DebuggerRunControl::showMessage(const QString &msg, int channel)
{
    m_engine->showMessage(msg, channel);
}

bool DebuggerRunControl::isRunning() const
{
    return m_running;
}

DebuggerStartParameters &DebuggerRunControl::startParameters()
{
    return m_engine->startParameters();
}

void DebuggerRunControl::notifyInferiorIll()
{
    m_engine->notifyInferiorIll();
}

void DebuggerRunControl::notifyInferiorExited()
{
    m_engine->notifyInferiorExited();
}

void DebuggerRunControl::quitDebugger()
{
    m_engine->quitDebugger();
}

void DebuggerRunControl::abortDebugger()
{
    m_engine->abortDebugger();
}

////////////////////////////////////////////////////////////////////////
//
// DebuggerRunControlFactory
//
////////////////////////////////////////////////////////////////////////

DebuggerRunControlFactory::DebuggerRunControlFactory(QObject *parent)
    : IRunControlFactory(parent)
{}

bool DebuggerRunControlFactory::canRun(RunConfiguration *runConfiguration, RunMode mode) const
{
    return (mode == DebugRunMode || mode == DebugRunModeWithBreakOnMain)
            && qobject_cast<LocalApplicationRunConfiguration *>(runConfiguration);
}

bool DebuggerRunControlFactory::fillParametersFromLocalRunConfiguration
    (DebuggerStartParameters *sp, const RunConfiguration *runConfiguration, QString *errorMessage)
{
    QTC_ASSERT(runConfiguration, return false);
    auto rc = qobject_cast<const LocalApplicationRunConfiguration *>(runConfiguration);
    QTC_ASSERT(rc, return false);
    EnvironmentAspect *environmentAspect = rc->extraAspect<EnvironmentAspect>();
    QTC_ASSERT(environmentAspect, return false);

    Target *target = runConfiguration->target();
    Kit *kit = target ? target->kit() : KitManager::defaultKit();
    if (!fillParametersFromKit(sp, kit, errorMessage))
        return false;
    sp->environment = environmentAspect->environment();

    // Normalize to work around QTBUG-17529 (QtDeclarative fails with 'File name case mismatch'...)
    sp->workingDirectory = FileUtils::normalizePathName(rc->workingDirectory());

    sp->executable = rc->executable();
    if (sp->executable.isEmpty())
        return false;

    sp->processArgs = rc->commandLineArguments();
    sp->useTerminal = rc->runMode() == ApplicationLauncher::Console;

    if (target) {
        if (const Project *project = target->project()) {
            sp->projectSourceDirectory = project->projectDirectory().toString();
            if (const BuildConfiguration *buildConfig = target->activeBuildConfiguration())
                sp->projectBuildDirectory = buildConfig->buildDirectory().toString();
            sp->projectSourceFiles = project->files(Project::ExcludeGeneratedFiles);
        }
    }

    DebuggerRunConfigurationAspect *debuggerAspect = runConfiguration->extraAspect<DebuggerRunConfigurationAspect>();
    QTC_ASSERT(debuggerAspect, return false);
    sp->multiProcess = debuggerAspect->useMultiProcess();

    if (debuggerAspect->useCppDebugger())
        sp->languages |= CppLanguage;

    if (debuggerAspect->useQmlDebugger()) {
        const IDevice::ConstPtr device =
                DeviceKitInformation::device(runConfiguration->target()->kit());
        QTC_ASSERT(device->type() == ProjectExplorer::Constants::DESKTOP_DEVICE_TYPE, return sp);
        QTcpServer server;
        const bool canListen = server.listen(QHostAddress::LocalHost)
                || server.listen(QHostAddress::LocalHostIPv6);
        if (!canListen) {
            if (errorMessage)
                *errorMessage = DebuggerPlugin::tr("Not enough free ports for QML debugging.") + QLatin1Char(' ');
            return sp;
        }
        sp->qmlServerAddress = server.serverAddress().toString();
        sp->qmlServerPort = server.serverPort();
        sp->languages |= QmlLanguage;

        // Makes sure that all bindings go through the JavaScript engine, so that
        // breakpoints are actually hit!
        const QString optimizerKey = _("QML_DISABLE_OPTIMIZER");
        if (!sp->environment.hasKey(optimizerKey))
            sp->environment.set(optimizerKey, _("1"));

        QtcProcess::addArg(&sp->processArgs, QString::fromLatin1("-qmljsdebugger=port:%1,block").arg(sp->qmlServerPort));
    }

    sp->startMode = StartInternal;
    sp->displayName = rc->displayName();

    return true;
}

RunControl *DebuggerRunControlFactory::create
    (RunConfiguration *runConfiguration, RunMode mode, QString *errorMessage)
{
    QTC_ASSERT(mode == DebugRunMode || mode == DebugRunModeWithBreakOnMain, return 0);

    // We cover only local setup here. Remote setups are handled by the
    // RunControl factories in the target specific plugins.
    DebuggerStartParameters sp;
    bool res = fillParametersFromLocalRunConfiguration(&sp, runConfiguration, errorMessage);
    if (sp.startMode == NoStartMode)
        return 0;

    QTC_ASSERT(res, return 0);

    if (mode == DebugRunModeWithBreakOnMain)
        sp.breakOnMain = true;

    sp.runConfiguration = runConfiguration;
    return doCreate(sp, errorMessage);
}

DebuggerRunControl *DebuggerRunControlFactory::doCreate
    (const DebuggerStartParameters &sp0, QString *errorMessage)
{
    TaskHub::clearTasks(Debugger::Constants::TASK_CATEGORY_DEBUGGER_DEBUGINFO);
    TaskHub::clearTasks(Debugger::Constants::TASK_CATEGORY_DEBUGGER_RUNTIME);

    DebuggerStartParameters sp = sp0;
    if (!boolSetting(AutoEnrichParameters)) {
        const QString sysroot = sp.sysRoot;
        if (sp.debugInfoLocation.isEmpty())
            sp.debugInfoLocation = sysroot + QLatin1String("/usr/lib/debug");
        if (sp.debugSourceLocation.isEmpty()) {
            QString base = sysroot + QLatin1String("/usr/src/debug/");
            sp.debugSourceLocation.append(base + QLatin1String("qt5base/src/corelib"));
            sp.debugSourceLocation.append(base + QLatin1String("qt5base/src/gui"));
            sp.debugSourceLocation.append(base + QLatin1String("qt5base/src/network"));
        }
    }

    if (sp.masterEngineType == NoEngineType) {
        if (sp.executable.endsWith(_(".py")) || sp.executable == _("/usr/bin/python")) {
            sp.masterEngineType = PdbEngineType;
        } else {
            if (RunConfiguration *rc = sp.runConfiguration) {
                DebuggerRunConfigurationAspect *aspect
                        = rc->extraAspect<Debugger::DebuggerRunConfigurationAspect>();
                if (const Target *target = rc->target())
                    if (!DebuggerRunControlFactory::fillParametersFromKit(&sp, target->kit(), errorMessage))
                        return 0;
                const bool useCppDebugger = aspect->useCppDebugger() && (sp.languages & CppLanguage);
                const bool useQmlDebugger = aspect->useQmlDebugger() && (sp.languages & QmlLanguage);
                if (useQmlDebugger) {
                    if (useCppDebugger)
                        sp.masterEngineType = QmlCppEngineType;
                    else
                        sp.masterEngineType = QmlEngineType;
                } else {
                    sp.masterEngineType = sp.cppEngineType;
                }
            } else {
                sp.masterEngineType = sp.cppEngineType;
            }
        }
    }

    QString error;
    DebuggerEngine *engine = createEngine(sp.masterEngineType, sp, &error);
    if (!engine) {
        Core::ICore::showWarningWithOptions(DebuggerRunControl::tr("Debugger"), error);
        if (errorMessage)
            *errorMessage = error;
        return 0;
    }
    return new DebuggerRunControl(sp.runConfiguration, engine);
}

IRunConfigurationAspect *DebuggerRunControlFactory::createRunConfigurationAspect(RunConfiguration *rc)
{
    return new DebuggerRunConfigurationAspect(rc);
}

DebuggerRunControl *DebuggerRunControlFactory::createAndScheduleRun(const DebuggerStartParameters &sp)
{
    QString errorMessage;
    DebuggerRunControl *rc = doCreate(sp, &errorMessage);
    if (!rc) {
        ProjectExplorerPlugin::showRunErrorMessage(errorMessage);
        return 0;
    }
    Internal::showMessage(sp.startMessage, 0);
    ProjectExplorerPlugin::startRunControl(rc, DebugRunMode);
    return rc;
}

static QString executableForPid(qint64 pid)
{
    foreach (const DeviceProcessItem &p, DeviceProcessList::localProcesses())
        if (p.pid == pid)
            return p.exe;
    return QString();
}

bool DebuggerRunControlFactory::fillParametersFromKit(DebuggerStartParameters *sp, const Kit *kit, QString *errorMessage /* = 0 */)
{
    if (!kit) {
        // This code can only be reached when starting via the command line
        // (-debug pid or executable) or attaching from runconfiguration
        // without specifying a kit. Try to find a kit via ABI.
        QList<Abi> abis;
        if (sp->toolChainAbi.isValid()) {
            abis.push_back(sp->toolChainAbi);
        } else {
            // Try via executable.
            if (sp->executable.isEmpty()
                && (sp->startMode == AttachExternal || sp->startMode == AttachCrashedExternal)) {
                sp->executable = executableForPid(sp->attachPID);
            }
            if (!sp->executable.isEmpty())
                abis = Abi::abisOfBinary(FileName::fromString(sp->executable));
        }
        if (!abis.isEmpty()) {
            // Try exact abis.
            kit = KitManager::find(std::function<bool (const Kit *)>([abis](const Kit *k) -> bool {
                if (const ToolChain *tc = ToolChainKitInformation::toolChain(k)) {
                    return abis.contains(tc->targetAbi())
                           && DebuggerKitInformation::isValidDebugger(k);
                }
                return false;
            }));
            if (!kit) {
                // Or something compatible.
                kit = KitManager::find(std::function<bool (const Kit *)>([abis](const Kit *k) -> bool {
                    if (const ToolChain *tc = ToolChainKitInformation::toolChain(k))
                        foreach (const Abi &a, abis)
                            if (a.isCompatibleWith(tc->targetAbi()) && DebuggerKitInformation::isValidDebugger(k))
                                return true;
                    return false;
                }));
            }
        }
        if (!kit)
            kit = KitManager::defaultKit();
    }

    // Verify that debugger and profile are valid
    if (!kit) {
        sp->startMode = NoStartMode;
        if (errorMessage)
            *errorMessage = DebuggerKitInformation::tr("No kit found.");
        return false;
    }
    // validate debugger if C++ debugging is enabled
    if (sp->languages & CppLanguage) {
        const QList<Task> tasks = DebuggerKitInformation::validateDebugger(kit);
        if (!tasks.isEmpty()) {
            sp->startMode = NoStartMode;
            if (errorMessage) {
                foreach (const Task &t, tasks) {
                    if (errorMessage->isEmpty())
                        errorMessage->append(QLatin1Char('\n'));
                    errorMessage->append(t.description);
                }
            }
            return false;
        }
    }
    sp->cppEngineType = DebuggerKitInformation::engineType(kit);
    sp->sysRoot = SysRootKitInformation::sysRoot(kit).toString();
    sp->debuggerCommand = DebuggerKitInformation::debuggerCommand(kit).toString();

    ToolChain *tc = ToolChainKitInformation::toolChain(kit);
    if (tc)
        sp->toolChainAbi = tc->targetAbi();

    sp->device = DeviceKitInformation::device(kit);
    if (sp->device) {
        sp->connParams = sp->device->sshParameters();
        // Could have been set from command line.
        if (sp->remoteChannel.isEmpty())
            sp->remoteChannel = sp->connParams.host + QLatin1Char(':') + QString::number(sp->connParams.port);
    }
    return true;
}

DebuggerEngine *DebuggerRunControlFactory::createEngine(DebuggerEngineType et,
    const DebuggerStartParameters &sp, QString *errorMessage)
{
    switch (et) {
    case GdbEngineType:
        return createGdbEngine(sp);
    case CdbEngineType:
        return createCdbEngine(sp, errorMessage);
    case PdbEngineType:
        return createPdbEngine(sp);
#ifdef SUPPORTQML
    case QmlEngineType:
        return createQmlEngine(sp);
#endif
    case LldbEngineType:
        return createLldbEngine(sp);
#ifdef SUPPORTQML
    case QmlCppEngineType:
        return createQmlCppEngine(sp, errorMessage);
#endif
    default:
        break;
    }
    *errorMessage = DebuggerPlugin::tr("Unable to create a debugger engine of the type \"%1\"").
                    arg(_(engineTypeName(et)));
    return 0;
}

} // namespace Debugger
