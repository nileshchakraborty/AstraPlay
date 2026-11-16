// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#include <QtTest/QtTest>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>

#include "xboxpopsigner.h"
#include "xboxauthmanager.h"
#include "credentialvault.h"

#include <openssl/evp.h>
#include <openssl/ecdsa.h>
#include <openssl/bn.h>
#include <openssl/pem.h>

class XboxAuthTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void testPoPKeygenAndProofKey();
    void testPoPSignatureFormat();
    void testPoPSignatureVerification();
    void testPoPPersistenceInVault();
    void testXboxAuthManagerInitialState();
};

void XboxAuthTest::initTestCase()
{
    // Clean any test keys from vault
    CredentialVault::deleteSecret(QStringLiteral("test_xbox_pop_key"));
}

void XboxAuthTest::cleanupTestCase()
{
    CredentialVault::deleteSecret(QStringLiteral("test_xbox_pop_key"));
}

void XboxAuthTest::testPoPKeygenAndProofKey()
{
    XboxPoPSigner signer;
    QVERIFY(signer.init(QStringLiteral("test_xbox_pop_key")));
    QVERIFY(signer.isValid());

    QVERIFY(!signer.getPublicKeyX().isEmpty());
    QVERIFY(!signer.getPublicKeyY().isEmpty());

    QJsonObject proofKey = signer.getProofKey();
    QCOMPARE(proofKey.value(QStringLiteral("crv")).toString(), QStringLiteral("P-256"));
    QCOMPARE(proofKey.value(QStringLiteral("alg")).toString(), QStringLiteral("ES256"));
    QCOMPARE(proofKey.value(QStringLiteral("kty")).toString(), QStringLiteral("EC"));
    QCOMPARE(proofKey.value(QStringLiteral("use")).toString(), QStringLiteral("sig"));
    QCOMPARE(proofKey.value(QStringLiteral("x")).toString(), signer.getPublicKeyX());
    QCOMPARE(proofKey.value(QStringLiteral("y")).toString(), signer.getPublicKeyY());
}

