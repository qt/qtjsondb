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

#include "jsondbquerytokenizer_p.h"

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

QT_END_NAMESPACE_JSONDB_PARTITION
