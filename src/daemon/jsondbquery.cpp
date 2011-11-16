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

#include <QDebug>
#include <QFile>
#include <QString>
#include <QStringList>

#include "jsondb-strings.h"
#include "jsondb.h"
#include "jsondbbtreestorage.h"
#include "jsondbquery.h"

namespace QtAddOn { namespace JsonDb {

#ifndef QT_NO_DEBUG_OUTPUT
extern bool gDebug;
#define DBG() if (gDebug) qDebug() << Q_FUNC_INFO
#else
#define DBG() if (0) qDebug() << Q_FUNC_INFO
#endif

struct TokenClassInitializer { QChar c; JsonDbQueryTokenizer::TokenClass tokenClass;};
static TokenClassInitializer sTokenClasses[] = {
    { '[', JsonDbQueryTokenizer::Singleton },
    { ']', JsonDbQueryTokenizer::Singleton },
    { '{', JsonDbQueryTokenizer::Singleton },
    { '}', JsonDbQueryTokenizer::Singleton },
    { '/', JsonDbQueryTokenizer::Singleton },
    { '\\', JsonDbQueryTokenizer::Singleton },
    { '?', JsonDbQueryTokenizer::Singleton },
    { ',', JsonDbQueryTokenizer::Singleton },
    { ':', JsonDbQueryTokenizer::Singleton },
    { '=', JsonDbQueryTokenizer::Operator },
    { '>', JsonDbQueryTokenizer::Operator },
    { '<', JsonDbQueryTokenizer::Operator },
    { '!', JsonDbQueryTokenizer::Operator },
    { '-', JsonDbQueryTokenizer::Operator },
    { '~', JsonDbQueryTokenizer::Operator }
};

JsonDbQueryTokenizer::TokenClass JsonDbQueryTokenizer::sTokenClass[128];
JsonDbQueryTokenizer::JsonDbQueryTokenizer(QString input)
    : mInput(input), mPos(0)
{
    if (sTokenClass[sTokenClasses[0].c.unicode()] != sTokenClasses[0].tokenClass) {
        memset(sTokenClass, 0, sizeof(sTokenClass));
        for (unsigned int i = 0; i < sizeof(sTokenClasses)/sizeof(TokenClassInitializer); i++)
            sTokenClass[sTokenClasses[i].c.unicode()] = sTokenClasses[i].tokenClass;
    }
}

QString JsonDbQueryTokenizer::pop()
{
    QString token;
    if (!mNextToken.isEmpty()) {
        token = mNextToken;
        mNextToken.clear();
    } else {
        token = getNextToken();
    }
    return token;
}

QString JsonDbQueryTokenizer::peek()
{
    if (mNextToken.isEmpty())
        mNextToken = getNextToken();
    return mNextToken;
}

QString JsonDbQueryTokenizer::getNextToken()
{
    QString result;
    bool indexExpression = false;
    while (mPos < mInput.size()) {
        QChar c = mInput[mPos++];
        JsonDbQueryTokenizer::TokenClass tokenClass = getTokenClass(c);
        if (tokenClass == JsonDbQueryTokenizer::Singleton) {
            result.append(c);
            return result;
        } else if (tokenClass == JsonDbQueryTokenizer::Operator) {
            result.append(c);
            if (getTokenClass(mInput[mPos]) == JsonDbQueryTokenizer::Operator)
                result.append(mInput[mPos++]);
            return result;
        } else if (c == '"') {
            // match string
            result.append(c);
            bool escaped = false;
            QChar sc;
            int size = mInput.size();
            int i;
            //qDebug() << "start of string";
            for (i = mPos; (i < size); i++) {
                sc = mInput[i];
                //qDebug() << i << sc << escaped;
                if (!escaped && (sc == '"'))
                    break;
                escaped = (sc == '\\');
            }
            //qDebug() << "end" << i << sc << escaped;
            //qDebug() << mInput.mid(mPos, i-mPos+1);
            if ((i < size) && (sc == '"')) {
                //qDebug() << mPos << i-mPos << "string is" << mInput.mid(mPos, i-mPos);
                result.append(mInput.mid(mPos, i-mPos+1));
                mPos = i+1;
            } else {
                mPos = i;
                result = QString();
            }
            return result;
        } else if (c.isSpace()) {
            if (result.size())
                return result;
            else
                continue;
        } else {
            result.append(c);
            if (mPos < mInput.size()) {
                QChar next = mInput[mPos];
                JsonDbQueryTokenizer::TokenClass nextTokenClass = getTokenClass(next);
                //qDebug() << mInput[mPos] << mInput[mPos].unicode() << "nextTokenTokenClass" << nextTokenClass << indexExpression;
                if (nextTokenClass == JsonDbQueryTokenizer::Other)
                    continue;
                else if ((next == '[')
                         || (indexExpression && (next == ']'))) {
                    // handle [*] as a special case
                    result.append(mInput[mPos++]);
                    indexExpression = (next == '[');
                    continue;
                } else
                    return result;
            }
        }
    }
    return QString();
}

JsonDbQuery::~JsonDbQuery()
{
    queryTerms.clear();
    orderTerms.clear();
}

QVariant JsonDbQuery::parseJsonLiteral(const QString &json, QueryTerm *term, QsonMap &bindings, bool *ok)
{
    const ushort trueLiteral[] = {'t','r','u','e', 0};
    const ushort falseLiteral[] = {'f','a','l','s','e', 0};
    const ushort *literal = json.utf16();
    if (ok) { *ok = true; }
    switch (literal[0]) {
    case '"':
        term->setValue(json.mid(1, json.size()-2));
        break;
    case 't':
        // we will interpret  "true0something" as true is it a real problem ?
        for (int i = 1; i < 5 /* 'true0' length */; ++i) {
            if (trueLiteral[i] != literal[i]) {
                if (ok) { *ok = false; }
                return term->value();
            }
        }
        term->setValue(true);
        break;
    case 'f':
        // we will interpret  "false0something" as false is it a real problem ?
        for (int i = 1; i < 6  /* 'false0' length */; ++i) {
            if (falseLiteral[i] != literal[i]) {
                if (ok) { *ok = false; }
                return term->value();
            }
        }
        term->setValue(false);
        break;
    case '%':
    {
        const QString name = json.mid(1); // TODO use .midRef()?
        QVariant value = qsonToVariant(bindings.value<QsonElement>(name));
        term->setValue(value);
        break;
    }
    case 0:
        // This can happen if json.length() == 0
        if (ok) { *ok = false; }
        return term->value();
    default:
        int result = json.toInt(ok);
        if (ok) {
            term->setValue(result);
        } else {
            // bad luck, it can be only a double
            term->setValue(json.toDouble(ok));
        }
    }
    return term->value();
}

JsonDbQuery JsonDbQuery::parse(const QString &query, QsonMap &bindings)
{
    JsonDbQuery parsedQuery;
    parsedQuery.query = query;

    bool parseError = false;
    JsonDbQueryTokenizer tokenizer(query);
    QString token;
    while (!parseError
           && !(token = tokenizer.pop()).isEmpty()) {
        if (token != "[") {
            qCritical() << "unexpected token" << token;
            break;
        }
        token = tokenizer.pop();
        if (token == "?") {
            OrQueryTerm oqt;
            do {
                QString fieldSpec = tokenizer.pop();
                if (fieldSpec == "|")
                    fieldSpec = tokenizer.pop();

                QString opOrJoin = tokenizer.pop();
                QString op;
                QStringList joinFields;
                QString joinField;
                while (opOrJoin == "->") {
                    joinFields.append(fieldSpec);
                    fieldSpec = tokenizer.pop();
                    opOrJoin = tokenizer.pop();
                }
                if (joinFields.size())
                    joinField = joinFields.join("->");
                op = opOrJoin;


                QueryTerm term;
                if (!joinField.isEmpty())
                    term.setJoinField(joinField);
                term.setPropertyName(fieldSpec);
                term.setOp(op);
                if (op == "=~") {
                    QString tvs = tokenizer.pop();
                    if (!tvs.startsWith("\"")) {
                        parsedQuery.queryExplanation.append(QString("Failed to parse query regular expression '%1' in query '%2' %3 op %4")
                                                            .arg(tvs)
                                                            .arg(parsedQuery.query)
                                                            .arg(fieldSpec)
                                                            .arg(op));
                        parseError = true;
                        break;
                    }
                    QChar sep = tvs[1];
                    int eor = 1;
                    do {
                        eor = tvs.indexOf(sep, eor+1); // end of regexp;
                        //qDebug() << "tvs" << tvs << "eor" << eor << "tvs[eor-1]" << ((eor > 0) ? tvs[eor-1] : QChar('*'));
                        if (eor <= 1) {
                            parseError = true;
                            break;
                        }
                    } while ((eor > 0) && (tvs[eor-1] == '\\'));
                    QString modifiers = tvs.mid(eor+1,tvs.size()-eor-2);
                    if (gDebug) qDebug() << "modifiers" << modifiers;
                    if (gDebug) qDebug() << "regexp" << tvs.mid(2, eor-2);
                    if (modifiers.contains('w'))
                        term.regExp().setPatternSyntax(QRegExp::Wildcard);
                    if (modifiers.contains('i'))
                        term.regExp().setCaseSensitivity(Qt::CaseInsensitive);
                    //qDebug() << "pattern" << tvs.mid(2, eor-2);
                    term.regExp().setPattern(tvs.mid(2, eor-2));
                } else if (op != "exists") {
                    QString value = tokenizer.pop();
                    bool ok = true;;
                    if (value == "[") {
                        QVariantList values;
                        while (1) {
                            value = tokenizer.pop();
                            if (value == "]")
                                break;
                            parseJsonLiteral(value, &term, bindings, &ok);
                            if (!ok)
                                break;
                            values.append(term.value());
                            if (tokenizer.peek() == ",")
                                tokenizer.pop();
                        }
                        term.setValue(values);
                    } else {
                        parseJsonLiteral(value, &term, bindings, &ok);
                    }
                    if (!ok) {
                        parsedQuery.queryExplanation.append(QString("Failed to parse query value '%1' in query '%2' %3 op %4")
                                                            .arg(value)
                                                            .arg(parsedQuery.query)
                                                            .arg(fieldSpec)
                                                            .arg(op));
                        parseError = true;
                        break;
                    }
                }

                oqt.addTerm(term);
            } while (tokenizer.peek() != "]");
            parsedQuery.queryTerms.append(oqt);
        } else if (token == "=") {
            bool isMapObject = false;
            bool isListObject = false;
            bool inListObject = false;
            QString nextToken;
            while (!(nextToken = tokenizer.pop()).isEmpty()) {
                if (nextToken == "{")
                    isMapObject = true;
                else if (nextToken == "[") {
                    isListObject = true;
                    inListObject = true;
                } else if (nextToken == "]") {
                    if (!inListObject)
                        tokenizer.push(nextToken);
                    else
                        inListObject = false;
                    break;
                } else if (nextToken == "}") {
                    break;
                } else {
                    if (isMapObject) {
                        //qDebug() << "isMapObject" << nextToken << tokenizer.peek();
                        parsedQuery.mapKeyList.append(nextToken);
                        QString colon = tokenizer.pop();
                        if (colon != ":") {
                            parsedQuery.queryExplanation.append(QString("Parse error: expecting ':' but got '%1'").arg(colon));
                            parseError = true;
                            break;
                        }
                        nextToken = tokenizer.pop();
                    }
                    while (tokenizer.peek() == "->") {
                        QString op = tokenizer.pop();
                        nextToken.append(op);
                        nextToken.append(tokenizer.pop());
                    }
                    parsedQuery.mapExpressionList.append(nextToken);
                    QString maybeComma = tokenizer.pop();
                    if ((maybeComma == "}") || (maybeComma == "]")) {
                        tokenizer.push(maybeComma);
                        continue;
                    } else if (maybeComma != ",") {
                        parsedQuery.queryExplanation.append(QString("Parse error: expecting ',', ']', or '}' but got '%1'")
                                                            .arg(maybeComma));
                        parseError = true;
                        break;
                    }
                }
            }
            if (gDebug) qDebug() << "isListObject" << isListObject << parsedQuery.mapExpressionList;
            if (isListObject)
                parsedQuery.resultType = QVariant::List;
            else if (isMapObject)
                parsedQuery.resultType = QVariant::Map;
            else
                parsedQuery.resultType = QVariant::String;
        } else if ((token == "/") || (token == "\\") || (token == ">") || (token == "<")) {
            QString ordering = token;
            OrderTerm term;
            term.propertyName = tokenizer.pop();
            term.ascending = ((ordering == "/") || (ordering == ">"));
            parsedQuery.orderTerms.append(term);
        } else if (token == "count") {
            parsedQuery.mAggregateOperation = "count";
        } else if (token == "*") {
            // match all objects
        } else {
            qCritical() << QString("Parse error: expecting '?', '/', '\\', or 'count' but got '%1'").arg(token);
            parseError = true;
            break;
        }
        QString closeBracket = tokenizer.pop();
        if (closeBracket != "]") {
            qCritical() << QString("Parse error: expecting ']' but got '%1'").arg(closeBracket);
            parseError = true;
            break;
        }
    }

    if (parseError) {
        QStringList explanation = parsedQuery.queryExplanation;
        parsedQuery = JsonDbQuery();
        parsedQuery.queryExplanation = explanation;
        qCritical() << "Parser error: query" << query;
        return parsedQuery;
    }

    foreach (const OrQueryTerm &oqt, parsedQuery.queryTerms) {
        foreach (const QueryTerm &term, oqt.terms()) {
            if (term.propertyName() == JsonDbString::kTypeStr) {
                if (term.op() == "=") {
                    parsedQuery.mMatchedTypes.clear();
                    parsedQuery.mMatchedTypes.insert(term.value().toString());
                } else if (term.op() == "!=") {
                    parsedQuery.mMatchedTypes.insert(term.value().toString());
                }
            }
        }
    }
    // TODO look at this again
    if (!parsedQuery.queryTerms.size() && !parsedQuery.orderTerms.size()) {
        // match everything -- sort on type
        OrderTerm term;
        term.propertyName = JsonDbString::kTypeStr;
        term.ascending = true;
        parsedQuery.orderTerms.append(term);
    }

    //qDebug() << "queryTerms.size()" << parsedQuery.queryTerms.size();
    //qDebug() << "orderTerms.size()" << parsedQuery.orderTerms.size();
    return parsedQuery;
}

bool JsonDbQuery::match(const QsonMap &object, QHash<QString, QsonMap> *objectCache, JsonDbBtreeStorage *storage) const
{
    for (int i = 0; i < queryTerms.size(); i++) {
        const OrQueryTerm &orQueryTerm = queryTerms[i];
        bool matches = false;
        foreach (const QueryTerm &term, orQueryTerm.terms()) {
            const QString &joinPropertyName = term.joinField();
            const QString &op = term.op();
            const QVariant &termValue = term.value();

            QVariant objectFieldValue;
            if (!joinPropertyName.isEmpty()) {
                QsonMap joinedObject = object;
                const QVector<QStringList> &joinPaths = term.joinPaths();
                for (int j = 0; j < joinPaths.size(); j++) {
                    if (!joinPaths[j].size()) {
                        DBG() << term.joinField() << term.joinPaths();
                    }
                    QString uuidValue = JsonDb::propertyLookup(joinedObject, joinPaths[j]).toString();
                    if (objectCache && objectCache->contains(uuidValue))
                        joinedObject = objectCache->value(uuidValue);
                    else if (storage) {
                        ObjectKey objectKey(uuidValue);
                        storage->getObject(objectKey, joinedObject);
                        if (objectCache) objectCache->insert(uuidValue, joinedObject);
                    }
                }
                objectFieldValue = JsonDb::propertyLookup(joinedObject, term.fieldPath());
            } else {
                objectFieldValue = JsonDb::propertyLookup(object, term.fieldPath());
            }

            if ((op == "=") || (op == "==")) {
                if (termValue.type() == QVariant::Int) {
                    if (objectFieldValue.toInt() == termValue.toInt())
                        matches = true;
                } else if (termValue.type() == QVariant::Double) {
                    if (objectFieldValue.toDouble() == termValue.toDouble())
                        matches = true;
                } else {
                    if (objectFieldValue == termValue)
                        matches = true;
                }
            } else if ((op == "<>") || (op == "!=")) {
                if (objectFieldValue != termValue)
                    matches = true;
            } else if (op == "=~") {
                DBG() << objectFieldValue.toString() << term.regExpConst().exactMatch(objectFieldValue.toString());
                if (term.regExpConst().exactMatch(objectFieldValue.toString()))
                    matches = true;
            } else if (op == "<=") {
                if (objectFieldValue.type() == QVariant::Double) {
                    if (objectFieldValue.toDouble() <= termValue.toDouble())
                        matches = true;
                } else if (objectFieldValue.type() == QVariant::Bool) {
                    if (objectFieldValue.toBool() <= termValue.toBool())
                        matches = true;
                } else if (objectFieldValue.type() == QVariant::Int){
                    if (objectFieldValue.toInt() <= termValue.toInt())
                        matches = true;
                } else {
                    if (objectFieldValue.toString() <= termValue.toString())
                        matches = true;
                }
            } else if (op == "<") {
                if (objectFieldValue.type() == QVariant::Double) {
                    if (objectFieldValue.toDouble() < termValue.toDouble())
                        matches = true;
                } else if (objectFieldValue.type() == QVariant::Bool) {
                    if (objectFieldValue.toBool() < termValue.toBool())
                        matches = true;
                } else if (objectFieldValue.type() == QVariant::Int){
                    if (objectFieldValue.toInt() < termValue.toInt())
                        matches = true;
                } else {
                    if (objectFieldValue.toString() < termValue.toString())
                        matches = true;
                }
            } else if (op == ">=") {
                if (objectFieldValue.type() == QVariant::Double) {
                    if (objectFieldValue.toDouble() >= termValue.toDouble())
                        matches = true;
                } else if (objectFieldValue.type() == QVariant::Bool) {
                    if (objectFieldValue.toBool() >= termValue.toBool())
                        matches = true;
                } else if (objectFieldValue.type() == QVariant::Int){
                    if (objectFieldValue.toInt() >= termValue.toInt())
                        matches = true;
                } else {
                    if (objectFieldValue.toString() >= termValue.toString())
                        matches = true;
                }
            } else if (op == ">") {
                if (objectFieldValue.type() == QVariant::Double) {
                    if (objectFieldValue.toDouble() > termValue.toDouble())
                        matches = true;
                } else if (objectFieldValue.type() == QVariant::Bool) {
                    if (objectFieldValue.toBool() > termValue.toBool())
                        matches = true;
                } else if (objectFieldValue.type() == QVariant::Int){
                    if (objectFieldValue.toInt() > termValue.toInt())
                        matches = true;
                } else {
                    if (objectFieldValue.toString() > termValue.toString())
                        matches = true;
                }
            } else if (op == "exists") {
                if (objectFieldValue.isValid())
                    matches = true;
            } else if (op == "in") {
                if (0) qDebug() << __FUNCTION__ << __LINE__ << objectFieldValue << "in" << termValue
                                << termValue.toList().contains(objectFieldValue.toString());
                if (termValue.toList().contains(objectFieldValue.toString()))
                    matches = true;
            } else if (op == "notIn") {
                if (0) qDebug() << __FUNCTION__ << __LINE__ << objectFieldValue << "notIn" << termValue
                                << !termValue.toList().contains(objectFieldValue.toString());
                if (!termValue.toList().contains(objectFieldValue.toString()))
                    matches = true;
            } else if (op == "contains") {
                if (0) qDebug() << __FUNCTION__ << __LINE__ << objectFieldValue << "contains" << termValue
                                << objectFieldValue.toList().contains(termValue);
                if (objectFieldValue.toList().contains(termValue))
                    matches = true;
            } else if (op == "startsWith") {
                if (objectFieldValue.toString().startsWith(termValue.toString()))
                    matches = true;
            } else {
                qCritical() << "match" << "unhandled term" << term.propertyName() << term.op() << term.value() << term.joinField();
            }
        }
        if (!matches)
            return false;
    }
    return true;
}



QueryTerm::QueryTerm()
    : mJoinPaths()
    , mRegExp()
{
}
#if 0
QueryTerm::QueryTerm(const QueryTerm &other)
    : mPropertyName(other.mPropertyName)
    , mFieldPath(other.mFieldPath)
    , mOp(other.mOp)
    , mJoinField(other.mJoinField)
    , mJoinPaths(other.mJoinPaths)
    , mValue(other.mValue)
    , mRegExp()
{
}

QueryTerm &QueryTerm::operator=(const QueryTerm &other)
{
    mPropertyName = other.mPropertyName;
    mFieldPath = other.mFieldPath;
    mOp = other.mOp;
    mJoinField = other.mJoinField;
    mJoinPaths = other.mJoinPaths;
    mValue = other.mValue;
    return *this;
}
#endif

QueryTerm::~QueryTerm()
{
    mValue = QVariant();
    mJoinPaths.clear();
}

OrQueryTerm::OrQueryTerm()
{
}

OrQueryTerm::OrQueryTerm(const QueryTerm &term)
{
    mTerms.append(term);
}

OrQueryTerm::~OrQueryTerm()
{
}

QList<QString> OrQueryTerm::propertyNames() const
{
    QList<QString> propertyNames;
    foreach (const QueryTerm &term, mTerms) {
        QString propertyName = term.propertyName();
        if (!propertyNames.contains(propertyName))
            propertyNames.append(propertyName);
    }
    return propertyNames;
}

OrderTerm::OrderTerm()
{
}

OrderTerm::~OrderTerm()
{
}

} } // end namespace QtAddOn::JsonDb
