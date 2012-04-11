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

#include <QtCore/qhash.h>
#include <QtCore/qlist.h>
#include <QtCore/qregexp.h>
#include "object.h"

#ifndef CHECKPOINTS_H
#define CHECKPOINTS_H

QT_BEGIN_HEADER

namespace SchemaValidation {

/**
  \internal
  This template is used for hash computation for static latin1 strings.
 */
template<ushort C1 = 0, ushort C2 = 0, ushort C3 = 0, ushort C4 = 0, ushort C5 = 0,
         ushort C6 = 0, ushort C7 = 0, ushort C8 = 0, ushort C9 = 0, ushort C10 = 0,
         ushort C11 = 0, ushort C12 = 0, ushort C13 = 0, ushort C14 = 0, ushort C15 = 0,
         ushort C16 = 0, ushort C17 = 0, ushort C18 = 0>
struct QStaticStringHash
{
    typedef QStaticStringHash<C2, C3, C4, C5, C6, C7, C8, C9, C10, C11, C12, C13, C14, C15, C16, C17, C18> Suffix;

    const static int Hash = (C1 ^ Suffix::Hash) + 5;
    //(C1 ^ ( (C2 ^ (...)) +5 )) +5
};

template<>
struct QStaticStringHash<>
{
    typedef QStaticStringHash<> Suffix;
    const static int Hash = 0;

    /**
      \internal
      This function has to be align with qStringHash::Hash value
    */
    inline static int hash(const QString &string)
    {
        const ushort *str = reinterpret_cast<const ushort*>(string.constData());
        return hash(str, 0, string.length());
    }
private:
    inline static int hash(const ushort *str, const int index, const int length)
    {
        return index != length ? (str[index] ^ hash(str, index + 1, length)) + 5
                     : 0;
    }
};

template<class T>
class SchemaPrivate<T>::NullCheck : public Check {
public:
    NullCheck(SchemaPrivate *schema)
        : Check(schema, "") // TODO
    {}
protected:
    virtual bool doCheck(const Value&) { return true; }
};

// 5.1
template<class T>
class SchemaPrivate<T>::CheckType : public Check {
    enum Type {StringType = 0x0001,
               NumberType = 0x0002,
               IntegerType = 0x0004,
               BooleanType = 0x0008,
               ObjectType = 0x0010,
               ArrayType = 0x0020,
               NullType = 0x0040,
               AnyType = 0x0080,
               UnknownType = 0};
public:
    CheckType(SchemaPrivate *schema, const Value& type)
        : Check(schema, "Type check failed for %1")
        , m_type(UnknownType)
    {
        bool ok;
        QStringList typesName;

        QString typeName = type.toString(&ok);
        if (!ok) {
            ValueList typesList = type.toList(&ok);
            Q_ASSERT_X(ok, Q_FUNC_INFO, "Type is neither a string nor an array");
            typename ValueList::const_iterator i;
            for (i = typesList.constBegin(); i != typesList.constEnd(); ++i) {
                typeName = (*i).toString(&ok);
                if (ok) {
                    typesName << typeName;
                }
            }
        } else {
            typesName << typeName;
        }
        foreach (const QString &name, typesName) {
            const int hash = QStaticStringHash<>::hash(name.toLower());

            // FIXME there are chances of conflicts, do we care? What is chance for new types in JSON?
            // FIXME we need to check only 2 chars. That would be faster.
            // FIXME probably we want to support schemas here too.
            switch (hash) {
            case QStaticStringHash<'s','t','r','i','n','g'>::Hash:
                m_type |= StringType;
                break;
            case QStaticStringHash<'n','u','m','b','e','r'>::Hash:
                m_type |= NumberType;
                m_type |= IntegerType; // an integer is also a number
                break;
            case QStaticStringHash<'i','n','t','e','g','e','r'>::Hash:
                m_type |= IntegerType;
                break;
            case QStaticStringHash<'b','o','o','l','e','a','n'>::Hash:
                m_type |= BooleanType;
                break;
            case QStaticStringHash<'o','b','j','e','c','t'>::Hash:
                m_type |= ObjectType;
                break;
            case QStaticStringHash<'a','r','r','a','y'>::Hash:
                m_type |= ArrayType;
                break;
            case QStaticStringHash<'a','n','y'>::Hash:
                m_type |= AnyType;
                break;
            case QStaticStringHash<'n','u','l','l'>::Hash:
                m_type |= NullType;
                break;
            default:
                m_type |= UnknownType;
            }
        }

//        qDebug() << Q_FUNC_INFO << m_type << type.toString(&ok);
    }

