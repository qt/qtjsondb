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

#ifndef JSONDBCOLLATOR_H
#define JSONDBCOLLATOR_H

#ifndef NO_COLLATION_SUPPORT

#include <QString>
#include <QLocale>

#include "jsondbpartitionglobal.h"

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE_JSONDB_PARTITION

class JsonDbCollatorPrivate;

class Q_JSONDB_PARTITION_EXPORT JsonDbCollator
{
public:
    enum Strength {
        PrimaryStrength = 1,
        BaseLetterStrength = PrimaryStrength,

        SecondaryStrength = 2,
        AccentsStrength = SecondaryStrength,

        TertiaryStrength = 3,
        CaseStrength = TertiaryStrength,

        QuaternaryStrength = 4,
        PunctuationStrength = QuaternaryStrength,

        IdenticalStrength = 5,
        CodepointStrength = IdenticalStrength
    };

    enum Option {
        PreferUpperCase        = 0x01,
        PreferLowerCase        = 0x02,
        FrenchCollation        = 0x04,
        DisableNormalization   = 0x08,
        IgnorePunctuation      = 0x10,
        ExtraCaseLevel         = 0x20,
        HiraganaQuaternaryMode = 0x40,
        NumericMode            = 0x80
    };
    Q_DECLARE_FLAGS(Options, Option)

    enum Collation {
        Default,
        Big5Han,
        Dictionary,
        Direct,
        GB2312Han,
        PhoneBook,
        Pinyin,
        Phonetic,
        Reformed,
        Standard,
        Stroke,
        Traditional,
        UniHan
    };

    JsonDbCollator(const QLocale &locale = QLocale(), JsonDbCollator::Collation collation = JsonDbCollator::Default);
    JsonDbCollator(const JsonDbCollator &);
    ~JsonDbCollator();
    JsonDbCollator &operator=(const JsonDbCollator &);

    void setLocale(const QLocale &locale);
    QLocale locale() const;

    void setCollation(Collation collation);
    Collation collation() const;

    void setStrength(Strength);
    Strength strength() const;

    void setOptions(Options);
    Options options() const;

    enum CasePreference {
        IgnoreCase = 0x0,
        UpperCase  = 0x1,
        LowerCase  = 0x2
    };

    bool isCaseSensitive() const
    { return options() & (PreferUpperCase | PreferLowerCase); }
    CasePreference casePreference() const
    {
        int value = options() & 0x3;
        if (value == 3)
            return UpperCase;
        return CasePreference(value);
    }
    void setCasePreference(CasePreference c)
    {
        Options o = options() & ~(PreferUpperCase | PreferLowerCase);
        if (c == UpperCase)
            o |= PreferUpperCase;
        else if (c == LowerCase)
            o |= PreferLowerCase;
        setOptions(o);
    }

    void setNumericMode(bool on)
    { setOptions(on ? options() | NumericMode : options() & ~NumericMode); }
    bool numericMode() const
    { return options() & NumericMode; }

    int compare(const QString &s1, const QString &s2) const;
    int compare(const QStringRef &s1, const QStringRef &s2) const;
    bool operator()(const QString &s1, const QString &s2) const
    { return compare(s1, s2) < 0; }

    QByteArray sortKey(const QString &string) const;

private:
    JsonDbCollatorPrivate *d;

    void detach();
    void init();
};

QT_END_NAMESPACE_JSONDB_PARTITION

QT_END_HEADER

#endif // NO_COLLATION_SUPPORT

#endif // JSONDBCOLLATOR_H
