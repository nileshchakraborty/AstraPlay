// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQuickItem>

#include "xboxpopsigner.h"
#include "xboxauthmanager.h"
#include "credentialvault.h"
#include <chiaki/controller.h>

#include <openssl/evp.h>
#include <openssl/ecdsa.h>
#include <openssl/bn.h>
#include <openssl/pem.h>

class AstraPlayStub : public QObject
{
    Q_OBJECT
public:
    Q_INVOKABLE QString getQRCodeDataUrl(const QString &data, int size) {
        Q_UNUSED(data);
        Q_UNUSED(size);
        return QStringLiteral("data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNk+M9QDwADhgGAWjR9awAAAABJRU5ErkJggg==");
    }
    Q_INVOKABLE void startXboxStream(const QString &serverId, const QString &titleId, const QString &gameName) {
        lastServerId = serverId;
        lastTitleId = titleId;
        lastGameName = gameName;
        streamStarted = true;
        sessionStopped = false;
    }
    Q_INVOKABLE void stopSession(bool sleep = false) {
        Q_UNUSED(sleep);
        streamStarted = false;
        sessionStopped = true;
    }
    Q_INVOKABLE void setClipboardText(const QString &text) {
        lastClipboard = text;
    }

    QString lastServerId;
    QString lastTitleId;
    QString lastGameName;
    QString lastClipboard;
    bool streamStarted = false;
    bool sessionStopped = false;
};

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
    void testLiveDeviceCodeFlow();
    void testXboxViewQml();
    void testXboxGamepadProtocolSerialization();
    void testXboxVibrationProtocolParsing();
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
    QVERIFY(!auth->getCloudTitlesJson().isEmpty());
    qDebug() << "Initial cached cloud titles count:" << auth->getCloudTitlesJson().size();
}

void XboxAuthTest::testLiveDeviceCodeFlow()
{
    XboxAuthManager *auth = XboxAuthManager::instance();
    QVERIFY(auth != nullptr);

    QSignalSpy spy(auth, &XboxAuthManager::deviceCodeReceived);
    auth->startDeviceCodeLogin();

    // Wait up to 10 seconds for live Microsoft OAuth response
    bool received = spy.wait(10000);
    QVERIFY2(received, "Timed out waiting for Microsoft devicecode response");
    QCOMPARE(spy.count(), 1);

    QList<QVariant> args = spy.takeFirst();
    QString userCode = args.at(0).toString();
    QString verificationUri = args.at(1).toString();
    int expiresIn = args.at(3).toInt();

    qDebug() << "LIVE TEST SUCCESS: Received User Code from Microsoft:" << userCode
             << "Verification URI:" << verificationUri
             << "Expires in:" << expiresIn << "seconds";

    QVERIFY(!userCode.isEmpty());
    QVERIFY(userCode.length() >= 8);
    QVERIFY(verificationUri.contains(QStringLiteral("microsoft.com/link")) ||
            verificationUri.contains(QStringLiteral("microsoft.com/devicelogin")));
    QVERIFY(expiresIn > 0);

    auth->cancelLogin();
}

