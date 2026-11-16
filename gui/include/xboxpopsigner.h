// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
#pragma once

#include <QString>
#include <QByteArray>
#include <QJsonObject>
#include <openssl/evp.h>

class XboxPoPSigner
{
public:
    XboxPoPSigner();
    ~XboxPoPSigner();

    // Disable copy
    XboxPoPSigner(const XboxPoPSigner &) = delete;
    XboxPoPSigner &operator=(const XboxPoPSigner &) = delete;

    // Generate a fresh key or initialize from existing PEM / CredentialVault
    bool init(const QString &vaultKey = QStringLiteral("xbox_pop_key"));
    bool loadFromPem(const QByteArray &pemData);
    QByteArray exportPem() const;

    // Export JWK ProofKey object for XBL auth requests
    QJsonObject getProofKey() const;

    // Generate PoP Signature header (76 bytes base64-encoded)
    // Windows NT 100ns timestamp, binary payload, IEEE P1363 (r || s) signature
    QString sign(const QString &method, const QString &url, const QString &authToken, const QByteArray &payload) const;

    // Get public key coordinates (base64url)
    QString getPublicKeyX() const { return m_pubX; }
    QString getPublicKeyY() const { return m_pubY; }

    bool isValid() const { return m_pkey != nullptr; }

private:
    EVP_PKEY *m_pkey = nullptr;
    QString m_pubX;
    QString m_pubY;

    bool extractPublicCoordinates();
};
