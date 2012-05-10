/****************************************************************************
**
** Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/
**
** This file is part of the QtAddOn.JsonDb module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** GNU Lesser General Public License Usage
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this
** file. Please review the following information to ensure the GNU Lesser
** General Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file. Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**
**
**
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef JSONDB_QUERY_H
#define JSONDB_QUERY_H

#include <QDebug>
#include <QObject>
#include <QHash>
#include <QStringList>
#include <QVariant>
#include "jsondberrors.h"

#include <qjsonarray.h>
#include <qjsonobject.h>
#include <qjsonvalue.h>

#include "jsondbobject.h"

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE_JSONDB_PARTITION

class JsonDbPartition;

class Q_JSONDB_PARTITION_EXPORT JsonDbQueryTokenizer {
public:
    JsonDbQueryTokenizer(QString input);
    QString pop();
    QString popIdentifier();
    QString peek();
    void push(QString token) {
        if (!mNextToken.isEmpty())
            qCritical() << Q_FUNC_INFO << "Cannot push multiple tokens";
        mNextToken = token;
    }
protected:
    QString getNextToken();
    static const char* sTokens[];
private:
    QString mInput;
    int mPos;
    QString mNextToken;
};

class JsonDbQuery;
class Q_JSONDB_PARTITION_EXPORT JsonDbQueryTerm {
public:
    JsonDbQueryTerm(const JsonDbQuery *query);
    ~JsonDbQueryTerm();
    QString propertyName() const { return mPropertyName; }
    void setPropertyName(QString propertyName) { mPropertyName = propertyName; mFieldPath = propertyName.split('.'); }
    const QStringList &fieldPath() const { return mFieldPath; }

    QString op() const { return mOp; }
    void setOp(QString op) { mOp = op; }

    QString joinField() const { return mJoinField; }
    void setJoinField(QString joinField) {
        mJoinField = joinField;
        if (!joinField.isEmpty()) {
            QStringList joinFields = joinField.split(QStringLiteral("->"));
            mJoinPaths.resize(joinFields.size());
            for (int j = 0; j < joinFields.size(); j++)
                mJoinPaths[j] = joinFields[j].split('.');
        }
    }
    const QVector<QStringList> &joinPaths() const { return mJoinPaths; }

    QString variable() const { return mVariable; }
    void setVariable(const QString variable) { mVariable = variable; }

    QString propertyVariable() const { return mPropertyVariable; }
    void setPropertyVariable(const QString variable) { mPropertyVariable = variable; }

    QJsonValue value() const;
    void setValue(const QJsonValue &v) { mValue = v; }
    QRegExp &regExp() { return mRegExp; }
    void setRegExp(const QRegExp &regExp) { mRegExp = regExp; }
    const QRegExp &regExpConst() const { return mRegExp; }

 private:
    const JsonDbQuery *mQuery;
    QString mVariable;
    QString mPropertyName;
    QString mPropertyVariable;
    QStringList mFieldPath;
    QString mOp;
    QString mJoinField;
    QVector<QStringList> mJoinPaths;
    QJsonValue mValue;
    QRegExp mRegExp;
};

class JsonDbOrQueryTerm {
public:
    JsonDbOrQueryTerm();
    JsonDbOrQueryTerm(const JsonDbQueryTerm &term);
    ~JsonDbOrQueryTerm();
    const QList<JsonDbQueryTerm> &terms() const { return mTerms; }
    void addTerm(const JsonDbQueryTerm &term) { mTerms.append(term); }
    QList<QString> propertyNames() const;
    QList<QString> findUnindexablePropertyNames() const;
private:
    QList<JsonDbQueryTerm> mTerms;
};

class Q_JSONDB_PARTITION_EXPORT JsonDbOrderTerm {
public:
    JsonDbOrderTerm() : ascending(false) { }
    ~JsonDbOrderTerm() { }
    bool ascending;
    QString propertyName;
};

class Q_JSONDB_PARTITION_EXPORT JsonDbQuery {
public:
    JsonDbQuery() { }
    JsonDbQuery(const QList<JsonDbOrQueryTerm> &qt, const QList<JsonDbOrderTerm> &ot);
    ~JsonDbQuery();
    QList<JsonDbOrQueryTerm> queryTerms;
    QList<JsonDbOrderTerm> orderTerms;
    QString query;
    QStringList mapExpressionList;
    QStringList mapKeyList;
    QStringList queryExplanation;
    QString mAggregateOperation;

    QSet<QString> matchedTypes() const { return mMatchedTypes; }
    QJsonValue binding(const QString variable) const { return mBindings.value(variable); }
    void bind(QString variable, QJsonValue &binding) { mBindings[variable] = binding; }
    bool match(const JsonDbObject &object, QHash<QString, JsonDbObject> *objectCache, JsonDbPartition *partition = 0) const;

    static QJsonValue parseJsonLiteral(const QString &json, JsonDbQueryTerm *term, const QJsonObject &bindings, bool *ok);
    static QJsonArray parseJsonArray(JsonDbQueryTokenizer &tokenizer, const QJsonObject &bindings, bool *ok);
    static QJsonObject parseJsonObject(JsonDbQueryTokenizer &tokenizer, const QJsonObject &bindings, bool *ok);
    static JsonDbQuery *parse(const QString &query, const QJsonObject &bindings = QJsonObject());

private:
    QSet<QString> mMatchedTypes;
    QMap<QString,QJsonValue> mBindings;
    Q_DISABLE_COPY(JsonDbQuery);
};

QT_END_NAMESPACE_JSONDB_PARTITION

QT_END_HEADER

#endif // JSONDB_QUERY_H
