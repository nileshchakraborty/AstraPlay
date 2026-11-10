// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#include "credentialvault.h"

#include <QSettings>
#include <QtTest>

class CredentialVaultTest final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void testWriteAndReadSecret();
    void testCiphertextEncryptedAtRest();
    void testLegacyMigration();
    void testBinarySecretRoundtrip();
    void testDeleteSecret();
};

void CredentialVaultTest::initTestCase()
{
    QCoreApplication::setOrganizationName("AstraPlayTest");
    QCoreApplication::setApplicationName("AstraPlayVaultTest");
    QSettings settings;
    settings.clear();
}

void CredentialVaultTest::cleanupTestCase()
{
    QSettings settings;
    settings.clear();
}

void CredentialVaultTest::testWriteAndReadSecret()
{
    const QString key = QStringLiteral("test_npsso_key");
    const QString secret = QStringLiteral("v1.AB12CD34EF56GH78IJ90KLMNOPQRSTUV");

    QVERIFY(CredentialVault::writeSecret(key, secret));
    QString retrieved = CredentialVault::readSecret(key);
    QCOMPARE(retrieved, secret);
}

void CredentialVaultTest::testCiphertextEncryptedAtRest()
{
    const QString key = QStringLiteral("secret_at_rest_key");
    const QString secret = QStringLiteral("super_confidential_token_value_98765");

    QVERIFY(CredentialVault::writeSecret(key, secret));

    // Inspect underlying QSettings value directly
    QSettings settings;
    QString storedRaw = settings.value(QStringLiteral("vault/") + key).toString();
    QVERIFY(!storedRaw.isEmpty());

    // Stored raw data MUST NOT contain the plaintext secret
    QVERIFY(!storedRaw.contains(secret));

    // Stored raw data should be valid Base64
    QByteArray decoded = QByteArray::fromBase64(storedRaw.toLatin1());
    QVERIFY(decoded.size() > 16); // IV (16) + at least 1 encrypted block (16)
}

void CredentialVaultTest::testLegacyMigration()
{
    const QString legacyKey = QStringLiteral("settings/legacy_npsso_token");
    const QString vaultKey = QStringLiteral("psn_npsso_token_migrated");
    const QString legacyToken = QStringLiteral("legacy_token_12345_plaintext");

    QSettings settings;
    settings.setValue(legacyKey, legacyToken);

    // Call readSecret with legacyFallback
    QString migrated = CredentialVault::readSecret(vaultKey, settings.value(legacyKey).toString());
    QCOMPARE(migrated, legacyToken);

    // Verify it is now stored in the encrypted vault
    QString storedRaw = settings.value(QStringLiteral("vault/") + vaultKey).toString();
    QVERIFY(!storedRaw.isEmpty());
    QVERIFY(!storedRaw.contains(legacyToken));

    // Reading without legacy fallback returns the migrated value
    QString fromVault = CredentialVault::readSecret(vaultKey);
    QCOMPARE(fromVault, legacyToken);
}

void CredentialVaultTest::testBinarySecretRoundtrip()
{
    const QString key = QStringLiteral("rp_key_console_01");
    // 16-byte random binary key like PS4/PS5 rp_key
    QByteArray rawKey("\x12\x34\x56\x78\x9a\xbc\xde\xf0\x0f\xed\xcb\xa9\x87\x65\x43\x21", 16);

    QVERIFY(CredentialVault::writeSecretBytes(key, rawKey));
    QByteArray retrieved = CredentialVault::readSecretBytes(key);
    QCOMPARE(retrieved, rawKey);
}

void CredentialVaultTest::testDeleteSecret()
{
    const QString key = QStringLiteral("temporary_token");
    QVERIFY(CredentialVault::writeSecret(key, QStringLiteral("temp_val")));
    QCOMPARE(CredentialVault::readSecret(key), QStringLiteral("temp_val"));

    QVERIFY(CredentialVault::deleteSecret(key));
    QVERIFY(CredentialVault::readSecret(key).isEmpty());

    QSettings settings;
    QVERIFY(!settings.contains(QStringLiteral("vault/") + key));
}

QTEST_GUILESS_MAIN(CredentialVaultTest)
#include "credentialvault_test.moc"
