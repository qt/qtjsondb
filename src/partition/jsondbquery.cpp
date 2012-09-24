/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtAddOn.JsonDb module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QDebug>
#include <QString>

#include "jsondbstrings.h"
#include "jsondbindexquery.h"
#include "jsondbpartition.h"
#include "jsondbpartition_p.h"
#include "jsondbquery.h"
#include "jsondbsettings.h"

#include "jsondbutils_p.h"

QT_BEGIN_NAMESPACE_JSONDB_PARTITION

JsonDbQueryTerm::JsonDbQueryTerm()
    : mValue(QJsonValue::Undefined)
{
}

JsonDbQueryTerm::~JsonDbQueryTerm()
{
}

QJsonValue JsonDbQueryTerm::value() const
{
    Q_ASSERT(mVariable.isEmpty());
    return mValue;
}

// JsonDbOrQueryTerm

JsonDbOrQueryTerm::JsonDbOrQueryTerm()
{
}

JsonDbOrQueryTerm::JsonDbOrQueryTerm(const JsonDbQueryTerm &term)
{
    mTerms.append(term);
}

JsonDbOrQueryTerm::~JsonDbOrQueryTerm()
{
}

QList<QString> JsonDbOrQueryTerm::propertyNames() const
{
    QList<QString> propertyNames;
    foreach (const JsonDbQueryTerm &term, mTerms) {
        QString propertyName = term.propertyName();
        if (!propertyNames.contains(propertyName))
            propertyNames.append(propertyName);
    }
    return propertyNames;
}

QList<QString> JsonDbOrQueryTerm::findUnindexablePropertyNames() const
{
    QList<QString> unindexablePropertyNames;
    QString firstPropertyName;
    if (!mTerms.isEmpty())
        firstPropertyName = mTerms[0].propertyName();
    foreach (const JsonDbQueryTerm &term, mTerms) {
        const QString propertyName = term.propertyName();
        const QString op = term.op();
        // notExists is unindexable because there would be no value to index
        // contains and notContains are unindexable because JsonDbIndex does not support array values
        if ((op == QLatin1String("notExists")
             || op == QLatin1String("contains")
             || op == QLatin1String("notContains"))
             && !unindexablePropertyNames.contains(propertyName))
            unindexablePropertyNames.append(propertyName);
        // if multiple properties are access in an disjunction ("|") then we cannot use an index on it
        if (propertyName != firstPropertyName)
            unindexablePropertyNames.append(propertyName);
    }
    return unindexablePropertyNames;
}

// JsonDbQuery

JsonDbQuery::JsonDbQuery()
{
}

JsonDbQuery::~JsonDbQuery()
{
}