    virtual bool doCheck(const Value &value)
    {
        if (m_type == UnknownType)
            return true;

        bool result = findType(value) & m_type;
//        bool ok;
//        qDebug() << Q_FUNC_INFO << findType(value)  << m_type << value.toString(&ok) << result;
        return result;
    }

private:
    inline Type findType(const Value &value) const
    {
        bool ok;
        // Lets assume that the value is valid.
        switch (m_type) {
        case StringType:
            value.toString(&ok);
            if (ok)
                return StringType;
            break;
        case NumberType:
            value.toDouble(&ok);
            if (ok)
                return NumberType;
            break;
        case IntegerType:
            value.toInt(&ok);
            if (ok)
                return IntegerType;
            break;
        case BooleanType:
            value.toBool(&ok);
            if (ok)
                return BooleanType;
            break;
        case ObjectType:
            value.toObject(&ok);
            if (ok)
                return ObjectType;
            break;
        case NullType:
            value.toNull(&ok);
            if (ok)
                return NullType;
            break;
        case AnyType:
            return AnyType;
        case UnknownType:
            break;
        default:
            break;
        };

        //TODO FIXME it can be better
        value.toInt(&ok);
        if (ok)
            return IntegerType;
        value.toDouble(&ok);
        if (ok)
            return NumberType;
        value.toObject(&ok);
        if (ok)
            return ObjectType;
        value.toString(&ok);
        if (ok)
            return StringType;
        value.toBool(&ok);
        if (ok)
            return BooleanType;
        value.toList(&ok);
        if (ok)
            return ArrayType;
        return AnyType;
    }

    uint m_type;
};

// 5.2
template<class T>
class SchemaPrivate<T>::CheckProperties : public Check {
public:
    CheckProperties(SchemaPrivate *schema, const Value &properties)
        : Check(schema, "Properties check failed for %1")
    {
        bool ok;
        const Object obj = properties.toObject(&ok);
        Q_ASSERT(ok);

        QList<Key> propertyNames = obj.propertyNames();
//        qDebug() << "    propertyNames: " << propertyNames <<this;

        m_checks.reserve(propertyNames.count());
        foreach (const Key &propertyName, propertyNames) {
            QVarLengthArray<Check *, 4> checks;
            const Object propertyChecks = obj.property(propertyName).toObject(&ok);
//            qDebug() << "    propertyChecks:" << propertyChecks;

            Q_ASSERT(ok);
            foreach (const Key &key, propertyChecks.propertyNames()) {
//                bool ok;
//                qDebug() << "        key:" << key << this << propertyChecks.property(key).toString(&ok)<< propertyChecks.property(key).toInt(&ok);
                checks.append(schema->createCheckPoint(key, propertyChecks.property(key)));
            }
            m_checks.insert(propertyName, checks);
        }
    }

    ~CheckProperties()
    {
        typename QHash<const Key, QVarLengthArray<Check *, 4> >::const_iterator i;
        for (i = m_checks.constBegin(); i != m_checks.constEnd(); ++i) {
            typename QVarLengthArray<Check *, 4>::const_iterator j;
            for (j = i.value().constBegin(); j != i.value().constEnd(); ++j){
                delete *j;
            }
        }
    }

