#pragma once

#include <QObject>
#include <QString>
#include <QNetworkAccessManager>
#include <QNetworkReply>

class Settings;
class QFile;

class UpdateManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool checking READ isChecking NOTIFY checkingChanged)
    Q_PROPERTY(bool updateAvailable READ isUpdateAvailable NOTIFY updateAvailableChanged)
    Q_PROPERTY(bool downloading READ isDownloading NOTIFY downloadingChanged)
    Q_PROPERTY(qreal downloadProgress READ downloadProgress NOTIFY downloadProgressChanged)
    Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY latestVersionChanged)
    Q_PROPERTY(QString currentVersion READ currentVersion CONSTANT)
    Q_PROPERTY(QString releaseNotes READ releaseNotes NOTIFY releaseNotesChanged)
    Q_PROPERTY(QString releaseUrl READ releaseUrl NOTIFY releaseUrlChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(bool autoCheckEnabled READ autoCheckEnabled WRITE setAutoCheckEnabled NOTIFY autoCheckEnabledChanged)
    Q_PROPERTY(bool autoDownloadUpdates READ autoDownloadUpdates WRITE setAutoDownloadUpdates NOTIFY autoDownloadUpdatesChanged)
    // [ASTRAPLAY BEGIN] Keep package-managed installations on their update channel.
    Q_PROPERTY(bool packageUpdatesManaged READ packageUpdatesManaged CONSTANT)
    // [ASTRAPLAY END]
    Q_PROPERTY(qint64 bytesReceived READ bytesReceived NOTIFY downloadProgressChanged)
    Q_PROPERTY(qint64 bytesTotal READ bytesTotal NOTIFY downloadProgressChanged)
    Q_PROPERTY(QString downloadSpeed READ downloadSpeed NOTIFY downloadProgressChanged)
    Q_PROPERTY(QString downloadSizeString READ downloadSizeString NOTIFY downloadProgressChanged)
    Q_PROPERTY(bool failed READ isFailed NOTIFY failedChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(bool readyToInstall READ isReadyToInstall NOTIFY readyToInstallChanged)

public:
    explicit UpdateManager(Settings *settings, QObject *parent = nullptr);
    ~UpdateManager() override;

    bool isChecking() const { return m_checking; }
    bool isUpdateAvailable() const { return m_updateAvailable; }
    bool isDownloading() const { return m_downloading; }
    qreal downloadProgress() const { return m_downloadProgress; }
    QString latestVersion() const { return m_latestVersion; }
    QString currentVersion() const { return QStringLiteral(CHIAKI_VERSION); }
    QString releaseNotes() const { return m_releaseNotes; }
    QString releaseUrl() const { return m_releaseUrl; }
    QString statusMessage() const { return m_statusMessage; }

    bool autoCheckEnabled() const;
    void setAutoCheckEnabled(bool enabled);
    bool autoDownloadUpdates() const;
    void setAutoDownloadUpdates(bool enabled);

    // [ASTRAPLAY BEGIN]
    bool packageUpdatesManaged() const { return m_packageUpdatesManaged; }
    // [ASTRAPLAY END]

    qint64 bytesReceived() const { return m_bytesReceived; }
    qint64 bytesTotal() const { return m_bytesTotal; }
    QString downloadSpeed() const { return m_downloadSpeed; }
    QString downloadSizeString() const { return m_downloadSizeString; }
    bool isFailed() const { return m_failed; }
    QString lastError() const { return m_lastError; }
    bool isReadyToInstall() const { return m_readyToInstall; }

    Q_INVOKABLE void checkForUpdates(bool userInitiated = false);
    Q_INVOKABLE void startDownload();
    Q_INVOKABLE void retryDownload();
    Q_INVOKABLE void installAndRestart();
    Q_INVOKABLE void cancelDownload();

    // Utility to compare semver versions (returns true if remote > current)
    static bool isVersionNewer(const QString &remoteVersion, const QString &currentVersion);
    static bool verifyDownloadedPayload(const QString &filePath, QString &errorMsg);

signals:
    void checkingChanged();
    void updateAvailableChanged();
    void downloadingChanged();
    void downloadProgressChanged();
    void latestVersionChanged();
    void releaseNotesChanged();
    void releaseUrlChanged();
    void statusMessageChanged();
    void autoCheckEnabledChanged();
    void autoDownloadUpdatesChanged();
    void failedChanged();
    void lastErrorChanged();
    void readyToInstallChanged();

    void updateCheckCompleted(bool hasUpdate, bool userInitiated, const QString &version);
    void downloadFinished(const QString &downloadedFilePath);
    void downloadFailed(const QString &errorMessage);

private slots:
    void onCheckFinished();
    void onDownloadDataAvailable();
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onDownloadFinished();

private:
    Settings *m_settings;
    QNetworkAccessManager *m_nam;
    QNetworkReply *m_checkReply = nullptr;
    QNetworkReply *m_downloadReply = nullptr;

    bool m_checking = false;
    bool m_updateAvailable = false;
    bool m_downloading = false;
    bool m_userInitiated = false;
    // [ASTRAPLAY BEGIN]
    bool m_packageUpdatesManaged = false;
    // [ASTRAPLAY END]
    qreal m_downloadProgress = 0.0;

    QString m_latestVersion;
    QString m_releaseNotes;
    QString m_releaseUrl;
    QString m_downloadUrl;
    QString m_statusMessage;
    QString m_downloadedFilePath;
    QFile *m_downloadFile = nullptr;
    bool m_userCancelled = false;
    bool m_usedFallback = false;

    qint64 m_bytesReceived = 0;
    qint64 m_bytesTotal = 0;
    qint64 m_speedLastTime = 0;
    qint64 m_speedLastBytes = 0;
    QString m_downloadSpeed;
    QString m_downloadSizeString;
    bool m_failed = false;
    QString m_lastError;
    bool m_readyToInstall = false;
};