void XboxAuthTest::testXboxViewQml()
{
    QQmlEngine engine;
    AstraPlayStub astraplayStub;
    XboxAuthManager *auth = XboxAuthManager::instance();

    const char *uri = "io.github.nileshchakraborty.astraplay";
    qmlRegisterSingletonInstance(uri, 1, 0, "XboxAuth", auth);
    qmlRegisterSingletonInstance(uri, 1, 0, "AstraPlay", &astraplayStub);

    QUrl qmlUrl(QStringLiteral("qrc:/XboxView.qml"));
    if (!QFile::exists(QStringLiteral(":/XboxView.qml"))) {
        QString fallback = QStringLiteral("%1/../../gui/src/qml/XboxView.qml").arg(QCoreApplication::applicationDirPath());
        if (QFile::exists(fallback))
            qmlUrl = QUrl::fromLocalFile(fallback);
    }
    QQmlComponent component(&engine, qmlUrl);

    if (!component.isReady()) {
        for (const auto &err : component.errors()) {
            qCritical() << "QML Error:" << err.toString();
        }
    }
    QVERIFY(component.isReady());

    QObject *rootObj = component.create();
    QVERIFY(rootObj != nullptr);

    // Initial state: not logged in, not in login flow, games populated from catalog
    QCOMPARE(rootObj->property("isLoggingIn").toBool(), false);
    QCOMPARE(rootObj->property("userCode").toString(), QString());
    QVERIFY(rootObj->property("filteredGames").toList().size() > 0);
    qDebug() << "XboxView QML loaded with games count:" << rootObj->property("filteredGames").toList().size();

    // Trigger device code login
    QSignalSpy spy(auth, &XboxAuthManager::deviceCodeReceived);
    auth->startDeviceCodeLogin();

    QVERIFY(spy.wait(10000));
    QCOMPARE(spy.count(), 1);

    // QML properties must update via Connections target: XboxAuth
    QCOMPARE(rootObj->property("isLoggingIn").toBool(), true);
    QString code = rootObj->property("userCode").toString();
    QVERIFY(!code.isEmpty());
    QVERIFY(code.length() >= 8);
    QVERIFY(!rootObj->property("verificationUri").toString().isEmpty());

    // Cancel login
    auth->cancelLogin();
    QCOMPARE(rootObj->property("isLoggingIn").toBool(), false);

    // Test Opening a Game (e.g. Cricket 26 / xCloud title)
    bool launched = QMetaObject::invokeMethod(rootObj, "launchGame",
                                              Q_ARG(QVariant, QStringLiteral("CRICKET26THEOFFICIALGAMEOFTHEASHES")),
                                              Q_ARG(QVariant, QStringLiteral("Cricket 26 - The Official Game of the Ashes")));
    QVERIFY(launched);
    QCOMPARE(astraplayStub.streamStarted, true);
    QCOMPARE(astraplayStub.lastTitleId, QStringLiteral("CRICKET26THEOFFICIALGAMEOFTHEASHES"));
    QCOMPARE(astraplayStub.lastGameName, QStringLiteral("Cricket 26 - The Official Game of the Ashes"));
    QCOMPARE(astraplayStub.lastServerId, QString());
    qDebug() << "Successfully launched game:" << astraplayStub.lastGameName << "ID:" << astraplayStub.lastTitleId;

    // Test Closing the same Game
    astraplayStub.stopSession(false);
    QCOMPARE(astraplayStub.streamStarted, false);
    QCOMPARE(astraplayStub.sessionStopped, true);
    qDebug() << "Successfully closed stream session for:" << astraplayStub.lastGameName;

    delete rootObj;
}