    virtual bool doCheck(const Value &value)
    {
        bool ok;
        Object object = value.toObject(&ok);
        if (!ok)
            return false;

        //qDebug() << Q_FUNC_INFO;
        foreach (const Key &key, object.propertyNames()) {
            QVarLengthArray<Check *, 4> empty;
            QVarLengthArray<Check *, 4> checks = m_checks.value(key, empty);
            Value property = object.property(key);
            foreach (Check *check, checks) {
                //qDebug()  <<"CHECKING:" << check;
                if (!check->check(property)) {
                    return false;
                }
            }
        }
        return true;
    }
private:
    QHash<const Key, QVarLengthArray<Check *, 4> > m_checks;
};

// 5.5
template<class T>
class SchemaPrivate<T>::CheckItems : public Check {
public:
    CheckItems(SchemaPrivate *schemap, const Value &schema)
        : Check(schemap, "Items check failed for %1")
    {
        // qDebug()  << Q_FUNC_INFO << this;
        bool ok;
        Object obj = schema.toObject(&ok);
        Q_ASSERT(ok);
        m_schema = Schema<T>(obj, schemap->m_callbacks);
    }

    virtual bool doCheck(const Value& value)
    {
        //qDebug() << Q_FUNC_INFO << this;
        bool ok;
        ValueList array = value.toList(&ok);
        if (!ok)
            return false;

        typename ValueList::const_iterator i;
        for (i = array.constBegin(); i != array.constEnd(); ++i) {
            if (!m_schema.check(*i, Check::m_schema->m_callbacks)) {
                return false;
            }
        }
        return true;
    }
private:
    Schema<T> m_schema;
};

// 5.7
template<class T>
class SchemaPrivate<T>::CheckRequired : public Check {
public:
    CheckRequired(SchemaPrivate *schema, const Value &required)
        : Check(schema, "Check required field")  // TODO what to do about Required ?
    {
        bool ok;
        m_req = required.toBool(&ok);
        if (!ok) {
            // maybe someone used string instead of bool
            QString value = required.toString(&ok).toLower();
            if (value == QString::fromLatin1("false"))
                m_req = false;
            else if (value == QString::fromLatin1("true"))
                m_req = true;
            else
                Q_ASSERT(false);

            qWarning() << QString::fromLatin1("Wrong 'required' syntax found, instead of boolean type a string was used");
        }
        Q_ASSERT(ok);
        if (m_req)
            Check::m_schema->m_maxRequired++;
    }

    virtual bool doCheck(const Value&)
    {
        //qDebug() << Q_FUNC_INFO << m_schema << this;
        if (m_req)
            Check::m_schema->m_requiredCount++;
        return true;
    }
private:
    bool m_req;
};

// 5.9
template<class T>
class SchemaPrivate<T>::CheckMinimum : public Check {
public:
    CheckMinimum(SchemaPrivate *schema, const Value &minimum)
        : Check(schema, "Minimum check failed for %1")
    {
        bool ok;
        m_min = minimum.toDouble(&ok);
        Q_ASSERT(ok);
    }

    virtual bool doCheck(const Value &value)
    {
        bool ok;
        return value.toDouble(&ok) >= m_min && ok;
    }
private:
    double m_min;
};

// 5.10
template<class T>
class SchemaPrivate<T>::CheckMaximum : public Check {
public:
    CheckMaximum(SchemaPrivate *schema, const Value &maximum)
        : Check(schema, "Maximum check failed for %1")
    {
        // qDebug()  << Q_FUNC_INFO << this;
        bool ok;
        m_max = maximum.toDouble(&ok);
        Q_ASSERT(ok);
    }

    virtual bool doCheck(const Value &value)
    {
        //qDebug() << Q_FUNC_INFO << value << m_max << this;
        bool ok;
        return value.toDouble(&ok) <= m_max && ok;
    }
private:
    double m_max;
};


// 5.11
template<class T>
class SchemaPrivate<T>::CheckExclusiveMinimum : public Check {
public:
    CheckExclusiveMinimum(SchemaPrivate *schema, const Value &minimum)
        : Check(schema, "Exclusive minimum check failed for %1")
    {
        bool ok;
        m_min = minimum.toDouble(&ok);
        Q_ASSERT(ok);
    }

    virtual bool doCheck(const Value &value)
    {
        bool ok;
        return value.toDouble(&ok) > m_min && ok;
    }
private:
    double m_min;
};

// 5.12
template<class T>
class SchemaPrivate<T>::CheckExclusiveMaximum : public Check {
public:
    CheckExclusiveMaximum(SchemaPrivate *schema, const Value &maximum)
        : Check(schema, "Exclusive minimum check failed for %1")
    {
        bool ok;
        m_max = maximum.toDouble(&ok);
        Q_ASSERT(ok);
    }

