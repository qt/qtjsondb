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

#include "jsondbqueryparser.h"
#include "jsondbquerytokenizer_p.h"
#include "jsondbutils_p.h"
#include "jsondbstrings.h"
#include "jsondbsettings.h"

QT_BEGIN_NAMESPACE_JSONDB_PARTITION

class JsonDbQueryParserPrivate
{
    Q_DECLARE_PUBLIC(JsonDbQueryParser)
public:
    JsonDbQueryParserPrivate(JsonDbQueryParser *q)
        : q_ptr(q) { }
    bool parse();
    QJsonValue termValue(const JsonDbQueryTerm &term) const;

    JsonDbQueryParser *q_ptr;
    QString query;
    QMap<QString, QJsonValue> bindings;
    JsonDbQuery spec;
    QString errorString;
};

static QJsonObject parseJsonObject(JsonDbQueryTokenizer &tokenizer, bool *ok);

static QJsonValue parseJsonLiteral(const QString &json, JsonDbQueryTerm *term, bool *ok)
{
    const ushort trueLiteral[] = {'t','r','u','e', 0};
    const ushort falseLiteral[] = {'f','a','l','s','e', 0};
    const ushort *literal = json.utf16();
    QJsonValue value(QJsonValue::Undefined);
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

static QJsonArray parseJsonArray(JsonDbQueryTokenizer &tokenizer, bool *ok)
{
    QJsonArray array;

    Q_ASSERT(ok != 0);
    *ok = true;
    for (QString tkn = tokenizer.pop(); !tkn.isEmpty(); tkn = tokenizer.pop()) {
        if (tkn == QLatin1Char(']') || tkn == QLatin1Char('|'))
            break;
        else if (tkn == QLatin1Char('['))
            array.append(parseJsonArray(tokenizer, ok));
        else if (tkn == QLatin1Char('{'))
            array.append(parseJsonObject(tokenizer, ok));
        else
            array.append(parseJsonLiteral(tkn, 0, ok));
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

static QJsonObject parseJsonObject(JsonDbQueryTokenizer &tokenizer, bool *ok)
{
    QJsonObject object;
    Q_ASSERT(ok != 0);
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
            value = parseJsonArray(tokenizer, ok);
        else if (tkn == QLatin1Char('{'))
            value = parseJsonObject(tokenizer, ok);
        else
            value = parseJsonLiteral(tkn, 0, ok);
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

bool JsonDbQueryParserPrivate::parse()
{
    spec = JsonDbQuery();

    if (!query.startsWith(QLatin1Char('[')))
        return false;
    spec.query = query;
    spec.bindings = bindings;

    QStringList queryExplanation;
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


                JsonDbQueryTerm term;
                if (!joinField.isEmpty())
                    term.setJoinField(joinField);
                if (fieldSpec.startsWith(QLatin1Char('%'))) {
                    const QString name = fieldSpec.mid(1);
                    QJsonValue val = bindings.value(name, QJsonValue(QJsonValue::Undefined));
                    if (!val.isUndefined())
                        spec.bindings.insert(name, val);
                    term.setPropertyVariable(name);
                } else {
                    term.setPropertyName(fieldSpec);
                }
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
                        queryExplanation.append(QStringLiteral("Failed to parse query regular expression '%1' in query '%2' %3 op %4")
                                                .arg(tvs, query, fieldSpec, op));
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
                        QJsonValue value = parseJsonArray(tokenizer, &ok);
                        if (ok)
                            term.setValue(value);
                    } else if (value == QLatin1Char('{')) {
                        QJsonValue value = parseJsonObject(tokenizer, &ok);
                        if (ok)
                            term.setValue(value);
                    } else {
                        parseJsonLiteral(value, &term, &ok);
                    }

                    if (!ok) {
                        queryExplanation.append(QStringLiteral("Failed to parse query value '%1' in query '%2' %3 op %4")
                                                .arg(value, query, fieldSpec, op));
                        parseError = true;
                        break;
                    }
                }

                oqt.addTerm(term);
            } while (tokenizer.peek() != QLatin1Char(']'));
            spec.queryTerms.append(oqt);
        } else if (token == QLatin1Char('=')) {
            QString curlyBraceToken = tokenizer.pop();
            if (curlyBraceToken != QLatin1Char('{')) {
                queryExplanation.append(QStringLiteral("Parse error: expecting '{' but got '%1'")
                                        .arg(curlyBraceToken));
                parseError = true;
                break;
            }
            QString nextToken;
            while (!(nextToken = tokenizer.popIdentifier()).isEmpty()) {
                if (nextToken == QLatin1Char('}')) {
                    break;
                } else {
                    spec.mapKeyList.append(nextToken);
                    QString colon = tokenizer.pop();
                    if (colon != QLatin1Char(':')) {
                        queryExplanation.append(QStringLiteral("Parse error: expecting ':' but got '%1'").arg(colon));
                        parseError = true;
                        break;
                    }
                    nextToken = tokenizer.popIdentifier();

                    while (tokenizer.peek() == QLatin1String("->")) {
                        QString op = tokenizer.pop();
                        nextToken.append(op);
                        nextToken.append(tokenizer.popIdentifier());
                    }
                    spec.mapExpressionList.append(nextToken);
                    QString maybeComma = tokenizer.pop();
                    if (maybeComma == QLatin1Char('}')) {
                        tokenizer.push(maybeComma);
                        continue;
                    } else if (maybeComma != QLatin1Char(',')) {
                        queryExplanation.append(QStringLiteral("Parse error: expecting ',', or '}' but got '%1'")
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
            spec.orderTerms.append(term);
        } else if (token == QLatin1String("count")) {
            spec.aggregateOperation = QStringLiteral("count");
        } else if (token == QLatin1Char('*')) {
            // match all objects
        } else {
            queryExplanation.append(QStringLiteral("Parse error: expecting '?', '/', '\\', or 'count' but got '%1'").arg(token));
            parseError = true;
            break;
        }
        QString closeBracket = tokenizer.pop();
        if (closeBracket != QLatin1Char(']')) {
            queryExplanation.append(QStringLiteral("Parse error: expecting ']' but got '%1'").arg(closeBracket));
            parseError = true;
            break;
        }
    }

    if (parseError) {
        errorString = queryExplanation.join(QLatin1String(";"));
        spec = JsonDbQuery();
        return false;
    }

    foreach (const JsonDbOrQueryTerm &oqt, spec.queryTerms) {
        foreach (const JsonDbQueryTerm &term, oqt.terms()) {
            if (term.propertyName() == JsonDbString::kTypeStr) {
                if (term.op() == QLatin1Char('=')) {
                    spec.mMatchedTypes.insert(termValue(term).toString());
                } else if (term.op() == QLatin1String("!=")) {
                    spec.mMatchedTypes.insert(termValue(term).toString());
                } else if (term.op() == QLatin1String("in")) {
                    foreach (const QJsonValue &v, termValue(term).toArray())
                        spec.mMatchedTypes.insert(v.toString());
                }
            }
        }
    }

    if (!spec.queryTerms.size() && !spec.orderTerms.size()) {
        // match everything -- sort on type
        JsonDbOrderTerm term;
        term.propertyName = JsonDbString::kTypeStr;
        term.ascending = true;
        spec.orderTerms.append(term);
    }

    return true;
}

QJsonValue JsonDbQueryParserPrivate::termValue(const JsonDbQueryTerm &term) const
{
    return term.hasValue() ? term.value() : bindings.value(term.variable(), QJsonValue(QJsonValue::Undefined));
}

JsonDbQueryParser::JsonDbQueryParser()
    : d_ptr(new JsonDbQueryParserPrivate(this))
{
}

JsonDbQueryParser::~JsonDbQueryParser()
{
}

void JsonDbQueryParser::setQuery(const QString &query)
{
    Q_D(JsonDbQueryParser);
    d->query = query;
}

QString JsonDbQueryParser::query() const
{
    Q_D(const JsonDbQueryParser);
    return d->query;
}

void JsonDbQueryParser::setBindings(const QJsonObject &bindings)
{
    Q_D(JsonDbQueryParser);
    d->bindings.clear();
    QJsonObject::const_iterator it = bindings.constBegin(), e = bindings.constEnd();
    for (; it != e; ++it)
        d->bindings.insert(it.key(), it.value());
}

void JsonDbQueryParser::setBindings(const QMap<QString, QJsonValue> &bindings)
{
    Q_D(JsonDbQueryParser);
    d->bindings = bindings;
}

QMap<QString, QJsonValue> JsonDbQueryParser::bindings() const
{
    Q_D(const JsonDbQueryParser);
    return d->bindings;
}

bool JsonDbQueryParser::parse()
{
    Q_D(JsonDbQueryParser);
    return d->parse();
}

QString JsonDbQueryParser::errorString() const
{
    Q_D(const JsonDbQueryParser);
    return d->errorString;
}

JsonDbQuery JsonDbQueryParser::result() const
{
    Q_D(const JsonDbQueryParser);
    return d->spec;
}

QT_END_NAMESPACE_JSONDB_PARTITION
