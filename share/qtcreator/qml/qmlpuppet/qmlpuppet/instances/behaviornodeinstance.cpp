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

#include "behaviornodeinstance.h"

#include <private/qdeclarativebehavior_p.h>

namespace QmlDesigner {
namespace Internal {

BehaviorNodeInstance::BehaviorNodeInstance(QObject *object)
    : ObjectNodeInstance(object),
    m_isEnabled(true)
{
}

BehaviorNodeInstance::Pointer BehaviorNodeInstance::create(QObject *object)
{
    QDeclarativeBehavior* behavior = qobject_cast<QDeclarativeBehavior*>(object);

    Q_ASSERT(behavior);

    Pointer instance(new BehaviorNodeInstance(behavior));

    instance->populateResetHashes();

    behavior->setEnabled(false);

    return instance;
}

void BehaviorNodeInstance::setPropertyVariant(const PropertyName &name, const QVariant &value)
{
    if (name == "enabled")
        return;

    ObjectNodeInstance::setPropertyVariant(name, value);
}

void BehaviorNodeInstance::setPropertyBinding(const PropertyName &name, const QString &expression)
{
    if (name == "enabled")
        return;

    ObjectNodeInstance::setPropertyBinding(name, expression);
}

QVariant BehaviorNodeInstance::property(const PropertyName &name) const
{
    if (name == "enabled")
        return QVariant::fromValue(m_isEnabled);

    return ObjectNodeInstance::property(name);
}

void BehaviorNodeInstance::resetProperty(const PropertyName &name)
{
    if (name == "enabled")
        m_isEnabled = true;

    ObjectNodeInstance::resetProperty(name);
}


} // namespace Internal
} // namespace QmlDesigner
