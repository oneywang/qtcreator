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

#ifndef BUILDCONFIGURATION_H
#define BUILDCONFIGURATION_H

#include "projectexplorer_export.h"
#include "projectconfiguration.h"

#include <utils/environment.h>

#include <QtCore/QString>
#include <QtCore/QStringList>



namespace ProjectExplorer {

class BuildStepList;
class Target;
class IOutputParser;

class PROJECTEXPLORER_EXPORT BuildConfiguration : public ProjectConfiguration
{
    Q_OBJECT

public:
    // ctors are protected
    virtual ~BuildConfiguration();

    virtual QString buildDirectory() const = 0;

    // TODO: Maybe the BuildConfiguration is not the best place for the environment
    virtual Utils::Environment baseEnvironment() const;
    QString baseEnvironmentText() const;
    Utils::Environment environment() const;
    void setUserEnvironmentChanges(const QList<Utils::EnvironmentItem> &diff);
    QList<Utils::EnvironmentItem> userEnvironmentChanges() const;
    bool useSystemEnvironment() const;
    void setUseSystemEnvironment(bool b);

    QStringList knownStepLists() const;
    BuildStepList *stepList(const QString &id) const;

    virtual QVariantMap toMap() const;

    // Creates a suitable outputparser for custom build steps
    // (based on the toolchain)
    // TODO this is not great API
    // it's mainly so that custom build systems are better integrated
    // with the generic project manager
    virtual IOutputParser *createOutputParser() const = 0;

    Target *target() const;

signals:
    void environmentChanged();
    void buildDirectoryChanged();

protected:
    BuildConfiguration(Target *target, const QString &id);
    BuildConfiguration(Target *target, BuildConfiguration *source);

    void cloneSteps(BuildConfiguration *source);

    virtual bool fromMap(const QVariantMap &map);

private:
    bool m_clearSystemEnvironment;
    QList<Utils::EnvironmentItem> m_userEnvironmentChanges;
    QList<BuildStepList *> m_stepLists;
};

class PROJECTEXPLORER_EXPORT IBuildConfigurationFactory :
    public QObject
{
    Q_OBJECT

public:
    explicit IBuildConfigurationFactory(QObject *parent = 0);
    virtual ~IBuildConfigurationFactory();

    // used to show the list of possible additons to a target, returns a list of types
    virtual QStringList availableCreationIds(Target *parent) const = 0;
    // used to translate the types to names to display to the user
    virtual QString displayNameForId(const QString &id) const = 0;

    virtual bool canCreate(Target *parent, const QString &id) const = 0;
    virtual BuildConfiguration *create(Target *parent, const QString &id) = 0;
    // used to recreate the runConfigurations when restoring settings
    virtual bool canRestore(Target *parent, const QVariantMap &map) const = 0;
    virtual BuildConfiguration *restore(Target *parent, const QVariantMap &map) = 0;
    virtual bool canClone(Target *parent, BuildConfiguration *product) const = 0;
    virtual BuildConfiguration *clone(Target *parent, BuildConfiguration *product) = 0;

signals:
    void availableCreationIdsChanged();
};

} // namespace ProjectExplorer

Q_DECLARE_METATYPE(ProjectExplorer::BuildConfiguration *);

#endif // BUILDCONFIGURATION_H
