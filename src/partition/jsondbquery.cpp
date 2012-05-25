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

const char* JsonDbQueryTokenizer::sTokens[] = {
"[", "]", "{", "}", "/", "?", ",", ":", "|", "\\"
//operators are ordered by precedence
, "!=~", "=~", "!=", "<=", ">=", "->", "=", ">", "<"
, ""//end of the token list
};

JsonDbQueryTokenizer::JsonDbQueryTokenizer(QString input)
    : mInput(input), mPos(0)
{
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

QString JsonDbQueryTokenizer::popIdentifier()
{
    QString identifier = pop();
    if (identifier.startsWith('\"')
        && identifier.endsWith('\"'))
        identifier = identifier.mid(1, identifier.size()-2);
    return identifier;
}

QString JsonDbQueryTokenizer::peek()
{
    if (mNextToken.isEmpty()) {
        mNextToken = getNextToken();
    }
    return mNextToken;
}

QString JsonDbQueryTokenizer::getNextToken()
{
    QString result;

    while (mPos < mInput.size()) {
        QChar c = mInput[mPos++];
        if (c == '"') {
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
        } else if (result.size() && mPos+1 < mInput.size()) {
            //index expression?[n],[*]
            if (c == '[' && mInput[mPos+1] == ']') {
                result.append(mInput.mid(mPos-1,3));
                mPos += 2;
                continue;
            }
        }
        //operators
        int i = 0;
        while (sTokens[i][0] != 0) {
            if (mInput.midRef(mPos - 1,3).startsWith(QLatin1String(sTokens[i]))) {
                if (!result.isEmpty()) {
                    mPos --;
                    return result;
                }
                result.append(QLatin1String(sTokens[i]));
                mPos += strlen(sTokens[i]) - 1;
                return result;
            }
            i++;
        }
        result.append(mInput[mPos-1]);
    }//while
    return QString();
}

QJsonValue JsonDbQueryTerm::value() const
{
    if (mVariable.size())
        return mQuery->binding(mVariable);
    else
        return mValue;
}

JsonDbQuery::JsonDbQuery(const QList<JsonDbOrQueryTerm> &qt, const QList<JsonDbOrderTerm> &ot) :
    queryTerms(qt)
  , orderTerms(ot)
{
}

JsonDbQuery::~JsonDbQuery()
{
    queryTerms.clear();
    orderTerms.clear();
}

QJsonValue JsonDbQuery::parseJsonLiteral(const QString &json, JsonDbQueryTerm *term, const QJsonObject &bindings, bool *ok)
{
    const ushort trueLiteral[] = {'t','r','u','e', 0};
    const ushort falseLiteral[] = {'f','a','l','s','e', 0};
    const ushort *literal = json.utf16();
    QJsonValue value;
    Q_ASSERT(ok != NULL);
    *ok = true;
    switch (literal[0]) {
    case '"':
        value = json.mid(1, json.size()-2);
        break;
    case 't':
        // we will interpret  "true0something" as true is it a real problem ?
        for (int i = 1; i < 5 /* 'true0' length */; ++i) {
            if (trueLiteral[i] != literal[i]) {
                *ok = false;
                return value;
            }
        }
        value = true;
        break;
    case 'f':
        // we will interpret  "false0something" as false is it a real problem ?
        for (int i = 1; i < 6  /* 'false0' length */; ++i) {
            if (falseLiteral[i] != literal[i]) {
                *ok = false;
                return value;
            }
        }
        value = false;
        break;
    case '%':
    {
        const QString name = json.mid(1);
        if (bindings.contains(name))
            value = bindings.value(name);
        else
            if (term)
                term->setVariable(name);
        break;
    }
    case 0:
        // This can happen if json.length() == 0
        *ok = false;
        return value;
    default:
        int result = json.toInt(ok);
        if (*ok) {
            value = result;
        } else {
            // bad luck, it can be only a double
            value = json.toDouble(ok);
        }
    }
    if (term)
        term->setValue(value);
    return value;
}

QJsonArray JsonDbQuery::parseJsonArray(JsonDbQueryTokenizer &tokenizer, const QJsonObject &bindings, bool *ok)
{
    QJsonArray array;

    *ok = true;
    for (QString tkn = tokenizer.pop(); !tkn.isEmpty(); tkn = tokenizer.pop()) {
        if (tkn == QLatin1Char(']') || tkn == QLatin1Char('|'))
            break;
        else if (tkn == QLatin1Char('['))
            array.append(parseJsonArray(tokenizer, bindings, ok));
        else if (tkn == QLatin1Char('{'))
            array.append(parseJsonObject(tokenizer, bindings, ok));
        else
            array.append(parseJsonLiteral(tkn, 0, bindings, ok));
        tkn = tokenizer.pop();
        if (tkn == QLatin1Char(']')) {
            break;
        } else if (tkn != QLatin1Char(',')) {
            *ok = false;
            break;
        }
    }
    return array;
}

QJsonObject JsonDbQuery::parseJsonObject(JsonDbQueryTokenizer &tokenizer, const QJsonObject &bindings, bool *ok)
{
    QJsonObject object;
    *ok = true;
    for (QString tkn = tokenizer.popIdentifier(); !tkn.isEmpty(); tkn = tokenizer.popIdentifier()) {
        if (tkn == QLatin1Char('}') || tkn == QLatin1Char('|'))
            break;
        QString key = tkn;
        tkn = tokenizer.pop();
        if (tkn != QLatin1Char(':')) {
            *ok = false;
            break;
        }

        tkn = tokenizer.pop();
        QJsonValue value;
        if (tkn == QLatin1Char('['))
            value = parseJsonArray(tokenizer, bindings, ok);
        else if (tkn == QLatin1Char('{'))
            value = parseJsonObject(tokenizer, bindings, ok);
        else
            value = parseJsonLiteral(tkn, 0, bindings, ok);
        object.insert(key, value);
        tkn = tokenizer.pop();
        if (tkn == QLatin1Char('}')) {
            break;
        } else if (tkn != QLatin1Char(',')) {
            *ok = false;
            break;
        }
    }
    return object;
}

JsonDbQuery *JsonDbQuery::parse(const QString &query, const QJsonObject &bindings)
{
    JsonDbQuery *parsedQuery = new JsonDbQuery;
    parsedQuery->query = query;

    if (!query.startsWith(QLatin1Char('[')))
        return parsedQuery;

    bool parseError = false;
    JsonDbQueryTokenizer tokenizer(query);
    QString token;
    while (!parseError
           && !(token = tokenizer.pop()).isEmpty()) {
        if (token != QLatin1Char('[')) {
            qCritical() << "unexpected token" << token;
            break;
        }
        token = tokenizer.pop();
        if (token == QLatin1Char('?')) {
            JsonDbOrQueryTerm oqt;
            do {
                QString fieldSpec = tokenizer.popIdentifier();
                if (fieldSpec == QLatin1Char('|'))
                    fieldSpec = tokenizer.popIdentifier();

                QString opOrJoin = tokenizer.pop();
                QString op;
                QStringList joinFields;
                QString joinField;
                while (opOrJoin == QLatin1String("->")) {
                    joinFields.append(fieldSpec);
                    fieldSpec = tokenizer.popIdentifier();
                    opOrJoin = tokenizer.pop();
                }
                if (joinFields.size())
                    joinField = joinFields.join(QStringLiteral("->"));
                op = opOrJoin;


                JsonDbQueryTerm term(parsedQuery);
                if (!joinField.isEmpty())
                    term.setJoinField(joinField);
                if (fieldSpec.startsWith(QLatin1Char('%'))) {
                    const QString name = fieldSpec.mid(1);
                    if (bindings.contains(name)) {
                        QJsonValue val = bindings.value(name);
                        parsedQuery->bind(name, val);
                    }
                    term.setPropertyVariable(name);
                }
                else
                    term.setPropertyName(fieldSpec);
                term.setOp(op);
                if (op == QLatin1String("=~")
                        || op == QLatin1String("!=~")) {
                    QString tvs = tokenizer.pop();
                    int sepPos = 1; // assuming it's a literal "/regexp/modifiers"
                    if (tvs.startsWith(QLatin1Char('%'))) {
                        const QString name = tvs.mid(1);
                        if (bindings.contains(name)) {
                            tvs = bindings.value(name).toString();
                            sepPos = 0;
                        }
                    } else if (!tvs.startsWith(QLatin1Char('\"'))) {
                        parsedQuery->queryExplanation.append(QString::fromLatin1("Failed to parse query regular expression '%1' in query '%2' %3 op %4")
                                                             .arg(tvs)
                                                             .arg(parsedQuery->query)
                                                             .arg(fieldSpec)
                                                             .arg(op));
                        parseError = true;
                        break;
                    }
                    QChar sep = tvs[sepPos];
                    int eor = sepPos;
                    do {
                        eor = tvs.indexOf(sep, eor+1); // end of regexp;
                        //qDebug() << "tvs" << tvs << "eor" << eor << "tvs[eor-1]" << ((eor > 0) ? tvs[eor-1] : QChar('*'));
                        if (eor <= sepPos) {
                            parseError = true;
                            break;
                        }
                    } while ((eor > 0) && (tvs[eor-1] == '\\'));
                    QString modifiers = tvs.mid(eor+1,tvs.size()-eor-2*sepPos);
                    if (jsondbSettings->debug()) {
                        qDebug() << "modifiers" << modifiers;
                        qDebug() << "regexp" << tvs.mid(sepPos + 1, eor-sepPos-1);
                    }
                    if (modifiers.contains('w'))
                        term.regExp().setPatternSyntax(QRegExp::Wildcard);
                    if (modifiers.contains('i'))
                        term.regExp().setCaseSensitivity(Qt::CaseInsensitive);
                    //qDebug() << "pattern" << tvs.mid(2, eor-2);
                    term.regExp().setPattern(tvs.mid(sepPos + 1, eor-sepPos-1));
                } else if (op != QLatin1String("exists") && op != QLatin1String("notExists")) {
                    bool ok = true;

                    QString value = tokenizer.pop();
                    if (value == QLatin1Char('[')) {
                        QJsonValue value = parseJsonArray(tokenizer, bindings, &ok);
                        if (ok)
                            term.setValue(value);
                    } else if (value == QLatin1Char('{')) {
                        QJsonValue value = parseJsonObject(tokenizer, bindings, &ok);
                        if (ok)
                            term.setValue(value);
                    } else {
                        parseJsonLiteral(value, &term, bindings, &ok);
                    }

                    if (!ok) {
                        parsedQuery->queryExplanation.append(QString::fromLatin1("Failed to parse query value '%1' in query '%2' %3 op %4")
                                                             .arg(value)
                                                             .arg(parsedQuery->query)
                                                             .arg(fieldSpec)
                                                             .arg(op));
                        parseError = true;
                        break;
                    }
                }

                oqt.addTerm(term);
            } while (tokenizer.peek() != QLatin1Char(']'));
            parsedQuery->queryTerms.append(oqt);
        } else if (token == QLatin1Char('=')) {
            QString curlyBraceToken = tokenizer.pop();
            if (curlyBraceToken != QLatin1Char('{')) {
                parsedQuery->queryExplanation.append(QString::fromLatin1("Parse error: expecting '{' but got '%1'")
                                                     .arg(curlyBraceToken));
                parseError = true;
                break;
            }
            QString nextToken;
            while (!(nextToken = tokenizer.popIdentifier()).isEmpty()) {
                if (nextToken == QLatin1Char('}')) {
                    break;
                } else {
                    parsedQuery->mapKeyList.append(nextToken);
                    QString colon = tokenizer.pop();
                    if (colon != QLatin1Char(':')) {
                        parsedQuery->queryExplanation.append(QString::fromLatin1("Parse error: expecting ':' but got '%1'").arg(colon));
                        parseError = true;
                        break;
                    }
                    nextToken = tokenizer.popIdentifier();

                    while (tokenizer.peek() == QLatin1String("->")) {
                        QString op = tokenizer.pop();
                        nextToken.append(op);
                        nextToken.append(tokenizer.popIdentifier());
                    }
                    parsedQuery->mapExpressionList.append(nextToken);
                    QString maybeComma = tokenizer.pop();
                    if (maybeComma == QLatin1Char('}')) {
                        tokenizer.push(maybeComma);
                        continue;
                    } else if (maybeComma != QLatin1Char(',')) {
                        parsedQuery->queryExplanation.append(QString(QStringLiteral("Parse error: expecting ',', or '}' but got '%1'"))
                                                             .arg(maybeComma));
                        parseError = true;
                        break;
                    }
                }
            }
        } else if (token == QLatin1Char('/') || token == QLatin1Char('\\') ||
                   token == QLatin1Char('>') || token == QLatin1Char('<')) {
            QString ordering = token;
            JsonDbOrderTerm term;
            term.propertyName = tokenizer.popIdentifier();
            term.ascending = ordering == QLatin1Char('/') || ordering == QLatin1Char('>');
            parsedQuery->orderTerms.append(term);
        } else if (token == QLatin1String("count")) {
            parsedQuery->mAggregateOperation = QLatin1String("count");
        } else if (token == QLatin1Char('*')) {
            // match all objects
        } else {
            parsedQuery->queryExplanation.append(QString::fromLatin1("Parse error: expecting '?', '/', '\\', or 'count' but got '%1'").arg(token));
            parseError = true;
            break;
        }
        QString closeBracket = tokenizer.pop();
        if (closeBracket != QLatin1Char(']')) {
            parsedQuery->queryExplanation.append(QString::fromLatin1("Parse error: expecting ']' but got '%1'").arg(closeBracket));
            parseError = true;
            break;
        }
    }

    if (parseError) {
        QStringList explanation = parsedQuery->queryExplanation;
        delete parsedQuery;
        parsedQuery = new JsonDbQuery;
        parsedQuery->queryExplanation = explanation;
        return parsedQuery;
    }

    foreach (const JsonDbOrQueryTerm &oqt, parsedQuery->queryTerms) {
        foreach (const JsonDbQueryTerm &term, oqt.terms()) {
            if (term.propertyName() == JsonDbString::kTypeStr) {
                if (term.op() == QLatin1Char('=')) {
                    parsedQuery->mMatchedTypes.insert(term.value().toString());
                } else if (term.op() == QLatin1String("!=")) {
                    parsedQuery->mMatchedTypes.insert(term.value().toString());
                } else if (term.op() == QLatin1String("in")) {
                    foreach (const QJsonValue &v, term.value().toArray())
                        parsedQuery->mMatchedTypes.insert(v.toString());
                }
            }
        }
    }

    if (!parsedQuery->queryTerms.size() && !parsedQuery->orderTerms.size()) {
        // match everything -- sort on type
        JsonDbOrderTerm term;
        term.propertyName = JsonDbString::kTypeStr;
        term.ascending = true;
        parsedQuery->orderTerms.append(term);
    }

    return parsedQuery;
}

bool JsonDbQuery::match(const JsonDbObject &object, QHash<QString, JsonDbObject> *objectCache, JsonDbPartition *partition) const
{
    for (int i = 0; i < queryTerms.size(); i++) {
        const JsonDbOrQueryTerm &orQueryTerm = queryTerms[i];
        bool matches = false;
        foreach (const JsonDbQueryTerm &term, orQueryTerm.terms()) {
            const QString &joinPropertyName = term.joinField();
            const QString &op = term.op();
            const QJsonValue &termValue = term.value();
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
                objectFieldValue = joinedObject.valueByPath(term.fieldPath());
            } else {
                if (term.propertyName().isEmpty())
                    objectFieldValue = binding(term.propertyVariable());
                else
                    objectFieldValue = object.valueByPath(term.fieldPath());
            }
            if (op == QLatin1Char('=') || op == QLatin1String("==")) {
                matches = (objectFieldValue == termValue);
            } else if (op == QLatin1String("<>") || op == QLatin1String("!=")) {
                matches = (objectFieldValue != termValue);
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
                matches = JsonDbIndexQuery::lessThan(objectFieldValue, termValue) || (objectFieldValue == termValue);
            } else if (op == QLatin1Char('<')) {
                matches = JsonDbIndexQuery::lessThan(objectFieldValue, termValue);
            } else if (op == QLatin1String(">=")) {
                matches = JsonDbIndexQuery::greaterThan(objectFieldValue, termValue) || (objectFieldValue == termValue);
            } else if (op == QLatin1Char('>')) {
                matches = JsonDbIndexQuery::greaterThan(objectFieldValue, termValue);
            } else if (op == QLatin1String("exists")) {
                matches = (objectFieldValue.type() != QJsonValue::Undefined);
            } else if (op == QLatin1String("notExists")) {
                matches = (objectFieldValue.type() == QJsonValue::Undefined);
            } else if (op == QLatin1String("in")) {
                if (0) qDebug() << __FUNCTION__ << __LINE__ << objectFieldValue << "in" << termValue
                                << termValue.toArray().contains(objectFieldValue);
                matches = termValue.toArray().contains(objectFieldValue);
            } else if (op == QLatin1String("notIn")) {
                if (0) qDebug() << __FUNCTION__ << __LINE__ << objectFieldValue << "notIn" << termValue
                                << !termValue.toArray().contains(objectFieldValue);
                matches = !termValue.toArray().contains(objectFieldValue);
            } else if (op == QLatin1String("contains")) {
                if (0) qDebug() << __FUNCTION__ << __LINE__ << objectFieldValue << "contains" << termValue
                                << objectFieldValue.toArray().contains(termValue);
                matches = objectFieldValue.toArray().contains(termValue);
            } else if (op == QLatin1String("notContains")) {
                if (0) qDebug() << __FUNCTION__ << __LINE__ << objectFieldValue << "notContains" << termValue
                                << !objectFieldValue.toArray().contains(termValue);
                matches = !objectFieldValue.toArray().contains(termValue);
            } else if (op == QLatin1String("startsWith")) {
                matches = (objectFieldValue.type() == QJsonValue::String
                           && objectFieldValue.toString().startsWith(termValue.toString()));
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



JsonDbQueryTerm::JsonDbQueryTerm(const JsonDbQuery *query)
    : mQuery(query), mJoinPaths()
{
}

JsonDbQueryTerm::~JsonDbQueryTerm()
{
    mValue = QJsonValue();
    mJoinPaths.clear();
}

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

QT_END_NAMESPACE_JSONDB_PARTITION
