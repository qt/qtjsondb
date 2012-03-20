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

#ifndef NO_COLLATION_SUPPORT

#include "jsondbcollator.h"
#include "jsondbcollator_p.h"

#include <unicode/utypes.h>
#include <unicode/ucol.h>
#include <unicode/ustring.h>

#include "qdebug.h"

QT_BEGIN_NAMESPACE_JSONDB

JsonDbCollatorPrivate::~JsonDbCollatorPrivate()
{
    if (collator)
        ucol_close(collator);
}

static const int collationStringsCount = 13;
static const char * const collationStrings[collationStringsCount] = {
    "default",
    "big5han",
    "dict",
    "direct",
    "gb2312",
    "phonebk",
    "pinyin",
    "phonetic",
    "reformed",
    "standard",
    "stroke",
    "trad",
    "unihan"
};

JsonDbCollator::JsonDbCollator(const QLocale &locale, JsonDbCollator::Collation collation)
    : d(new JsonDbCollatorPrivate)
{
    d->locale = locale;
    if ((int)collation >= 0 && (int)collation < collationStringsCount)
        d->collation = collation;

    init();
}

JsonDbCollator::JsonDbCollator(const JsonDbCollator &other)
    : d(other.d)
{
    d->ref.ref();
}

JsonDbCollator::~JsonDbCollator()
{
    if (!d->ref.deref())
        delete d;
}

JsonDbCollator &JsonDbCollator::operator=(const JsonDbCollator &other)
{
    if (this != &other) {
        if (!d->ref.deref())
            delete d;
        *d = *other.d;
        d->ref.ref();
    }
    return *this;
}

void JsonDbCollator::setLocale(const QLocale &locale)
{
    if (d->ref.load() != 1)
        detach();
    if (d->collator)
        ucol_close(d->collator);
    d->locale = locale;

    init();
}

QLocale JsonDbCollator::locale() const
{
    return d->locale;
}

void JsonDbCollator::setCollation(JsonDbCollator::Collation collation)
{
    if ((int)collation < 0 || (int)collation >= collationStringsCount)
        return;

    if (d->ref.load() != 1)
        detach();
    if (d->collator)
        ucol_close(d->collator);
    d->collation = collation;

    init();
}

JsonDbCollator::Collation JsonDbCollator::collation() const
{
    return d->collation;
}

void JsonDbCollator::init()
{
    Q_ASSERT((int)d->collation < collationStringsCount);
    const char *collationString = collationStrings[(int)d->collation];
    UErrorCode status = U_ZERO_ERROR;
    QByteArray name = (d->locale.bcp47Name().replace(QLatin1Char('-'), QLatin1Char('_')) + QLatin1String("@collation=") + QLatin1String(collationString)).toLatin1();
    d->collator = ucol_open(name.constData(), &status);
    if (U_FAILURE(status))
        qWarning("Could not create collator: %d", status);

    // enable normalization by default
    ucol_setAttribute(d->collator, UCOL_NORMALIZATION_MODE, UCOL_ON, &status);

    // fetch options from the collator
    d->options = 0;

    switch (ucol_getAttribute(d->collator, UCOL_CASE_FIRST, &status)) {
    case UCOL_UPPER_FIRST: d->options |= JsonDbCollator::PreferUpperCase; break;
    case UCOL_LOWER_FIRST: d->options |= JsonDbCollator::PreferLowerCase; break;
    case UCOL_OFF:
    default:
        break;
    }

    switch (ucol_getAttribute(d->collator, UCOL_FRENCH_COLLATION, &status)) {
    case UCOL_ON: d->options |= JsonDbCollator::FrenchCollation; break;
    case UCOL_OFF:
    default:
        break;
    }

    switch (ucol_getAttribute(d->collator, UCOL_ALTERNATE_HANDLING, &status)) {
    case UCOL_SHIFTED: d->options |= JsonDbCollator::IgnorePunctuation; break;
    case UCOL_NON_IGNORABLE:
    default:
        break;
    }


    switch (ucol_getAttribute(d->collator, UCOL_CASE_LEVEL, &status)) {
    case UCOL_ON: d->options |= JsonDbCollator::ExtraCaseLevel; break;
    case UCOL_OFF:
    default:
        break;
    }

    switch (ucol_getAttribute(d->collator, UCOL_HIRAGANA_QUATERNARY_MODE, &status)) {
    case UCOL_ON: d->options |= JsonDbCollator::HiraganaQuaternaryMode; break;
    case UCOL_OFF:
    default:
        break;
    }

    switch (ucol_getAttribute(d->collator, UCOL_NUMERIC_COLLATION, &status)) {
    case UCOL_ON: d->options |= JsonDbCollator::NumericMode; break;
    case UCOL_OFF:
    default:
        break;
    }
}

void JsonDbCollator::detach()
{
    if (d->ref.load() != 1) {
        JsonDbCollatorPrivate *x = new JsonDbCollatorPrivate;
        x->ref.store(1);
        x->strength = d->strength;
        x->options = d->options;
        x->modified = true;
        x->collator = 0;
        if (!d->ref.deref())
            delete d;
        d = x;
    }
}

