// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#include "xboxpopsigner.h"
#include "credentialvault.h"

#include <QDateTime>
#include <QUrl>
#include <QtEndian>
#include <QDebug>

#include <openssl/evp.h>
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/bn.h>
#include <openssl/pem.h>
#include <openssl/core_names.h>

XboxPoPSigner::XboxPoPSigner()
{
}

XboxPoPSigner::~XboxPoPSigner()
{
    if (m_pkey) {
        EVP_PKEY_free(m_pkey);
        m_pkey = nullptr;
    }
}

bool XboxPoPSigner::init(const QString &vaultKey)
{
    // 1. Try reading existing key from CredentialVault
    QByteArray savedPem = CredentialVault::readSecretBytes(vaultKey);
    if (!savedPem.isEmpty()) {
        if (loadFromPem(savedPem)) {
            return true;
        }
    }

    // 2. Generate new ECDSA P-256 key pair
    EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, nullptr);
    if (!pctx) return false;

    if (EVP_PKEY_keygen_init(pctx) <= 0 ||
        EVP_PKEY_CTX_set_ec_paramgen_curve_nid(pctx, NID_X9_62_prime256v1) <= 0) {
        EVP_PKEY_CTX_free(pctx);
        return false;
    }

    if (m_pkey) {
        EVP_PKEY_free(m_pkey);
        m_pkey = nullptr;
    }

    if (EVP_PKEY_keygen(pctx, &m_pkey) <= 0) {
        EVP_PKEY_CTX_free(pctx);
        return false;
    }
    EVP_PKEY_CTX_free(pctx);

    if (!extractPublicCoordinates()) {
        return false;
    }

    // 3. Persist new key in CredentialVault
    QByteArray pemData = exportPem();
    if (!pemData.isEmpty()) {
        CredentialVault::writeSecretBytes(vaultKey, pemData);
    }

    return true;
}

bool XboxPoPSigner::loadFromPem(const QByteArray &pemData)
{
    BIO *bio = BIO_new_mem_buf(pemData.constData(), pemData.size());
    if (!bio) return false;

    EVP_PKEY *loaded = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);

    if (!loaded) return false;

    if (m_pkey) {
        EVP_PKEY_free(m_pkey);
    }
    m_pkey = loaded;

    return extractPublicCoordinates();
}

QByteArray XboxPoPSigner::exportPem() const
{
    if (!m_pkey) return QByteArray();

    BIO *bio = BIO_new(BIO_s_mem());
    if (!bio) return QByteArray();

    if (PEM_write_bio_PKCS8PrivateKey(bio, m_pkey, nullptr, nullptr, 0, nullptr, nullptr) <= 0) {
        BIO_free(bio);
        return QByteArray();
    }

    char *data = nullptr;
    long len = BIO_get_mem_data(bio, &data);
    QByteArray result(data, len);
    BIO_free(bio);

    return result;
}

bool XboxPoPSigner::extractPublicCoordinates()
{
    if (!m_pkey) return false;

    BIGNUM *x = nullptr;
    BIGNUM *y = nullptr;

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
    if (!EVP_PKEY_get_bn_param(m_pkey, OSSL_PKEY_PARAM_EC_PUB_X, &x) ||
        !EVP_PKEY_get_bn_param(m_pkey, OSSL_PKEY_PARAM_EC_PUB_Y, &y)) {
        return false;
    }
#else
    const EC_KEY *eckey = EVP_PKEY_get0_EC_KEY(m_pkey);
    if (!eckey) return false;
    const EC_POINT *point = EC_KEY_get0_public_key(eckey);
    const EC_GROUP *group = EC_KEY_get0_group(eckey);
    if (!point || !group) return false;
    x = BN_new();
    y = BN_new();
    if (!EC_POINT_get_affine_coordinates(group, point, x, y, nullptr)) {
        BN_free(x);
        BN_free(y);
        return false;
    }
#endif

    unsigned char x_buf[32] = {0};
    unsigned char y_buf[32] = {0};
    BN_bn2binpad(x, x_buf, 32);
    BN_bn2binpad(y, y_buf, 32);
    BN_free(x);
    BN_free(y);

    m_pubX = QString::fromLatin1(QByteArray(reinterpret_cast<const char*>(x_buf), 32).toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
    m_pubY = QString::fromLatin1(QByteArray(reinterpret_cast<const char*>(y_buf), 32).toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));

    return !m_pubX.isEmpty() && !m_pubY.isEmpty();
}

