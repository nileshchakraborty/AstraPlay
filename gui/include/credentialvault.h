// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
#pragma once

#include <QString>
#include <QByteArray>

class CredentialVault
{
public:
    static bool writeSecret(const QString &key, const QString &secret);
    static QString readSecret(const QString &key, const QString &legacyFallback = QString());
    static bool deleteSecret(const QString &key);

    static bool writeSecretBytes(const QString &key, const QByteArray &secret);
    static QByteArray readSecretBytes(const QString &key, const QByteArray &legacyFallback = QByteArray());
};
