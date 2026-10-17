#include "updatemanager.h"
#include "settings.h"

#include <QTemporaryFile>
#include <QTemporaryDir>
#include <QtTest>

class UpdateManagerTest final : public QObject
{
    Q_OBJECT

private slots:
    void testIsVersionNewer();
    void testVerifyNonExistentFile();
    void testVerifySmallFile();
    void testVerifyHtmlPayload();
    void testVerifyValidPayload();
};

void UpdateManagerTest::testIsVersionNewer()
{
    // Patch upgrade
    QVERIFY(UpdateManager::isVersionNewer(QStringLiteral("2.12.12"), QStringLiteral("2.12.11")));
    // Same version
    QVERIFY(!UpdateManager::isVersionNewer(QStringLiteral("2.12.11"), QStringLiteral("2.12.11")));
    // Older version
    QVERIFY(!UpdateManager::isVersionNewer(QStringLiteral("2.12.10"), QStringLiteral("2.12.11")));
    // Minor upgrade
    QVERIFY(UpdateManager::isVersionNewer(QStringLiteral("2.13.0"), QStringLiteral("2.12.11")));
    // Major upgrade
    QVERIFY(UpdateManager::isVersionNewer(QStringLiteral("3.0.0"), QStringLiteral("2.12.11")));
    // With 'v' prefix
    QVERIFY(UpdateManager::isVersionNewer(QStringLiteral("v2.12.12"), QStringLiteral("2.12.11")));
    QVERIFY(UpdateManager::isVersionNewer(QStringLiteral("2.12.12"), QStringLiteral("v2.12.11")));
    QVERIFY(!UpdateManager::isVersionNewer(QStringLiteral("v2.12.11"), QStringLiteral("v2.12.11")));
    // Invalid strings
    QVERIFY(!UpdateManager::isVersionNewer(QStringLiteral("invalid"), QStringLiteral("2.12.11")));
    QVERIFY(!UpdateManager::isVersionNewer(QStringLiteral(""), QStringLiteral("2.12.11")));
}

void UpdateManagerTest::testVerifyNonExistentFile()
{
    QString errorMsg;
    bool ok = UpdateManager::verifyDownloadedPayload(QStringLiteral("/tmp/non_existent_astraplay_file_12345.AppImage"), errorMsg);
    QVERIFY(!ok);
    QVERIFY(!errorMsg.isEmpty());
}

void UpdateManagerTest::testVerifySmallFile()
{
    QTemporaryFile tempFile;
    QVERIFY(tempFile.open());
    tempFile.write("Short dummy content");
    tempFile.close();

    QString errorMsg;
    bool ok = UpdateManager::verifyDownloadedPayload(tempFile.fileName(), errorMsg);
    QVERIFY(!ok);
    QVERIFY(errorMsg.contains(QStringLiteral("incomplete or corrupted"), Qt::CaseInsensitive));
}

void UpdateManagerTest::testVerifyHtmlPayload()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QString path = tempDir.filePath(QStringLiteral("update.AppImage"));

    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QByteArray htmlContent = "<!DOCTYPE html><html><body>Error 404 Not Found</body></html>";
    file.write(htmlContent);
    // Pad to > 5MB to test that the magic byte check catches HTML even if file size is large
    QByteArray padding(5 * 1024 * 1024 + 100 - htmlContent.size(), ' ');
    file.write(padding);
    file.close();

    QString errorMsg;
    bool ok = UpdateManager::verifyDownloadedPayload(path, errorMsg);
    QVERIFY(!ok);
    QVERIFY(errorMsg.contains(QStringLiteral("server returned web/text content"), Qt::CaseInsensitive)
            || errorMsg.contains(QStringLiteral("missing ELF header"), Qt::CaseInsensitive));
}

void UpdateManagerTest::testVerifyValidPayload()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
#if defined(Q_OS_LINUX)
    QString path = tempDir.filePath(QStringLiteral("update.AppImage"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    // Valid ELF magic: \x7fELF
    file.write("\x7f""ELF");
    QByteArray padding(6 * 1024 * 1024, 0x00);
    file.write(padding);
    file.close();

    QString errorMsg;
    bool ok = UpdateManager::verifyDownloadedPayload(path, errorMsg);
    QVERIFY2(ok, qPrintable(errorMsg));
#elif defined(Q_OS_WIN)
    QString path = tempDir.filePath(QStringLiteral("update.exe"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("MZ");
    QByteArray padding(6 * 1024 * 1024, 0x00);
    file.write(padding);
    file.close();

    QString errorMsg;
    bool ok = UpdateManager::verifyDownloadedPayload(path, errorMsg);
    QVERIFY2(ok, qPrintable(errorMsg));
#else
    QString path = tempDir.filePath(QStringLiteral("update.dmg"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QByteArray binaryHeader(16, 0x00);
    binaryHeader[0] = 0x12;
    file.write(binaryHeader);
    QByteArray padding(6 * 1024 * 1024, 0x00);
    file.write(padding);
    file.close();

    QString errorMsg;
    bool ok = UpdateManager::verifyDownloadedPayload(path, errorMsg);
    QVERIFY2(ok, qPrintable(errorMsg));
#endif
}

QTEST_GUILESS_MAIN(UpdateManagerTest)
#include "updatemanager_test.moc"