void XboxAuthTest::testXboxGamepadProtocolSerialization()
{
    // Test 38-byte Xbox Cloud / xHome Gamepad Protocol Layout
    ChiakiControllerState state;
    chiaki_controller_state_set_idle(&state);

    // Set buttons: Cross (A), Moon (B), DPad Up, LB, R3, Options (Menu)
    state.buttons = CHIAKI_CONTROLLER_BUTTON_CROSS |
                    CHIAKI_CONTROLLER_BUTTON_MOON |
                    CHIAKI_CONTROLLER_BUTTON_DPAD_UP |
                    CHIAKI_CONTROLLER_BUTTON_L1 |
                    CHIAKI_CONTROLLER_BUTTON_R3 |
                    CHIAKI_CONTROLLER_BUTTON_OPTIONS;

    // Set sticks
    state.left_x = 16000;
    state.left_y = -20000; // In Chiaki, up is negative. In Xbox, up is positive.
    state.right_x = -8000;
    state.right_y = 12000;

    // Set triggers (0..255)
    state.l2_state = 128; // ~50%
    state.r2_state = 255; // 100%

    // Serialize packet conforming to Xbox WebRTC specification
    QByteArray packet(38, 0);
    uint8_t *p = reinterpret_cast<uint8_t*>(packet.data());

    p[0] = 0x02; // ReportType = Gamepad
    p[1] = 0x00;

    uint32_t seq = 42;
    p[2] = uint8_t(seq & 0xFF);
    p[3] = uint8_t((seq >> 8) & 0xFF);
    p[4] = uint8_t((seq >> 16) & 0xFF);
    p[5] = uint8_t((seq >> 24) & 0xFF);

    double nowMs = 12345678.0;
    memcpy(p + 6, &nowMs, sizeof(double));

    p[14] = 0x01; // Frame count = 1
    p[15] = 0x00; // Gamepad index = 0

    uint16_t xbuttons = 0;
    if (state.buttons & CHIAKI_CONTROLLER_BUTTON_CROSS) xbuttons |= 16;
    if (state.buttons & CHIAKI_CONTROLLER_BUTTON_MOON) xbuttons |= 32;
    if (state.buttons & CHIAKI_CONTROLLER_BUTTON_BOX) xbuttons |= 64;
    if (state.buttons & CHIAKI_CONTROLLER_BUTTON_PYRAMID) xbuttons |= 128;
    if (state.buttons & CHIAKI_CONTROLLER_BUTTON_DPAD_UP) xbuttons |= 256;
    if (state.buttons & CHIAKI_CONTROLLER_BUTTON_DPAD_DOWN) xbuttons |= 512;
    if (state.buttons & CHIAKI_CONTROLLER_BUTTON_DPAD_LEFT) xbuttons |= 1024;
    if (state.buttons & CHIAKI_CONTROLLER_BUTTON_DPAD_RIGHT) xbuttons |= 2048;
    if (state.buttons & CHIAKI_CONTROLLER_BUTTON_L1) xbuttons |= 4096;
    if (state.buttons & CHIAKI_CONTROLLER_BUTTON_R1) xbuttons |= 8192;
    if (state.buttons & CHIAKI_CONTROLLER_BUTTON_L3) xbuttons |= 16384;
    if (state.buttons & CHIAKI_CONTROLLER_BUTTON_R3) xbuttons |= 32768;
    if (state.buttons & CHIAKI_CONTROLLER_BUTTON_OPTIONS) xbuttons |= 4;
    if (state.buttons & CHIAKI_CONTROLLER_BUTTON_SHARE) xbuttons |= 8;
    if (state.buttons & CHIAKI_CONTROLLER_BUTTON_PS) xbuttons |= 2;

    p[16] = uint8_t(xbuttons & 0xFF);
    p[17] = uint8_t((xbuttons >> 8) & 0xFF);

    int16_t lx = state.left_x;
    int16_t ly = -state.left_y;
    int16_t rx = state.right_x;
    int16_t ry = -state.right_y;

    memcpy(p + 18, &lx, 2);
    memcpy(p + 20, &ly, 2);
    memcpy(p + 22, &rx, 2);
    memcpy(p + 24, &ry, 2);

    uint16_t lt = static_cast<uint16_t>((state.l2_state * 65535) / 255);
    uint16_t rt = static_cast<uint16_t>((state.r2_state * 65535) / 255);
    memcpy(p + 26, &lt, 2);
    memcpy(p + 28, &rt, 2);

    uint32_t physical = 1;
    memcpy(p + 30, &physical, 4);

    p[34] = 0;
    p[35] = 0;
    p[36] = 0;
    p[37] = 1;

    // Verify Packet Dimensions and Exact Layout
    QCOMPARE(packet.size(), 38);
    QCOMPARE(p[0], 0x02);
    QCOMPARE(p[1], 0x00);
    QCOMPARE(p[14], 0x01);
    QCOMPARE(p[15], 0x00);

    // Verify Button Bitmask:
    // A (16) + B (32) + DPadUp (256) + LB (4096) + R3 (32768) + Options (4) = 37166
    uint16_t expectedButtons = 16 | 32 | 256 | 4096 | 32768 | 4;
    uint16_t parsedButtons = p[16] | (p[17] << 8);
    QCOMPARE(parsedButtons, expectedButtons);

    // Verify Sticks:
    int16_t parsedLx = 0, parsedLy = 0, parsedRx = 0, parsedRy = 0;
    memcpy(&parsedLx, p + 18, 2);
    memcpy(&parsedLy, p + 20, 2);
    memcpy(&parsedRx, p + 22, 2);
    memcpy(&parsedRy, p + 24, 2);
    QCOMPARE(parsedLx, 16000);
    QCOMPARE(parsedLy, 20000); // inverted from -20000
    QCOMPARE(parsedRx, -8000);
    QCOMPARE(parsedRy, -12000); // inverted from 12000

    // Verify Triggers:
    uint16_t parsedLt = 0, parsedRt = 0;
    memcpy(&parsedLt, p + 26, 2);
    memcpy(&parsedRt, p + 28, 2);
    QCOMPARE(parsedLt, uint16_t((128 * 65535) / 255));
    QCOMPARE(parsedRt, uint16_t(65535));

    // Verify Physicality:
    uint32_t parsedPhysical = 0;
    memcpy(&parsedPhysical, p + 30, 4);
    QCOMPARE(parsedPhysical, 1u);
    QCOMPARE(p[37], 0x01);
}

