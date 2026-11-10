// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#include "credentialvault.h"

#include <QSettings>
#include <QDir>
#include <QFile>
#include <QByteArray>

#include <openssl/evp.h>
#include <openssl/sha.h>
#include <openssl/rand.h>

#if defined(Q_OS_MACOS)
#include <IOKit/IOKitLib.h>
#include <CoreFoundation/CoreFoundation.h>
#elif defined(Q_OS_WIN)
#include <windows.h>
#endif

static QByteArray getMachineKey()
{
    QByteArray entropy;
#if defined(Q_OS_MACOS)
    io_service_t platformExpert = IOServiceGetMatchingService(0, IOServiceMatching("IOPlatformExpertDevice"));
    if (platformExpert) {
        CFTypeRef uuidRef = IORegistryEntryCreateCFProperty(platformExpert, CFSTR(kIOPlatformUUIDKey), kCFAllocatorDefault, 0);
        if (uuidRef) {
            char uuidBuf[128] = {0};
            if (CFStringGetCString((CFStringRef)uuidRef, uuidBuf, sizeof(uuidBuf), kCFStringEncodingUTF8)) {
                entropy.append(uuidBuf);
            }
            CFRelease(uuidRef);
        }
        IOObjectRelease(platformExpert);
    }
#elif defined(Q_OS_WIN)
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Cryptography", 0, KEY_READ | KEY_WOW64_64KEY, &hKey) == ERROR_SUCCESS) {
        wchar_t guid[128] = {0};
        DWORD size = sizeof(guid);
        if (RegQueryValueExW(hKey, L"MachineGuid", NULL, NULL, (LPBYTE)guid, &size) == ERROR_SUCCESS) {
            entropy.append(QString::fromWCharArray(guid).toUtf8());
        }
        RegCloseKey(hKey);
    }
#else
    QFile f1(QStringLiteral("/etc/machine-id"));
    if (f1.open(QIODevice::ReadOnly)) {
        entropy.append(f1.readAll().trimmed());
        f1.close();
    }
    if (entropy.isEmpty()) {
        QFile f2(QStringLiteral("/var/lib/dbus/machine-id"));
        if (f2.open(QIODevice::ReadOnly)) {
            entropy.append(f2.readAll().trimmed());
            f2.close();
        }
    }
#endif

    entropy.append(QDir::homePath().toUtf8());
    entropy.append("AstraPlay-Credential-Vault-v1");

    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((const unsigned char *)entropy.constData(), entropy.size(), hash);
    return QByteArray((const char *)hash, SHA256_DIGEST_LENGTH);
}

bool CredentialVault::writeSecretBytes(const QString &key, const QByteArray &secret)
{
    QSettings settings;
    if (secret.isEmpty()) {
        settings.remove("vault/" + key);
        return true;
    }

    QByteArray keyData = getMachineKey();
    unsigned char iv[16];
    if (RAND_bytes(iv, sizeof(iv)) != 1)
        return false;

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
        return false;

    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL,
                           (const unsigned char *)keyData.constData(), iv) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }

    QByteArray cipher(secret.size() + 16, 0);
    int outLen1 = 0;
    if (EVP_EncryptUpdate(ctx, (unsigned char *)cipher.data(), &outLen1,
                          (const unsigned char *)secret.constData(), secret.size()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }

    int outLen2 = 0;
    if (EVP_EncryptFinal_ex(ctx, (unsigned char *)cipher.data() + outLen1, &outLen2) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    EVP_CIPHER_CTX_free(ctx);

    cipher.resize(outLen1 + outLen2);

    QByteArray payload;
    payload.append((const char *)iv, sizeof(iv));
    payload.append(cipher);

    settings.setValue("vault/" + key, QString::fromLatin1(payload.toBase64()));
    return true;
}

QByteArray CredentialVault::readSecretBytes(const QString &key, const QByteArray &legacyFallback)
{
    QSettings settings;
    QString base64 = settings.value("vault/" + key).toString();
    if (!base64.isEmpty()) {
        QByteArray payload = QByteArray::fromBase64(base64.toLatin1());
        if (payload.size() > 16) {
            QByteArray iv = payload.left(16);
            QByteArray cipher = payload.mid(16);
            QByteArray keyData = getMachineKey();

            EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
            if (ctx) {
                if (EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL,
                                       (const unsigned char *)keyData.constData(),
                                       (const unsigned char *)iv.constData()) == 1) {
                    QByteArray plain(cipher.size() + 16, 0);
                    int outLen1 = 0;
                    if (EVP_DecryptUpdate(ctx, (unsigned char *)plain.data(), &outLen1,
                                          (const unsigned char *)cipher.constData(), cipher.size()) == 1) {
                        int outLen2 = 0;
                        if (EVP_DecryptFinal_ex(ctx, (unsigned char *)plain.data() + outLen1, &outLen2) == 1) {
                            plain.resize(outLen1 + outLen2);
                            EVP_CIPHER_CTX_free(ctx);
                            return plain;
                        }
                    }
                }
                EVP_CIPHER_CTX_free(ctx);
            }
        }
    }

    if (!legacyFallback.isEmpty()) {
        writeSecretBytes(key, legacyFallback);
        return legacyFallback;
    }

    return QByteArray();
}

bool CredentialVault::writeSecret(const QString &key, const QString &secret)
{
    return writeSecretBytes(key, secret.toUtf8());
}

QString CredentialVault::readSecret(const QString &key, const QString &legacyFallback)
{
    QByteArray bytes = readSecretBytes(key, legacyFallback.toUtf8());
    return QString::fromUtf8(bytes);
}

bool CredentialVault::deleteSecret(const QString &key)
{
    QSettings settings;
    settings.remove("vault/" + key);
    return true;
}