    virtual bool doCheck(const Value &value)
    {
        bool ok;
        return value.toDouble(&ok) < m_max && ok;
    }
private:
    double m_max;
};

// 5.13
template<class T>
class SchemaPrivate<T>::CheckMinItems : public Check {
public:
    CheckMinItems(SchemaPrivate *schema, const Value& minimum)
        : Check(schema, "Minimum item count check failed for %1")
    {
        bool ok;
        m_min = minimum.toInt(&ok);
        Q_ASSERT(ok);
    }

    virtual bool doCheck(const Value &value)
    {
        bool ok;
        int count = value.toList(&ok).size();
        return count >= m_min && ok;
    }
private:
    int m_min;
};

// 5.14
template<class T>
class SchemaPrivate<T>::CheckMaxItems : public Check {
public:
    CheckMaxItems(SchemaPrivate *schema, const Value& maximum)
        : Check(schema, "Maximum item count check failed for %1")
    {
        bool ok;
        m_max = maximum.toInt(&ok);
        Q_ASSERT(ok);
    }

    virtual bool doCheck(const Value &value)
    {
        bool ok;
        int count = value.toList(&ok).size();
        return count <= m_max && ok;
    }

private:
    int m_max;
};

// 5.16
template<class T>
class SchemaPrivate<T>::CheckPattern : public Check {
public:
    CheckPattern(SchemaPrivate *schema, const Value& patternValue)
        : Check(schema, "Pattern check failed for %1")
    {
        bool ok;
        QString patternString = patternValue.toString(&ok);
        m_regexp.setPattern(patternString);
        Q_ASSERT(ok && m_regexp.isValid());
    }

    virtual bool doCheck(const Value &value)
    {
        bool ok;
        QString str = value.toString(&ok);
        if (!ok) {
            // According to spec (5.15) we should check the value only when it exist and it is a string.
            // It is a bit strange, but I think we have to return true here.
            return true;
        }
        return m_regexp.exactMatch(str);
    }
private:
    QRegExp m_regexp;
};

// 5.17
template<class T>
class SchemaPrivate<T>::CheckMinLength : public Check {
public:
    CheckMinLength(SchemaPrivate *schema, const Value& min)
        : Check(schema, "Minimal string length check failed for %1")
    {
        bool ok;
        m_min = min.toInt(&ok);
        Q_ASSERT(ok);
    }

    virtual bool doCheck(const Value &value)
    {
        bool ok;
        QString str = value.toString(&ok);
//        qDebug() << Q_FUNC_INFO << str << ok;
        if (!ok) {
            // According to spec (5.16) we should check the value only when it exist and it is a string.
            // It is a bit strange, but I think we have to return true here.
            return true;
        }
        return str.length() >= m_min;
    }
private:
    int m_min;
};

// 5.18
template<class T>
class SchemaPrivate<T>::CheckMaxLength : public Check {
public:
    CheckMaxLength(SchemaPrivate *schema, const Value& max)
        : Check(schema, "Maximal string length check failed for %1")
    {
        bool ok;
        m_max = max.toInt(&ok);
        Q_ASSERT(ok);
    }