void XboxAuthTest::testPoPSignatureFormat()
{
    XboxPoPSigner signer;
    QVERIFY(signer.init(QStringLiteral("test_xbox_pop_key")));

    QString url = QStringLiteral("https://device.auth.xboxlive.com/device/authenticate");
    QString method = QStringLiteral("POST");
    QString auth = QString();
    QByteArray body = "{\"test\":123}";

    QString sigBase64 = signer.sign(method, url, auth, body);
    QVERIFY(!sigBase64.isEmpty());

    QByteArray rawHeader = QByteArray::fromBase64(sigBase64.toLatin1());
    // Header format: 4 bytes version + 8 bytes NT timestamp + 64 bytes IEEE P1363 sig = 76 bytes
    QCOMPARE(rawHeader.size(), 76);

    // Verify version is 1 (big-endian)
    const uint8_t *p = reinterpret_cast<const uint8_t*>(rawHeader.constData());
    uint32_t version = (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | uint32_t(p[3]);
    QCOMPARE(version, 1u);

    // Verify NT timestamp is reasonable (after year 2026: ~133,000,000,000,000,000 in 100ns units)
    uint64_t nt_ts = 0;
    for (int i = 0; i < 8; ++i) {
        nt_ts = (nt_ts << 8) | p[4 + i];
    }
    uint64_t min_2026_ts = (1767225600ULL + 11644473600ULL) * 10000000ULL;
    QVERIFY(nt_ts >= min_2026_ts);
}

void XboxAuthTest::testPoPSignatureVerification()
{
    XboxPoPSigner signer;
    QVERIFY(signer.init(QStringLiteral("test_xbox_pop_key")));

    QString url = QStringLiteral("https://device.auth.xboxlive.com/device/authenticate");
    QString method = QStringLiteral("POST");
    QString auth = QString();
    QByteArray body = "{\"AuthMethod\":\"ProofOfPossession\"}";

    QString sigBase64 = signer.sign(method, url, auth, body);
    QByteArray rawHeader = QByteArray::fromBase64(sigBase64.toLatin1());
    QCOMPARE(rawHeader.size(), 76);

    const uint8_t *p = reinterpret_cast<const uint8_t*>(rawHeader.constData());
    uint64_t nt_ts = 0;
    for (int i = 0; i < 8; ++i) {
        nt_ts = (nt_ts << 8) | p[4 + i];
    }

    // Reconstruct the exact signed buffer
    QUrl qurl(url);
    QString pathAndQuery = qurl.path();
    QByteArray methodBytes = method.toUtf8();
    QByteArray pathBytes = pathAndQuery.toUtf8();
    QByteArray authBytes = auth.toUtf8();

    QByteArray signBuf;
    uint32_t v = qToBigEndian<uint32_t>(1);
    signBuf.append(reinterpret_cast<const char*>(&v), 4);
    signBuf.append('\0');

    uint64_t ts = qToBigEndian<uint64_t>(nt_ts);
    signBuf.append(reinterpret_cast<const char*>(&ts), 8);
    signBuf.append('\0');

    signBuf.append(methodBytes);
    signBuf.append('\0');
    signBuf.append(pathBytes);
    signBuf.append('\0');
    signBuf.append(authBytes);
    signBuf.append('\0');
    signBuf.append(body);
    signBuf.append('\0');

    // Extract raw 64-byte signature r || s
    const unsigned char *rawSig = p + 12;
    BIGNUM *r = BN_bin2bn(rawSig, 32, nullptr);
    BIGNUM *s = BN_bin2bn(rawSig + 32, 32, nullptr);
    ECDSA_SIG *ec_sig = ECDSA_SIG_new();
    ECDSA_SIG_set0(ec_sig, r, s);

    int derLen = i2d_ECDSA_SIG(ec_sig, nullptr);
    QByteArray derSig(derLen, 0);
    unsigned char *derP = reinterpret_cast<unsigned char*>(derSig.data());
    i2d_ECDSA_SIG(ec_sig, &derP);
    ECDSA_SIG_free(ec_sig);

    // Verify with OpenSSL public key from PEM
    QByteArray pem = signer.exportPem();
    BIO *bio = BIO_new_mem_buf(pem.constData(), pem.size());
    EVP_PKEY *pkey = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);
    QVERIFY(pkey != nullptr);

    EVP_MD_CTX *vctx = EVP_MD_CTX_new();
    QCOMPARE(EVP_DigestVerifyInit(vctx, nullptr, EVP_sha256(), nullptr, pkey), 1);
    QCOMPARE(EVP_DigestVerifyUpdate(vctx, signBuf.constData(), signBuf.size()), 1);
    int verified = EVP_DigestVerifyFinal(vctx, reinterpret_cast<const unsigned char*>(derSig.constData()), derSig.size());
    EVP_MD_CTX_free(vctx);
    EVP_PKEY_free(pkey);

    QCOMPARE(verified, 1);
}

void XboxAuthTest::testPoPPersistenceInVault()
{
    QString testKey = QStringLiteral("test_vault_persistence_key");
    CredentialVault::deleteSecret(testKey);

    QString x1, y1;
    {
        XboxPoPSigner signer1;
        QVERIFY(signer1.init(testKey));
        x1 = signer1.getPublicKeyX();
        y1 = signer1.getPublicKeyY();
        QVERIFY(!x1.isEmpty());
    }

    {
        XboxPoPSigner signer2;
        // Should reload existing key from vault instead of generating a new one
        QVERIFY(signer2.init(testKey));
        QCOMPARE(signer2.getPublicKeyX(), x1);
        QCOMPARE(signer2.getPublicKeyY(), y1);
    }

    CredentialVault::deleteSecret(testKey);
}

void XboxAuthTest::testXboxAuthManagerInitialState()
{
    XboxAuthManager *auth = XboxAuthManager::instance();
    QVERIFY(auth != nullptr);
    QVERIFY(!auth->isLoggedIn());
    QVERIFY(auth->getGamertag().isEmpty());
    QVERIFY(auth->getUserHash().isEmpty());
    QVERIFY(auth->getConsolesJson().isEmpty());
}

QTEST_MAIN(XboxAuthTest)
#include "xboxauth_test.moc"