void JsonDbCollator::setStrength(JsonDbCollator::Strength strength)
{
    if (d->ref.load() != 1)
        detach();

    switch (strength) {
    case JsonDbCollator::PrimaryStrength:    ucol_setStrength(d->collator, UCOL_PRIMARY); break;
    case JsonDbCollator::SecondaryStrength:  ucol_setStrength(d->collator, UCOL_SECONDARY); break;
    case JsonDbCollator::TertiaryStrength:   ucol_setStrength(d->collator, UCOL_TERTIARY); break;
    case JsonDbCollator::QuaternaryStrength: ucol_setStrength(d->collator, UCOL_QUATERNARY); break;
    case JsonDbCollator::IdenticalStrength:  ucol_setStrength(d->collator, UCOL_IDENTICAL); break;
    default:
        qWarning("Invalid strength mode %d", strength);
    }
}

JsonDbCollator::Strength JsonDbCollator::strength() const
{
    switch (ucol_getStrength(d->collator)) {
    case UCOL_PRIMARY:   return JsonDbCollator::PrimaryStrength;
    case UCOL_SECONDARY: return JsonDbCollator::SecondaryStrength;
    case UCOL_QUATERNARY:return JsonDbCollator::QuaternaryStrength;
    case UCOL_IDENTICAL: return JsonDbCollator::IdenticalStrength;
    case UCOL_TERTIARY:
    default:
        return JsonDbCollator::TertiaryStrength;
    }
    return JsonDbCollator::TertiaryStrength;
}

void JsonDbCollator::setOptions(JsonDbCollator::Options options)
{
    if (d->ref.load() != 1)
        detach();

    d->options = options;

    UErrorCode status = U_ZERO_ERROR;

    if (options & JsonDbCollator::PreferUpperCase)
        ucol_setAttribute(d->collator, UCOL_CASE_FIRST, UCOL_UPPER_FIRST, &status);
    else if (options & JsonDbCollator::PreferLowerCase)
        ucol_setAttribute(d->collator, UCOL_CASE_FIRST, UCOL_LOWER_FIRST, &status);
    else
        ucol_setAttribute(d->collator, UCOL_CASE_FIRST, UCOL_OFF, &status);
    if (U_FAILURE(status))
        qWarning("ucol_setAttribute: Case First failed: %d", status);

    ucol_setAttribute(d->collator, UCOL_FRENCH_COLLATION,
                      options & JsonDbCollator::FrenchCollation ? UCOL_ON : UCOL_OFF, &status);
    if (U_FAILURE(status))
        qWarning("ucol_setAttribute: French collation failed: %d", status);

    ucol_setAttribute(d->collator, UCOL_NORMALIZATION_MODE,
                      options & JsonDbCollator::DisableNormalization ? UCOL_OFF : UCOL_ON, &status);
    if (U_FAILURE(status))
        qWarning("ucol_setAttribute: Normalization mode failed: %d", status);

    ucol_setAttribute(d->collator, UCOL_ALTERNATE_HANDLING,
                      (options & JsonDbCollator::IgnorePunctuation) ? UCOL_SHIFTED : UCOL_NON_IGNORABLE, &status);
    if (U_FAILURE(status))
        qWarning("ucol_setAttribute: Alternate handling failed: %d", status);

    ucol_setAttribute(d->collator, UCOL_CASE_LEVEL,
                      options & JsonDbCollator::ExtraCaseLevel ? UCOL_ON : UCOL_OFF, &status);
    if (U_FAILURE(status))
        qWarning("ucol_setAttribute: Case level failed: %d", status);

    ucol_setAttribute(d->collator, UCOL_HIRAGANA_QUATERNARY_MODE,
                      options & JsonDbCollator::HiraganaQuaternaryMode ? UCOL_ON : UCOL_OFF, &status);
    if (U_FAILURE(status))
        qWarning("ucol_setAttribute: hiragana mode failed: %d", status);

    ucol_setAttribute(d->collator, UCOL_NUMERIC_COLLATION,
                      options & JsonDbCollator::NumericMode ? UCOL_ON : UCOL_OFF, &status);
    if (U_FAILURE(status))
        qWarning("ucol_setAttribute: numeric collation failed: %d", status);
}

JsonDbCollator::Options JsonDbCollator::options() const
{
    return d->options;
}

int JsonDbCollator::compare(const QString &s1, const QString &s2) const
{
    return d->compare((ushort *)s1.constData(), s1.size(), (ushort *)s2.constData(), s2.size());
}

int JsonDbCollator::compare(const QStringRef &s1, const QStringRef &s2) const
{
    return d->compare((ushort *)s1.constData(), s1.size(), (ushort *)s2.constData(), s2.size());
}

int JsonDbCollatorPrivate::compare(ushort *s1, int len1, ushort *s2, int len2)
{
    const UCollationResult result =
            ucol_strcoll(collator, (const UChar *)s1, len1, (const UChar *)s2, len2);
    return result;
}

QByteArray JsonDbCollator::sortKey(const QString &string) const
{
    QByteArray result(16 + string.size() + (string.size() >> 2), Qt::Uninitialized);
    int size = ucol_getSortKey(d->collator, (const UChar *)string.constData(),
                               string.size(), (uint8_t *)result.data(), result.size());
    if (size > result.size()) {
#ifdef _DEBUG
        qDebug() << "### sortKey realloc from" << result.size() << "to" << size << "for string" << string.size();
#endif
        result.resize(size);
        size = ucol_getSortKey(d->collator, (const UChar *)string.constData(),
                               string.size(), (uint8_t *)result.data(), result.size());
    }
    result.truncate(size);
    return result;
}
QT_END_NAMESPACE_JSONDB

#endif // NO_COLLATION_SUPPORT
