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

#ifndef JSONDB_SETTINGS_H
#define JSONDB_SETTINGS_H

#include "jsondbpartitionglobal.h"

#include <QObject>
#include <QStringList>

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE_JSONDB_PARTITION

#define jsondbSettings JsonDbSettings::instance()

class Q_JSONDB_PARTITION_EXPORT JsonDbSettings : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool rejectStaleUpdates READ rejectStaleUpdates WRITE setRejectStaleUpdates)
    Q_PROPERTY(bool debug READ debug WRITE setDebug)
    Q_PROPERTY(bool debugIndexes READ debugIndexes WRITE setDebugIndexes)
    Q_PROPERTY(bool verbose READ verbose WRITE setVerbose)
    Q_PROPERTY(bool verboseErrors READ verboseErrors WRITE setVerboseErrors)
    Q_PROPERTY(bool performanceLog READ performanceLog WRITE setPerformanceLog)
    Q_PROPERTY(int cacheSize READ cacheSize WRITE setCacheSize)
    Q_PROPERTY(int compactRate READ compactRate WRITE setCompactRate)
    Q_PROPERTY(bool enforceAccessControl READ enforceAccessControl WRITE setEnforceAccessControl)
    Q_PROPERTY(int transactionSize READ transactionSize WRITE setTransactionSize)
    Q_PROPERTY(bool validateSchemas READ validateSchemas WRITE setValidateSchemas)
    Q_PROPERTY(bool softValidation READ softValidation WRITE setSoftValidation)
    Q_PROPERTY(int syncInterval READ syncInterval WRITE setSyncInterval)
    Q_PROPERTY(int indexSyncInterval READ indexSyncInterval WRITE setIndexSyncInterval)
    Q_PROPERTY(bool debugQuery READ debugQuery WRITE setDebugQuery)
    Q_PROPERTY(QStringList configSearchPath READ configSearchPath WRITE setConfigSearchPath)
    Q_PROPERTY(int indexFieldValueSize READ indexFieldValueSize WRITE setIndexFieldValueSize)
    Q_PROPERTY(int minimumRequiredSpace READ minimumRequiredSpace WRITE setMinimumRequiredSpace)
    Q_PROPERTY(int changeLogCacheVersions READ changeLogCacheVersions WRITE setChangeLogCacheVersions)
    Q_PROPERTY(bool useStrictMode READ useStrictMode WRITE setUseStrictMode)
    Q_PROPERTY(QString injectionScript READ injectionScript WRITE setInjectionScript)
    Q_PROPERTY(int offsetCacheSize READ offsetCacheSize WRITE setOffsetCacheSize)
    Q_PROPERTY(int maxQueriesInOffsetCache READ maxQueriesInOffsetCache WRITE setMaxQueriesInOffsetCache)

public:
    static JsonDbSettings *instance();

    inline void reload() { loadEnvironment(); }

    inline bool rejectStaleUpdates() const { return mRejectStaleUpdates; }
    inline void setRejectStaleUpdates(bool reject) { mRejectStaleUpdates = reject; }

    inline bool debug() const { return mDebug; }
    inline void setDebug(bool debug) { mDebug = debug; }

    inline bool debugIndexes() const { return mDebugIndexes; }
    inline void setDebugIndexes(bool debugIndexes) { mDebugIndexes = debugIndexes; }

    inline bool verbose() const { return mVerbose; }
    inline void setVerbose(bool verbose) { mVerbose = verbose; }

    inline bool verboseErrors() const { return mVerboseErrors; }
    inline void setVerboseErrors(bool verboseErrors) { mVerboseErrors = verboseErrors; }

    inline bool performanceLog() const { return mPerformanceLog; }
    inline void setPerformanceLog(bool performanceLog) { mPerformanceLog = performanceLog; }

    inline int cacheSize() const { return mCacheSize; }
    inline void setCacheSize(int cacheSize) { mCacheSize = cacheSize; }

    inline int compactRate() const { return mCompactRate; }
    inline void setCompactRate(int compactRate) { mCompactRate = compactRate; }

    inline bool enforceAccessControl() const { return mEnforceAccessControl; }
    inline void setEnforceAccessControl(bool enforce) { mEnforceAccessControl = enforce; }

    inline int transactionSize() const { return mTransactionSize; }
    inline void setTransactionSize(int transactionSize) { mTransactionSize = transactionSize; }

    inline bool validateSchemas() const { return mValidateSchemas; }
    inline void setValidateSchemas(bool validate) { mValidateSchemas = validate; }

    inline bool softValidation() const { return mSoftValidation; }
    inline void setSoftValidation(bool soft) { mSoftValidation = soft; }

    inline int syncInterval() const { return mSyncInterval; }
    inline void setSyncInterval(int interval) { mSyncInterval = interval; }

    inline int indexSyncInterval() const { return mIndexSyncInterval; }
    inline void setIndexSyncInterval(int interval) { mIndexSyncInterval = interval; }

    inline bool debugQuery() const { return mDebugQuery; }
    inline void setDebugQuery(bool debug) { mDebugQuery = debug; }

    inline QStringList configSearchPath() const { return mConfigSearchPath; }
    inline void setConfigSearchPath(const QStringList &searchPath) { mConfigSearchPath = searchPath; }

    inline int indexFieldValueSize() const { return mIndexFieldValueSize; }
    // >= 76 because of map/reduce UUIDs which are 76 bytes (38 wide characters)
    inline void setIndexFieldValueSize(int size) { Q_ASSERT((size % 2) == 0 && size >= 76); mIndexFieldValueSize = size; }

    inline int minimumRequiredSpace() const { return mMinimumRequiredSpace; }
    inline void setMinimumRequiredSpace(int space) { mMinimumRequiredSpace = space; }

    inline int changeLogCacheVersions() const { return mChangeLogCacheVersions; }
    inline void setChangeLogCacheVersions(int versions) { mChangeLogCacheVersions = versions; }

    inline bool useStrictMode() const { return mUseStrictMode; }
    inline void setUseStrictMode(bool useStrictMode) { mUseStrictMode = useStrictMode; }

    inline QString injectionScript() const { return mInjectionScript; }
    inline void setInjectionScript(const QString &script) { mInjectionScript = script; }

    inline int offsetCacheSize() const { return mOffsetCacheSize; }
    inline void setOffsetCacheSize(int size) { mOffsetCacheSize = size; }

    inline int maxQueriesInOffsetCache() const { return mMaxQueriesInOffsetCache; }
    inline void setMaxQueriesInOffsetCache(int size) { mMaxQueriesInOffsetCache = size; }

    JsonDbSettings();

private:
    void loadEnvironment();

    bool mRejectStaleUpdates;
    bool mDebug;
    bool mDebugIndexes;
    bool mVerbose;
    bool mVerboseErrors;
    bool mPerformanceLog;
    int mCacheSize;
    int mCompactRate;
    bool mEnforceAccessControl;
    int mTransactionSize;
    bool mValidateSchemas;
    bool mSoftValidation;
    int mSyncInterval;
    int mIndexSyncInterval;
    bool mDebugQuery;
    QStringList mConfigSearchPath;
    int mIndexFieldValueSize;
    int mMinimumRequiredSpace;
    int mChangeLogCacheVersions;
    bool mUseStrictMode;
    QString mInjectionScript;
    int mOffsetCacheSize;
    int mMaxQueriesInOffsetCache;
};

QT_END_NAMESPACE_JSONDB_PARTITION

QT_END_HEADER

#endif // JSONDB_SETTINGS_H
