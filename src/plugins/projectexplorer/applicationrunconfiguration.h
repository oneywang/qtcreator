/**************************************************************************
**
** This file is part of Qt Creator
**
** Copyright (c) 2010 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** Commercial Usage
**
** Licensees holding valid Qt Commercial licenses may use this file in
** accordance with the Qt Commercial License Agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Nokia.
**
** GNU Lesser General Public License Usage
**
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** If you are unsure which license is appropriate for your use, please
** contact the sales department at http://qt.nokia.com/contact.
**
**************************************************************************/

#ifndef APPLICATIONRUNCONFIGURATION_H
#define APPLICATIONRUNCONFIGURATION_H

#include <projectexplorer/toolchain.h>

#include "runconfiguration.h"
#include "applicationlauncher.h"

namespace Utils {
class Environment;
}

namespace ProjectExplorer {

class PROJECTEXPLORER_EXPORT LocalApplicationRunConfiguration : public RunConfiguration
{
    Q_OBJECT
public:
    enum RunMode {
        Console = ApplicationLauncher::Console,
        Gui
    };

    virtual ~LocalApplicationRunConfiguration();
    virtual QString executable() const = 0;
    virtual RunMode runMode() const = 0;
    virtual QString workingDirectory() const = 0;
    virtual QStringList commandLineArguments() const = 0;
    virtual Utils::Environment environment() const = 0;
    virtual QString dumperLibrary() const = 0;
    virtual QStringList dumperLibraryLocations() const = 0;
    virtual ProjectExplorer::ToolChain::ToolChainType toolChainType() const = 0;

protected:
    explicit LocalApplicationRunConfiguration(Target *target, const QString &id);
    explicit LocalApplicationRunConfiguration(Target *target, LocalApplicationRunConfiguration *rc);
};

} // namespace ProjectExplorer

#endif // APPLICATIONRUNCONFIGURATION_H