    virtual bool doCheck(const Value &value)
    {
        bool ok;
        QString str = value.toString(&ok);
//        qDebug() << Q_FUNC_INFO << str << ok;
        if (!ok) {
            // According to spec (5.16) we should check the value only when it exist and it is a string.
            // It is a bit strange, but I think we have to return true here.
            return true;
        }
        return str.length() <= m_max;
    }
private:
    int m_max;
};

// 5.26
template<class T>
class SchemaPrivate<T>::CheckExtends : public Check {
public:
    CheckExtends(SchemaPrivate *schema, const Value &value)
        : Check(schema, "Extends check failed for %1")
    {
        // FIXME
        // Keep in mind that there is a bug in spec. (internet draft 3).
        // We should search for a schema not for a string here.
        // Tests are using "string" syntax, so we need to support it for a while
        bool ok;
        Object obj = value.toObject(&ok);
        if (!ok) {
            QString schemaName = value.toString(&ok);
            if (!ok) {
                ValueList array = value.toList(&ok);
                Q_ASSERT(ok);
                typename ValueList::const_iterator i;
                for (i = array.constBegin(); i != array.constEnd(); ++i) {
                    Object obj = (*i).toObject(&ok);
                    Q_ASSERT(ok);
                    m_extendedSchema.append(Schema<T>(obj, schema->m_callbacks));
                }
            } else {
                qWarning() << QString::fromLatin1("Wrong 'extends' syntax found, instead of \"%1\" should be \"%2\"")
                              .arg(schemaName, QString::fromLatin1("{\"$ref\":\"%1\"}").arg(schemaName));
                m_extendedSchema.append(schema->m_callbacks->loadSchema(schemaName));
            }
        } else {
            m_extendedSchema.append(Schema<T>(obj, schema->m_callbacks));
        }
    }