bool JsonDbQuery::match(const JsonDbObject &object, QHash<QString, JsonDbObject> *objectCache, JsonDbPartition *partition) const
{
    for (int i = 0; i < queryTerms.size(); i++) {
        const JsonDbOrQueryTerm &orQueryTerm = queryTerms[i];
        bool matches = false;
        foreach (const JsonDbQueryTerm &term, orQueryTerm.terms()) {
            const QString &joinPropertyName = term.joinField();
            const QString &op = term.op();
            QJsonValue value = termValue(term);

            QJsonValue objectFieldValue;
            if (!joinPropertyName.isEmpty()) {
                JsonDbObject joinedObject = object;
                const QVector<QStringList> &joinPaths = term.joinPaths();
                for (int j = 0; j < joinPaths.size(); j++) {
                    if (!joinPaths[j].size()) {
                        if (jsondbSettings->debug())
                            qDebug() << term.joinField() << term.joinPaths();
                    }
                    QString uuidValue = joinedObject.valueByPath(joinPaths[j]).toString();
                    if (objectCache && objectCache->contains(uuidValue))
                        joinedObject = objectCache->value(uuidValue);
                    else if (partition) {
                        ObjectKey objectKey(uuidValue);
                        partition->d_func()->getObject(objectKey, joinedObject);
                        if (objectCache) objectCache->insert(uuidValue, joinedObject);
                    }
                }
                objectFieldValue = joinedObject.valueByPath(term.propertyName());
            } else {
                if (!term.hasPropertyName())
                    objectFieldValue = bindings.value(term.propertyVariable());
                else
                    objectFieldValue = object.valueByPath(term.propertyName());
            }
            if (op == QLatin1Char('=') || op == QLatin1String("==")) {
                matches = (objectFieldValue == value);
            } else if (op == QLatin1String("<>") || op == QLatin1String("!=")) {
                matches = (objectFieldValue != value);
            } else if (op == QLatin1String("=~")) {
                QRegExp rx = term.regExpConst();
                if (jsondbSettings->debug())
                    qDebug() << "=~" << objectFieldValue.toString() << rx.exactMatch(objectFieldValue.toString());
                matches = rx.exactMatch(objectFieldValue.toString());
            } else if (op == QLatin1String("!=~")) {
                QRegExp rx = term.regExpConst();
                if (jsondbSettings->debug())
                    qDebug() << "!=~" << objectFieldValue.toString() << rx.exactMatch(objectFieldValue.toString());
                matches = !rx.exactMatch(objectFieldValue.toString());
            } else if (op == QLatin1String("<=")) {
                matches = JsonDbIndexQuery::lessThan(objectFieldValue, value) || (objectFieldValue == value);
            } else if (op == QLatin1Char('<')) {
                matches = JsonDbIndexQuery::lessThan(objectFieldValue, value);
            } else if (op == QLatin1String(">=")) {
                matches = JsonDbIndexQuery::greaterThan(objectFieldValue, value) || (objectFieldValue == value);
            } else if (op == QLatin1Char('>')) {
                matches = JsonDbIndexQuery::greaterThan(objectFieldValue, value);
            } else if (op == QLatin1String("exists")) {
                matches = (objectFieldValue.type() != QJsonValue::Undefined);
            } else if (op == QLatin1String("notExists")) {
                matches = (objectFieldValue.type() == QJsonValue::Undefined);
            } else if (op == QLatin1String("in")) {
                if (0) qDebug() << __FUNCTION__ << __LINE__ << objectFieldValue << "in" << value
                                << value.toArray().contains(objectFieldValue);
                matches = value.toArray().contains(objectFieldValue);
            } else if (op == QLatin1String("notIn")) {
                if (0) qDebug() << __FUNCTION__ << __LINE__ << objectFieldValue << "notIn" << value
                                << !value.toArray().contains(objectFieldValue);
                matches = !value.toArray().contains(objectFieldValue);
            } else if (op == QLatin1String("contains")) {
                if (0) qDebug() << __FUNCTION__ << __LINE__ << objectFieldValue << "contains" << value
                                << objectFieldValue.toArray().contains(value);
                matches = objectFieldValue.toArray().contains(value);
            } else if (op == QLatin1String("notContains")) {
                if (0) qDebug() << __FUNCTION__ << __LINE__ << objectFieldValue << "notContains" << value
                                << !objectFieldValue.toArray().contains(value);
                matches = !objectFieldValue.toArray().contains(value);
            } else if (op == QLatin1String("startsWith")) {
                matches = (objectFieldValue.type() == QJsonValue::String
                           && objectFieldValue.toString().startsWith(value.toString()));
            } else {
                qCritical() << "match" << "unhandled term" << term.propertyName() << term.op() << term.value() << term.joinField();
            }
            // if any of the OR query terms match, we continue to the next AND query term
            if (matches)
                break;
        }
        // if any of the AND query terms fail, it's not a match
        if (!matches)
            return false;
    }
    return true;
}

bool JsonDbQuery::isAscending() const
{
    return orderTerms.isEmpty() || orderTerms.at(0).ascending;
}

QT_END_NAMESPACE_JSONDB_PARTITION
