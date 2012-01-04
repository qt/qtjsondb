/****************************************************************************
**
** Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
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
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef JsonDbQuery_H
#define JsonDbQuery_H

#include <QDebug>
#include <QObject>
#include <QHash>
#include <QStringList>
#include <QVariant>

#include <QtJsonDbQson/private/qson_p.h>

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE_JSONDB

class JsonDbBtreeStorage;

class JsonDbQueryTokenizer {
public:
    enum TokenClass {
        Other = 0,
        Singleton,
        Operator
    };

    JsonDbQueryTokenizer(QString input);
    QString pop();
    QString peek();
    void push(QString token) {
        if (!mNextToken.isEmpty())
            qCritical() << Q_FUNC_INFO << "Cannot push multiple tokens";
        mNextToken = token;
    }
protected:
    QString getNextToken();
    TokenClass getTokenClass(QChar c) {
      int u = c.unicode();
      TokenClass tc = (u < 128) ? sTokenClass[u] : Other;
      return tc;
    }
private:
    QString mInput;
    int mPos;
    QString mNextToken;
    static TokenClass sTokenClass[128];
};


class QueryTerm {
public:
    QueryTerm();
    ~QueryTerm();
    QString propertyName() const { return mPropertyName; }
    void setPropertyName(QString propertyName) { mPropertyName = propertyName; mFieldPath = propertyName.split('.'); }
    const QStringList &fieldPath() const { return mFieldPath; }

    QString op() const { return mOp; }
    void setOp(QString op) { mOp = op; }

    QString joinField() const { return mJoinField; }
    void setJoinField(QString joinField) {
        mJoinField = joinField;
        if (!joinField.isEmpty()) {
            QStringList joinFields = joinField.split("->");
            mJoinPaths.resize(joinFields.size());
            for (int j = 0; j < joinFields.size(); j++)
                mJoinPaths[j] = joinFields[j].split('.');
        }
    }
    const QVector<QStringList> &joinPaths() const { return mJoinPaths; }

    QVariant value() const { return mValue; }
    void setValue(QVariant v) { mValue = v; }
    QRegExp &regExp() { return mRegExp; }
    const QRegExp &regExpConst() const { return mRegExp; }

 private:
    QString mPropertyName;
    QStringList mFieldPath;
    QString mOp;
    QString mJoinField;
    QVector<QStringList> mJoinPaths;
    QVariant mValue;
    QRegExp mRegExp;
    //QString valueString;
};

class OrQueryTerm {
public:
    OrQueryTerm();
    OrQueryTerm(const QueryTerm &term);
    ~OrQueryTerm();
    const QList<QueryTerm> &terms() const { return mTerms; }
    void addTerm(const QueryTerm &term) { mTerms.append(term); }
    QList<QString> propertyNames() const;
private:
    QList<QueryTerm> mTerms;
};

class OrderTerm {
public:
    OrderTerm();
    ~OrderTerm();
    bool ascending;
    QString propertyName;
};

class JsonDbQuery {
public:
    JsonDbQuery() {}
    JsonDbQuery(const QList<OrQueryTerm> &qt, const QList<OrderTerm> &ot)
    : queryTerms(qt), orderTerms(ot) {}
    ~JsonDbQuery();
    QList<OrQueryTerm> queryTerms;
    QList<OrderTerm> orderTerms;
    QString query;
    QVariant::Type resultType;
    QStringList mapExpressionList;
    QStringList mapKeyList;
    QStringList queryExplanation;
    QString mAggregateOperation;

    QSet<QString> matchedTypes() const { return mMatchedTypes; }
    bool match(const QsonMap &object, QHash<QString, QsonMap> *objectCache, JsonDbBtreeStorage *storage = 0) const;

    static QVariant parseJsonLiteral(const QString &json, QueryTerm *term, QsonMap &bindings, bool *ok);
    static JsonDbQuery parse(const QString &query, QsonMap &bindings);

private:
    QSet<QString> mMatchedTypes;
};

QT_END_NAMESPACE_JSONDB

QT_END_HEADER

#endif