    virtual bool doCheck(const Value &value)
    {
        for (int i = 0; i < m_extendedSchema.count(); ++i) {
            if (!m_extendedSchema[i].check(value, Check::m_schema->m_callbacks))
                return false;
        }
        return true;
    }
private:
    QVarLengthArray<Schema<T>, 4> m_extendedSchema;
};

// 5.28
template<class T>
class SchemaPrivate<T>::CheckRef : public Check {
public:
    CheckRef(SchemaPrivate *schema, const Value &value)
        : Check(schema, "$Ref check failed for %1")
    {
        // TODO according to spec we should replace existing check by this one
        // I'm not sure what does it mean. Should we remove other checks?
        // What if we have two $ref? Can it happen? For now, lets use magic of
        // undefined bahaviour (without crashing of course).
        bool ok;
        QString schemaName = value.toString(&ok);
        Q_ASSERT(ok);

        m_newSchema = schema->m_callbacks->loadSchema(schemaName);
        if (!m_newSchema.isValid()) {
            // FIXME should we have current schema name?
            const QString msg =  QString::fromLatin1("Schema extends %1 but it is unknown.")
                    .arg(schemaName);
            qWarning() << msg;
            schema->m_callbacks->setError(msg);
        }
    }
    virtual bool doCheck(const Value &value)
    {
        bool result = m_newSchema.check(value, Check::m_schema->m_callbacks);
//        qDebug() << Q_FUNC_INFO << result;
        return result;
    }
private:
    Schema<T> m_newSchema;
};

template<class T>
class SchemaPrivate<T>::CheckDescription : public NullCheck {
public:
    CheckDescription(SchemaPrivate *schema)
        : NullCheck(schema)
    {}
};

template<class T>
class SchemaPrivate<T>::CheckTitle : public NullCheck {
public:
    CheckTitle(SchemaPrivate *schema)
        : NullCheck(schema)
    {}
};

template<class T>
typename SchemaPrivate<T>::Check *SchemaPrivate<T>::createCheckPoint(const Key &key, const Value &value)
{
    QString keyName = key;
    keyName = keyName.toLower();

    // This is a perfect hash. BUT spec, in future, can be enriched by new values, that we should just ignore.
    // As we do not know about them we can't be sure that our perfect hash will be still perfect, therefore
    // we have to do additional string comparison that confirm result hash function result.
    int hash = QStaticStringHash<>::hash(keyName);
    switch (hash) {
    case QStaticStringHash<'r','e','q','u','i','r','e','d'>::Hash:
        if (QString::fromLatin1("required") == keyName)
            return new CheckRequired(this, value);
        break;
    case QStaticStringHash<'m','a','x','i','m','u','m'>::Hash:
        if (QString::fromLatin1("maximum") == keyName)
            return new CheckMaximum(this, value);
        break;
    case QStaticStringHash<'e','x','c','l','u','s','i','v','e','m','a','x','i','m','u','m'>::Hash:
        if (QString::fromLatin1("exclusivemaximum") == keyName)
            return new CheckExclusiveMaximum(this, value);
        break;
    case QStaticStringHash<'m','i','n','i','m','u','m'>::Hash:
        if (QString::fromLatin1("minimum") == keyName)
            return new CheckMinimum(this, value);
        break;
    case QStaticStringHash<'e','x','c','l','u','s','i','v','e','m','i','n','i','m','u','m'>::Hash:
        if (QString::fromLatin1("exclusiveminimum") == keyName)
            return new CheckExclusiveMinimum(this, value);
        break;
    case QStaticStringHash<'p','r','o','p','e','r','t','i','e','s'>::Hash:
        if (QString::fromLatin1("properties") == keyName)
            return new CheckProperties(this, value);
        break;
    case QStaticStringHash<'d','e','s','c','r','i','p','t','i','o','n'>::Hash:
        if (QString::fromLatin1("description") == keyName)
            return new CheckDescription(this);
        break;
    case QStaticStringHash<'t','i','t','l','e'>::Hash:
        if (QString::fromLatin1("title") == keyName)
            return new CheckTitle(this);
        break;
    case QStaticStringHash<'m','a','x','i','t','e','m','s'>::Hash:
        if (QString::fromLatin1("maxitems") == keyName)
            return new CheckMaxItems(this,value);
        break;
    case QStaticStringHash<'m','i','n','i','t','e','m','s'>::Hash:
        if (QString::fromLatin1("minitems") == keyName)
            return new CheckMinItems(this,value);
        break;
    case QStaticStringHash<'i','t','e','m','s'>::Hash:
        if (QString::fromLatin1("items") == keyName)
            return new CheckItems(this,value);
        break;
    case QStaticStringHash<'e','x','t','e','n','d','s'>::Hash:
        if (QString::fromLatin1("extends") == keyName)
            return new CheckExtends(this,value);
        break;
    case QStaticStringHash<'p','a','t','t','e','r','n'>::Hash:
        if (QString::fromLatin1("pattern") == keyName)
            return new CheckPattern(this, value);
        break;
    case QStaticStringHash<'m','i','n','l','e','n','g','t','h'>::Hash:
        if (QString::fromLatin1("minlength") == keyName)
            return new CheckMinLength(this, value);
        break;
    case QStaticStringHash<'m','a','x','l','e','n','g','t','h'>::Hash:
        if (QString::fromLatin1("maxlength") == keyName)
            return new CheckMaxLength(this, value);
        break;
    case QStaticStringHash<'$','r','e','f'>::Hash:
        if (QString::fromLatin1("$ref") == keyName)
            return new CheckRef(this, value);
        break;
    case QStaticStringHash<'t','y','p','e'>::Hash:
        if (QString::fromLatin1("type") == keyName)
            return new CheckType(this, value);
        break;
    default:
//        qDebug() << "NOT FOUND"  << keyName;
        return new NullCheck(this);
    }

//    qDebug() << "FALLBACK"  << keyName;
//    bool  ok;
//    qCritical() << keyName << value.toString(&ok);
    return new NullCheck(this);
}

template<class T>
bool Schema<T>::check(const Value &value, Service *callbackToUseForCheck) const
{
    return d_ptr->check(value, callbackToUseForCheck);
}

template<class T>
bool SchemaPrivate<T>::check(const Value &value, Service *callbackToUseForCheck) const
{
    //qDebug() << Q_FUNC_INFO << m_checks.count() << this;
    Q_ASSERT(callbackToUseForCheck);
    Q_ASSERT(!m_callbacks);

    m_callbacks = callbackToUseForCheck;
    bool result = check(value);
    m_callbacks = 0;
    return result;
}

template<class T>
bool SchemaPrivate<T>::check(const Value &value) const
{
    Q_ASSERT(m_callbacks);

    m_requiredCount = 0;
    foreach (Check *check, m_checks) {
        if (!check->check(value)) {
            return false;
        }
    }
    if (m_requiredCount != m_maxRequired) {
        m_callbacks->setError(QString::fromLatin1("Schema validation error: Required field is missing"));
        return false;
    }
    return true;
}

} // namespace SchemaValidation

QT_END_HEADER

#endif // CHECKPOINTS_H