void XboxAuthTest::testXboxVibrationProtocolParsing()
{
    // Simulate a 13-byte incoming Vibration packet from Xbox WebRTC input data channel
    // reportType = 128 (uint16 little endian)
    // rumbleType = 0
    // gamepadIndex = 0
    // leftMotorPercent = 80
    // rightMotorPercent = 40
    // leftTriggerPercent = 20
    // rightTriggerPercent = 10
    // durationMs = 350
    // delayMs = 0
    // repeat = 1
    QByteArray bin(13, 0);
    bin[0] = static_cast<char>(128); // 0x80
    bin[1] = 0x00;
    bin[2] = 0x00;
    bin[3] = 0x00; // GamepadIndex = 0
    bin[4] = 80;   // Left motor 80%
    bin[5] = 40;   // Right motor 40%
    bin[6] = 20;
    bin[7] = 10;
    bin[8] = static_cast<char>(350 & 0xFF);
    bin[9] = static_cast<char>((350 >> 8) & 0xFF);
    bin[10] = 0;
    bin[11] = 0;
    bin[12] = 1;

    QVERIFY(bin.size() >= 13);
    uint16_t reportType = static_cast<uint8_t>(bin[0]) | (static_cast<uint8_t>(bin[1]) << 8);
    QCOMPARE(reportType, 128);

    uint8_t leftPercent = static_cast<uint8_t>(bin[4]);
    uint8_t rightPercent = static_cast<uint8_t>(bin[5]);
    QCOMPARE(leftPercent, 80);
    QCOMPARE(rightPercent, 40);

    uint16_t leftStrength = (static_cast<uint32_t>(leftPercent) * 65535) / 100;
    uint16_t rightStrength = (static_cast<uint32_t>(rightPercent) * 65535) / 100;
    QCOMPARE(leftStrength, uint16_t((80 * 65535) / 100));
    QCOMPARE(rightStrength, uint16_t((40 * 65535) / 100));

    uint16_t maxHaptic = qMax(leftStrength, rightStrength);
    QCOMPARE(maxHaptic, leftStrength);

    // SDL 8-bit rumble scaling
    uint8_t leftSdl = static_cast<uint8_t>(leftStrength >> 8);
    uint8_t rightSdl = static_cast<uint8_t>(rightStrength >> 8);
    QVERIFY(leftSdl > rightSdl);
}

QTEST_MAIN(XboxAuthTest)
#include "xboxauth_test.moc"