QJsonObject XboxPoPSigner::getProofKey() const
{
    QJsonObject obj;
    obj[QStringLiteral("use")] = QStringLiteral("sig");
    obj[QStringLiteral("alg")] = QStringLiteral("ES256");
    obj[QStringLiteral("kty")] = QStringLiteral("EC");
    obj[QStringLiteral("crv")] = QStringLiteral("P-256");
    obj[QStringLiteral("x")] = m_pubX;
    obj[QStringLiteral("y")] = m_pubY;
    return obj;
}

QString XboxPoPSigner::sign(const QString &method, const QString &url, const QString &authToken, const QByteArray &payload) const
{
    if (!m_pkey) return QString();

    // 1. Windows NT 100ns timestamp
    // (unix_seconds + 11644473600ULL) * 10000000ULL
    uint64_t unix_sec = QDateTime::currentSecsSinceEpoch();
    uint64_t windowsTimestamp = (unix_sec + 11644473600ULL) * 10000000ULL;

    QUrl qurl(url);
    QString pathAndQuery = qurl.path();
    if (!qurl.query().isEmpty()) {
        pathAndQuery += QStringLiteral("?") + qurl.query();
    }

    QByteArray methodBytes = method.toUtf8();
    QByteArray pathBytes = pathAndQuery.toUtf8();
    QByteArray authBytes = authToken.toUtf8();

    // 2. Build input buffer to sign:
    // 4 bytes: version 1 (big endian)
    // 1 byte: 0x00
    // 8 bytes: windowsTimestamp (big endian)
    // 1 byte: 0x00
    // method + '\0'
    // pathAndQuery + '\0'
    // authBytes + '\0'
    // payload + '\0'
    QByteArray signBuf;
    signBuf.reserve(14 + methodBytes.size() + 1 + pathBytes.size() + 1 + authBytes.size() + 1 + payload.size() + 1);

    uint32_t v = qToBigEndian<uint32_t>(1);
    signBuf.append(reinterpret_cast<const char*>(&v), 4);
    signBuf.append('\0');

    uint64_t ts = qToBigEndian<uint64_t>(windowsTimestamp);
    signBuf.append(reinterpret_cast<const char*>(&ts), 8);
    signBuf.append('\0');

    signBuf.append(methodBytes);
    signBuf.append('\0');

    signBuf.append(pathBytes);
    signBuf.append('\0');

    signBuf.append(authBytes);
    signBuf.append('\0');

    signBuf.append(payload);
    signBuf.append('\0');

    // 3. Sign SHA256 with ECDSA
    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    if (!mdctx) return QString();

    if (EVP_DigestSignInit(mdctx, nullptr, EVP_sha256(), nullptr, m_pkey) <= 0) {
        EVP_MD_CTX_free(mdctx);
        return QString();
    }

    if (EVP_DigestSignUpdate(mdctx, signBuf.constData(), signBuf.size()) <= 0) {
        EVP_MD_CTX_free(mdctx);
        return QString();
    }

    size_t siglen = 0;
    if (EVP_DigestSignFinal(mdctx, nullptr, &siglen) <= 0) {
        EVP_MD_CTX_free(mdctx);
        return QString();
    }

    QByteArray derSig(siglen, 0);
    if (EVP_DigestSignFinal(mdctx, reinterpret_cast<unsigned char*>(derSig.data()), &siglen) <= 0) {
        EVP_MD_CTX_free(mdctx);
        return QString();
    }
    derSig.resize(siglen);
    EVP_MD_CTX_free(mdctx);

    // 4. Convert DER to IEEE P1363 (64 bytes raw r || s)
    const unsigned char *p = reinterpret_cast<const unsigned char*>(derSig.constData());
    ECDSA_SIG *ec_sig = d2i_ECDSA_SIG(nullptr, &p, derSig.size());
    if (!ec_sig) return QString();

    const BIGNUM *r = nullptr;
    const BIGNUM *s = nullptr;
    ECDSA_SIG_get0(ec_sig, &r, &s);
    unsigned char rawSig[64] = {0};
    BN_bn2binpad(r, rawSig, 32);
    BN_bn2binpad(s, rawSig + 32, 32);
    ECDSA_SIG_free(ec_sig);

    // 5. Header format (76 bytes):
    // 4 bytes: version 1 (big endian)
    // 8 bytes: windowsTimestamp (big endian)
    // 64 bytes: IEEE P1363 signature
    QByteArray headerBuf;
    headerBuf.reserve(76);
    headerBuf.append(reinterpret_cast<const char*>(&v), 4);
    headerBuf.append(reinterpret_cast<const char*>(&ts), 8);
    headerBuf.append(reinterpret_cast<const char*>(rawSig), 64);

    return QString::fromLatin1(headerBuf.toBase64());
}
