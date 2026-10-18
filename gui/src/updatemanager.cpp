#include "updatemanager.h"
#include "settings.h"

#include <chiaki/version.h>

#include <QGuiApplication>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QProcess>
#include <QDesktopServices>
#include <QUrl>
#include <QRegularExpression>
#include <QSysInfo>
#include <QDateTime>
#include <QDebug>

static QString formatByteSize(qint64 bytes)
{
    if (bytes >= 1024ULL * 1024 * 1024)
        return QString::asprintf("%.2f GB", bytes / (1024.0 * 1024.0 * 1024.0));
    if (bytes >= 1024ULL * 1024)
        return QString::asprintf("%.1f MB", bytes / (1024.0 * 1024.0));
    if (bytes >= 1024ULL)
        return QString::asprintf("%.0f KB", bytes / 1024.0);
    return QString::asprintf("%lld B", static_cast<long long>(bytes));
}

static QString escapeSh(const QString &str)
{
    QString res = str;
    res.replace(QLatin1Char('\''), QLatin1String("'\\''"));
    return QLatin1Char('\'') + res + QLatin1Char('\'');
}

#ifdef Q_OS_WIN
static QString escapePs(const QString &str)
{
    QString res = str;
    res.replace(QLatin1Char('\''), QLatin1String("''"));
    return res;
}
#endif

bool UpdateManager::verifyDownloadedPayload(const QString &filePath, QString &errorMsg)
{
    QFileInfo fi(filePath);
    if (!fi.exists() || !fi.isFile()) {
        errorMsg = tr("Downloaded update file does not exist.");
        return false;
    }

    qint64 size = fi.size();
    if (size < 5 * 1024 * 1024) { // Minimum 5 MB for any AstraPlay artifact
        errorMsg = tr("Downloaded file is incomplete or corrupted (size: %1).").arg(formatByteSize(size));
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        errorMsg = tr("Cannot open downloaded file for verification.");
        return false;
    }

    QByteArray header = file.read(16);
    file.close();

    if (header.startsWith("<!DOC") || header.startsWith("<html") || header.startsWith("<?xml") || header.startsWith("{\"error\"")) {
        errorMsg = tr("Downloaded file is not a valid update payload (server returned web/text content).");
        return false;
    }

#if defined(Q_OS_LINUX)
    if (filePath.endsWith(QLatin1String(".AppImage"), Qt::CaseInsensitive)) {
        // Linux executable / AppImage must begin with standard ELF magic: \x7fELF
        if (header.size() < 4 || header[0] != '\x7f' || header[1] != 'E' || header[2] != 'L' || header[3] != 'F') {
            errorMsg = tr("Downloaded payload is not a valid Linux binary (missing ELF header).");
            return false;
        }
    }
#elif defined(Q_OS_WIN)
    if (filePath.endsWith(QLatin1String(".exe"), Qt::CaseInsensitive)) {
        // Windows PE executable must begin with MZ header: 'M', 'Z'
        if (header.size() < 2 || header[0] != 'M' || header[1] != 'Z') {
            errorMsg = tr("Downloaded payload is not a valid Windows executable.");
            return false;
        }
    }
#elif defined(Q_OS_MACOS)
    if (filePath.endsWith(QLatin1String(".zip"), Qt::CaseInsensitive)) {
        // Zip magic: 'P', 'K'
        if (header.size() < 2 || header[0] != 'P' || header[1] != 'K') {
            errorMsg = tr("Downloaded payload is not a valid zip archive.");
            return false;
        }
    }
#endif

    return true;
}

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
// [ASTRAPLAY BEGIN]
#include <appmodel.h>
// [ASTRAPLAY END]
#endif

static const QString REPO_RELEASES_API = QStringLiteral("https://api.github.com/repos/nileshchakraborty/AstraPlay/releases/latest");
static const QString REPO_VERSION_FALLBACK_URL = QStringLiteral("https://raw.githubusercontent.com/nileshchakraborty/AstraPlay/master/version.json");

UpdateManager::UpdateManager(Settings *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_nam(new QNetworkAccessManager(this))
{
#ifdef Q_OS_WIN
    // [ASTRAPLAY BEGIN] MSIX files cannot be replaced by the GitHub updater.
    UINT32 packageNameLength = 0;
    m_packageUpdatesManaged = GetCurrentPackageFullName(&packageNameLength, nullptr) == ERROR_INSUFFICIENT_BUFFER;
    if (m_packageUpdatesManaged)
        m_statusMessage = tr("Windows manages updates for this package through Microsoft Store or App Installer.");
    // [ASTRAPLAY END]
#endif
#ifdef Q_OS_LINUX
    // Flatpak updates are delivered by the configured remote, so the in-app
    // AppImage updater must not replace the sandboxed installation.
    m_packageUpdatesManaged = qEnvironmentVariableIsSet("FLATPAK_ID");
    if (m_packageUpdatesManaged)
        m_statusMessage = tr("Flatpak manages updates for this package. Update AstraPlay through Discover or your software center.");
#endif
}

UpdateManager::~UpdateManager()
{
    cancelDownload();
    if (m_checkReply) {
        m_checkReply->abort();
        m_checkReply->deleteLater();
        m_checkReply = nullptr;
    }
}

bool UpdateManager::autoCheckEnabled() const
{
    // [ASTRAPLAY BEGIN]
    if (m_packageUpdatesManaged)
        return false;
    // [ASTRAPLAY END]
    if (m_settings)
        return m_settings->GetAutoUpdateEnabled();
    return true;
}

void UpdateManager::setAutoCheckEnabled(bool enabled)
{
    // [ASTRAPLAY BEGIN]
    if (m_packageUpdatesManaged)
        return;
    // [ASTRAPLAY END]
    if (m_settings) {
        m_settings->SetAutoUpdateEnabled(enabled);
        emit autoCheckEnabledChanged();
    }
}

bool UpdateManager::autoDownloadUpdates() const
{
    if (m_packageUpdatesManaged)
        return false;
    if (m_settings)
        return m_settings->GetAutoDownloadUpdates();
    return false;
}

void UpdateManager::setAutoDownloadUpdates(bool enabled)
{
    if (m_packageUpdatesManaged)
        return;
    if (m_settings) {
        m_settings->SetAutoDownloadUpdates(enabled);
        emit autoDownloadUpdatesChanged();
    }
}

bool UpdateManager::isVersionNewer(const QString &remoteVersion, const QString &currentVersion)
{
    auto sanitize = [](QString v) -> QStringList {
        if (v.startsWith(QLatin1Char('v'), Qt::CaseInsensitive))
            v = v.mid(1);
        int dashIdx = v.indexOf(QLatin1Char('-'));
        if (dashIdx != -1)
            v = v.left(dashIdx);
        return v.split(QLatin1Char('.'), Qt::SkipEmptyParts);
    };

    QStringList rParts = sanitize(remoteVersion);
    QStringList cParts = sanitize(currentVersion);

    int count = qMax(rParts.size(), cParts.size());
    for (int i = 0; i < count; ++i) {
        int rNum = (i < rParts.size()) ? rParts[i].toInt() : 0;
        int cNum = (i < cParts.size()) ? cParts[i].toInt() : 0;
        if (rNum > cNum)
            return true;
        if (rNum < cNum)
            return false;
    }

    return false;
}

void UpdateManager::checkForUpdates(bool userInitiated)
{
    // [ASTRAPLAY BEGIN] Keep packaged installations off the self-update path.
    if (m_packageUpdatesManaged) {
#ifdef Q_OS_WIN
        m_statusMessage = tr("Windows manages updates for this package through Microsoft Store or App Installer.");
#else
        m_statusMessage = tr("Flatpak manages updates for this package. Update AstraPlay through Discover or your software center.");
#endif
        emit statusMessageChanged();
        emit updateCheckCompleted(false, userInitiated, QString());
        return;
    }
    // [ASTRAPLAY END]
    if (m_checking)
        return;

    m_userInitiated = userInitiated;
    m_usedFallback = false;
    m_checking = true;
    emit checkingChanged();

    m_statusMessage = tr("Checking for updates...");
    emit statusMessageChanged();

#if defined(Q_OS_WIN)
    // Clean up any stale .bak files in application directory
    QString appDir = QFileInfo(QCoreApplication::applicationFilePath()).absolutePath();
    QDir dir(appDir);
    const QStringList bakFiles = dir.entryList(QStringList() << QStringLiteral("*.bak"), QDir::Files);
    for (const QString &bakFile : bakFiles) {
        dir.remove(bakFile);
    }
#endif

    QNetworkRequest request{QUrl(REPO_RELEASES_API)};
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("AstraPlay-Updater/") + QStringLiteral(CHIAKI_VERSION));
    request.setRawHeader("Accept", "application/vnd.github.v3+json");

    if (m_checkReply) {
        m_checkReply->abort();
        m_checkReply->deleteLater();
    }

    m_checkReply = m_nam->get(request);
    connect(m_checkReply, &QNetworkReply::finished, this, &UpdateManager::onCheckFinished);
}

void UpdateManager::onCheckFinished()
{
    if (!m_checkReply)
        return;

    int statusCode = m_checkReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    bool hasNetworkError = (m_checkReply->error() != QNetworkReply::NoError);
    bool isHttpError = (statusCode == 403 || statusCode >= 400);

    if ((hasNetworkError || isHttpError) && !m_usedFallback) {
        qWarning() << "[UpdateManager] Primary update check failed (HTTP" << statusCode
                   << (hasNetworkError ? m_checkReply->errorString() : QString())
                   << "), attempting CDN fallback:" << REPO_VERSION_FALLBACK_URL;
        m_usedFallback = true;
        m_checkReply->deleteLater();
        m_checkReply = nullptr;

        QNetworkRequest fallbackRequest{QUrl(REPO_VERSION_FALLBACK_URL)};
        fallbackRequest.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("AstraPlay-Updater/") + QStringLiteral(CHIAKI_VERSION));
        m_checkReply = m_nam->get(fallbackRequest);
        connect(m_checkReply, &QNetworkReply::finished, this, &UpdateManager::onCheckFinished);
        return;
    }

    m_checking = false;
    emit checkingChanged();

    if (hasNetworkError) {
        qWarning() << "[UpdateManager] Network error checking updates:" << m_checkReply->errorString();
        m_statusMessage = tr("Failed to check for updates: %1").arg(m_checkReply->errorString());
        emit statusMessageChanged();
        emit updateCheckCompleted(false, m_userInitiated, QString());
        m_checkReply->deleteLater();
        m_checkReply = nullptr;
        return;
    }

    QByteArray data = m_checkReply->readAll();
    m_checkReply->deleteLater();
    m_checkReply = nullptr;

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (!m_usedFallback) {
            qWarning() << "[UpdateManager] JSON parse error, attempting CDN fallback:" << REPO_VERSION_FALLBACK_URL;
            m_usedFallback = true;
            m_checking = true;
            emit checkingChanged();
            QNetworkRequest fallbackRequest{QUrl(REPO_VERSION_FALLBACK_URL)};
            fallbackRequest.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("AstraPlay-Updater/") + QStringLiteral(CHIAKI_VERSION));
            m_checkReply = m_nam->get(fallbackRequest);
            connect(m_checkReply, &QNetworkReply::finished, this, &UpdateManager::onCheckFinished);
            return;
        }
        qWarning() << "[UpdateManager] JSON parse error:" << parseError.errorString();
        m_statusMessage = tr("Failed to parse release metadata.");
        emit statusMessageChanged();
        emit updateCheckCompleted(false, m_userInitiated, QString());
        return;
    }

    QJsonObject obj = doc.object();
    QString tagName = obj.value("tag_name").toString();
    m_releaseNotes = obj.value("body").toString();
    m_releaseUrl = obj.value("html_url").toString();
    emit releaseNotesChanged();
    emit releaseUrlChanged();

    QString currentVer = QStringLiteral(CHIAKI_VERSION);
    bool hasUpdate = isVersionNewer(tagName, currentVer);

    m_latestVersion = tagName;
    emit latestVersionChanged();

    // Look for matching asset for current platform and CPU architecture
    m_downloadUrl.clear();
    QJsonArray assets = obj.value("assets").toArray();

    QString arch = QSysInfo::currentCpuArchitecture().toLower();
    bool isArm = (arch == QLatin1String("arm64") || arch == QLatin1String("aarch64"));
    bool isX86 = (arch == QLatin1String("x86_64") || arch == QLatin1String("amd64") || arch == QLatin1String("x64"));

#if defined(Q_OS_LINUX)
    QString bestUrl;
    for (const auto &val : assets) {
        QJsonObject asset = val.toObject();
        QString name = asset.value("name").toString();
        QString dlUrl = asset.value("browser_download_url").toString();

        if (name.endsWith(QLatin1String(".AppImage"), Qt::CaseInsensitive)) {
            bool assetIsArm = (name.contains(QLatin1String("aarch64"), Qt::CaseInsensitive) || name.contains(QLatin1String("arm64"), Qt::CaseInsensitive));
            bool assetIsX86 = (name.contains(QLatin1String("x86_64"), Qt::CaseInsensitive) || name.contains(QLatin1String("x64"), Qt::CaseInsensitive));

            if (isArm && assetIsArm) {
                bestUrl = dlUrl;
                break;
            } else if (isX86 && assetIsX86) {
                bestUrl = dlUrl;
                break;
            } else if (!assetIsArm && !assetIsX86 && bestUrl.isEmpty()) {
                bestUrl = dlUrl;
            }
        }
    }
    m_downloadUrl = bestUrl;
#elif defined(Q_OS_MACOS)
    QString bestZipUrl;
    QString bestDmgUrl;
    QString universalZipUrl;
    QString universalDmgUrl;
    for (const auto &val : assets) {
        QJsonObject asset = val.toObject();
        QString name = asset.value("name").toString();
        QString dlUrl = asset.value("browser_download_url").toString();

        bool isZip = name.endsWith(QLatin1String(".zip"), Qt::CaseInsensitive);
        bool isDmg = name.endsWith(QLatin1String(".dmg"), Qt::CaseInsensitive);
        if (!isZip && !isDmg)
            continue;

        // Skip installer ZIPs/DMGs for the in-app updater - prefer clean application payloads
        if (name.contains(QLatin1String("installer"), Qt::CaseInsensitive))
            continue;

        bool assetIsArm = name.contains(QLatin1String("arm64"), Qt::CaseInsensitive);
        bool assetIsX86 = name.contains(QLatin1String("x86_64"), Qt::CaseInsensitive);

        if (isZip) {
            if (isArm && assetIsArm)
                bestZipUrl = dlUrl;
            else if (isX86 && assetIsX86)
                bestZipUrl = dlUrl;
            else if (!assetIsArm && !assetIsX86 && universalZipUrl.isEmpty())
                universalZipUrl = dlUrl;
        } else if (isDmg) {
            if (isArm && assetIsArm)
                bestDmgUrl = dlUrl;
            else if (isX86 && assetIsX86)
                bestDmgUrl = dlUrl;
            else if (!assetIsArm && !assetIsX86 && universalDmgUrl.isEmpty())
                universalDmgUrl = dlUrl;
        }
    }
    // Prefer .zip payload: extracts in sub-second via ditto with zero disk image mount/unmount overhead,
    // avoiding "Resource busy" kernel lock conflicts and macOS Sequoia Gatekeeper DMG mount blockers.
    if (!bestZipUrl.isEmpty())
        m_downloadUrl = bestZipUrl;
    else if (!universalZipUrl.isEmpty())
        m_downloadUrl = universalZipUrl;
    else if (!bestDmgUrl.isEmpty())
        m_downloadUrl = bestDmgUrl;
    else
        m_downloadUrl = universalDmgUrl;
#elif defined(Q_OS_WIN)
    QString msiUrl;
    QString installerExeUrl;
    QString portableExeUrl;
    for (const auto &val : assets) {
        QJsonObject asset = val.toObject();
        QString name = asset.value("name").toString();
        QString dlUrl = asset.value("browser_download_url").toString();

        bool assetIsArm = name.contains(QLatin1String("arm64"), Qt::CaseInsensitive);
        if (isArm && !assetIsArm && !name.contains(QLatin1String("x64"), Qt::CaseInsensitive))
            continue;
        if (!isArm && assetIsArm)
            continue;

        if (name.endsWith(QLatin1String(".msi"), Qt::CaseInsensitive)) {
            msiUrl = dlUrl;
        } else if (name.endsWith(QLatin1String(".exe"), Qt::CaseInsensitive) &&
                   (name.contains(QLatin1String("installer"), Qt::CaseInsensitive) ||
                    name.contains(QLatin1String("setup"), Qt::CaseInsensitive))) {
            installerExeUrl = dlUrl;
        } else if (name.endsWith(QLatin1String(".exe"), Qt::CaseInsensitive)) {
            portableExeUrl = dlUrl;
        }
    }
    // Prefer InnoSetup installer: supports UAC elevation, handles clean in-place upgrades,
    // updates shortcuts/registry, and does not fail silently under unelevated execution like msiexec /qn.
    if (!installerExeUrl.isEmpty())
        m_downloadUrl = installerExeUrl;
    else if (!portableExeUrl.isEmpty())
        m_downloadUrl = portableExeUrl;
    else
        m_downloadUrl = msiUrl;
#endif

    m_updateAvailable = hasUpdate;
    emit updateAvailableChanged();

    if (hasUpdate) {
        m_statusMessage = tr("Update available: %1 (Current: %2)").arg(tagName, currentVer);
    } else {
        m_statusMessage = tr("You are up to date (Version %1).").arg(currentVer);
    }
    emit statusMessageChanged();

    if (hasUpdate && autoDownloadUpdates() && !m_downloadUrl.isEmpty()) {
        startDownload();
    }

    emit updateCheckCompleted(hasUpdate, m_userInitiated, tagName);
}

void UpdateManager::startDownload()
{
    // [ASTRAPLAY BEGIN]
    if (m_packageUpdatesManaged)
        return;
    // [ASTRAPLAY END]
    if (m_downloading)
        return;

    // If no direct downloadable asset found on GitHub Releases, open release URL in browser
    if (m_downloadUrl.isEmpty()) {
        if (!m_releaseUrl.isEmpty()) {
            QDesktopServices::openUrl(QUrl(m_releaseUrl));
        }
        return;
    }

    m_downloading = true;
    m_failed = false;
    m_userCancelled = false;
    m_readyToInstall = false;
    m_bytesReceived = 0;
    m_bytesTotal = 0;
    m_downloadProgress = 0.0;
    m_downloadSpeed.clear();
    m_downloadSizeString.clear();
    m_speedLastTime = 0;
    m_speedLastBytes = 0;

    emit downloadingChanged();
    emit failedChanged();
    emit readyToInstallChanged();
    emit downloadProgressChanged();

    m_statusMessage = tr("Downloading update...");
    emit statusMessageChanged();

    QUrl url(m_downloadUrl);
    QString filename = QFileInfo(url.path()).fileName();
    if (filename.isEmpty()) {
#if defined(Q_OS_WIN)
        filename = QStringLiteral("astraplay-update.exe");
#elif defined(Q_OS_LINUX)
        filename = QStringLiteral("astraplay-update.AppImage");
#else
        filename = QStringLiteral("astraplay-update.dmg");
#endif
    }

    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    m_downloadedFilePath = QDir(tempDir).filePath(filename);

    if (m_downloadFile) {
        if (m_downloadFile->isOpen())
            m_downloadFile->close();
        delete m_downloadFile;
        m_downloadFile = nullptr;
    }

    QFile::remove(m_downloadedFilePath);

    m_downloadFile = new QFile(this);
    m_downloadFile->setFileName(m_downloadedFilePath);
    if (!m_downloadFile->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        m_downloading = false;
        m_failed = true;
        m_lastError = tr("Cannot create destination file in temporary directory.");
        m_statusMessage = m_lastError;
        emit downloadingChanged();
        emit failedChanged();
        emit lastErrorChanged();
        emit statusMessageChanged();
        delete m_downloadFile;
        m_downloadFile = nullptr;
        return;
    }

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("AstraPlay-Updater/") + QStringLiteral(CHIAKI_VERSION));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    if (m_downloadReply) {
        m_downloadReply->abort();
        m_downloadReply->deleteLater();
    }

    m_downloadReply = m_nam->get(request);
    connect(m_downloadReply, &QNetworkReply::readyRead, this, &UpdateManager::onDownloadDataAvailable);
    connect(m_downloadReply, &QNetworkReply::downloadProgress, this, &UpdateManager::onDownloadProgress);
    connect(m_downloadReply, &QNetworkReply::finished, this, &UpdateManager::onDownloadFinished);
}

void UpdateManager::retryDownload()
{
    m_failed = false;
    m_lastError.clear();
    emit failedChanged();
    emit lastErrorChanged();
    startDownload();
}

void UpdateManager::onDownloadDataAvailable()
{
    if (!m_downloadReply || !m_downloadFile || !m_downloadFile->isOpen())
        return;

    m_downloadFile->write(m_downloadReply->readAll());
}

void UpdateManager::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    m_bytesReceived = bytesReceived;
    m_bytesTotal = bytesTotal;
    if (bytesTotal > 0) {
        m_downloadProgress = static_cast<qreal>(bytesReceived) / static_cast<qreal>(bytesTotal);
        m_downloadSizeString = QStringLiteral("%1 / %2").arg(formatByteSize(bytesReceived), formatByteSize(bytesTotal));
    } else {
        m_downloadProgress = 0.0;
        m_downloadSizeString = formatByteSize(bytesReceived);
    }

    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (m_speedLastTime == 0) {
        m_speedLastTime = now;
        m_speedLastBytes = bytesReceived;
    } else {
        qint64 elapsed = now - m_speedLastTime;
        if (elapsed >= 500) {
            qint64 diff = bytesReceived - m_speedLastBytes;
            qreal bytesPerSec = (diff * 1000.0) / elapsed;
            if (bytesPerSec >= 1024 * 1024)
                m_downloadSpeed = QString::asprintf("%.1f MB/s", bytesPerSec / (1024.0 * 1024.0));
            else if (bytesPerSec >= 1024)
                m_downloadSpeed = QString::asprintf("%.0f KB/s", bytesPerSec / 1024.0);
            else
                m_downloadSpeed = QString::asprintf("%lld B/s", static_cast<long long>(bytesPerSec));
            m_speedLastTime = now;
            m_speedLastBytes = bytesReceived;
        }
    }
    emit downloadProgressChanged();
}

void UpdateManager::onDownloadFinished()
{
    if (!m_downloadReply)
        return;

    m_downloading = false;
    emit downloadingChanged();

    if (m_downloadFile) {
        if (m_downloadFile->isOpen()) {
            m_downloadFile->flush();
            m_downloadFile->close();
        }
        delete m_downloadFile;
        m_downloadFile = nullptr;
    }

    int statusCode = m_downloadReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (statusCode != 0 && (statusCode < 200 || statusCode >= 300)) {
        m_failed = true;
        m_lastError = tr("Server returned HTTP %1").arg(statusCode);
        emit failedChanged();
        emit lastErrorChanged();
        m_statusMessage = tr("Download failed: %1").arg(m_lastError);
        emit statusMessageChanged();
        emit downloadFailed(m_lastError);
        QFile::remove(m_downloadedFilePath);
        m_downloadReply->deleteLater();
        m_downloadReply = nullptr;
        return;
    }

    if (m_downloadReply->error() != QNetworkReply::NoError) {
        if (m_userCancelled) {
            m_userCancelled = false;
            m_statusMessage = tr("Download cancelled.");
            emit statusMessageChanged();
        } else {
            m_failed = true;
            m_lastError = m_downloadReply->errorString();
            emit failedChanged();
            emit lastErrorChanged();
            qWarning() << "[UpdateManager] Download error:" << m_lastError;
            m_statusMessage = tr("Download failed: %1").arg(m_lastError);
            emit statusMessageChanged();
            emit downloadFailed(m_lastError);
        }
        QFile::remove(m_downloadedFilePath);
        m_downloadReply->deleteLater();
        m_downloadReply = nullptr;
        return;
    }

    m_downloadReply->deleteLater();
    m_downloadReply = nullptr;

    QString verifyError;
    if (!verifyDownloadedPayload(m_downloadedFilePath, verifyError)) {
        m_failed = true;
        m_lastError = verifyError;
        emit failedChanged();
        emit lastErrorChanged();
        qWarning() << "[UpdateManager] Payload verification failed:" << verifyError;
        m_statusMessage = tr("Verification failed: %1").arg(verifyError);
        emit statusMessageChanged();
        emit downloadFailed(verifyError);
        QFile::remove(m_downloadedFilePath);
        return;
    }

    m_failed = false;
    emit failedChanged();

    m_downloadProgress = 1.0;
    m_readyToInstall = true;
    emit downloadProgressChanged();
    emit readyToInstallChanged();

    m_statusMessage = tr("Download complete and verified. Ready to install.");
    emit statusMessageChanged();
    emit downloadFinished(m_downloadedFilePath);
}

void UpdateManager::cancelDownload()
{
    m_userCancelled = true;
    if (m_downloadReply) {
        m_downloadReply->abort();
        m_downloadReply->deleteLater();
        m_downloadReply = nullptr;
    }
    if (m_downloadFile) {
        if (m_downloadFile->isOpen())
            m_downloadFile->close();
        delete m_downloadFile;
        m_downloadFile = nullptr;
    }
    if (m_downloading) {
        m_downloading = false;
        emit downloadingChanged();
    }
    QFile::remove(m_downloadedFilePath);
    m_speedLastTime = 0;
    m_speedLastBytes = 0;
    m_downloadSpeed.clear();
    m_downloadSizeString.clear();
    emit downloadProgressChanged();
}

void UpdateManager::installAndRestart()
{
    // [ASTRAPLAY BEGIN]
    if (m_packageUpdatesManaged)
        return;
    // [ASTRAPLAY END]
    if (m_downloadedFilePath.isEmpty() || !QFile::exists(m_downloadedFilePath)) {
        if (!m_releaseUrl.isEmpty()) {
            QDesktopServices::openUrl(QUrl(m_releaseUrl));
        }
        return;
    }

#if defined(Q_OS_WIN)
    QString nativePath = QDir::toNativeSeparators(m_downloadedFilePath);
    QString appExePath = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    QString appDir = QDir::toNativeSeparators(QFileInfo(appExePath).absolutePath());
    qint64 currentPid = QCoreApplication::applicationPid();

    if (nativePath.endsWith(QLatin1String(".msi"), Qt::CaseInsensitive)) {
        // Run MSI installer cleanly with msiexec in fully silent mode (/qn), purging .bak and .old files
        QString scriptPath = QDir::toNativeSeparators(QStandardPaths::writableLocation(QStandardPaths::TempLocation) + QLatin1String("/astraplay_run_msi.bat"));
        QFile batFile(scriptPath);
        if (batFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&batFile);
            out << "@echo off\r\n";
            out << ":wait_pid\r\n";
            out << "tasklist /FI \"PID eq " << currentPid << "\" 2>NUL | find /I /N \"" << currentPid << "\">NUL\r\n";
            out << "if \"%ERRORLEVEL%\"==\"0\" (\r\n";
            out << "    timeout /t 1 /nobreak >nul\r\n";
            out << "    goto wait_pid\r\n";
            out << ")\r\n";
            out << "del /f /q \"" << appDir << "\\*.bak\" 2>nul\r\n";
            out << "del /f /q \"" << appDir << "\\*.old\" 2>nul\r\n";
            out << "powershell -NoProfile -ExecutionPolicy Bypass -Command \"Unblock-File -LiteralPath '" << escapePs(nativePath) << "' -ErrorAction SilentlyContinue; Start-Process msiexec.exe -ArgumentList '/i \\\"" << nativePath << "\\\" /passive /norestart ALLUSERS=1' -Verb RunAs -Wait\" 2>nul\r\n";
            out << "if \"%ERRORLEVEL%\" neq \"0\" (\r\n";
            out << "    msiexec /i \"" << nativePath << "\" /passive /norestart ALLUSERS=1\r\n";
            out << ")\r\n";
            out << "del /f /q \"" << appDir << "\\*.bak\" 2>nul\r\n";
            out << "del /f /q \"" << appDir << "\\*.old\" 2>nul\r\n";
            out << "start \"\" \"" << appExePath << "\"\r\n";
            out << "del \"%~f0\"\r\n";
            batFile.close();
        }

        QProcess::startDetached("cmd.exe", QStringList() << "/c" << scriptPath);
        QGuiApplication::quit();
    } else if (nativePath.endsWith(QLatin1String(".exe"), Qt::CaseInsensitive) &&
               (nativePath.contains(QLatin1String("installer"), Qt::CaseInsensitive) ||
                nativePath.contains(QLatin1String("setup"), Qt::CaseInsensitive))) {
        // Run setup executable with UAC elevation via PowerShell Start-Process -Verb RunAs, purging .bak and .old files
        QString scriptPath = QDir::toNativeSeparators(QStandardPaths::writableLocation(QStandardPaths::TempLocation) + QLatin1String("/astraplay_run_installer.bat"));
        QFile batFile(scriptPath);
        if (batFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&batFile);
            out << "@echo off\r\n";
            out << ":wait_pid\r\n";
            out << "tasklist /FI \"PID eq " << currentPid << "\" 2>NUL | find /I /N \"" << currentPid << "\">NUL\r\n";
            out << "if \"%ERRORLEVEL%\"==\"0\" (\r\n";
            out << "    timeout /t 1 /nobreak >nul\r\n";
            out << "    goto wait_pid\r\n";
            out << ")\r\n";
            out << "del /f /q \"" << appDir << "\\*.bak\" 2>nul\r\n";
            out << "del /f /q \"" << appDir << "\\*.old\" 2>nul\r\n";
            out << "powershell -NoProfile -ExecutionPolicy Bypass -Command \"Unblock-File -LiteralPath '" << escapePs(nativePath) << "' -ErrorAction SilentlyContinue; Start-Process '" << escapePs(nativePath) << "' -ArgumentList '/VERYSILENT /SUPPRESSMSGBOXES /NORESTART /CLOSEAPPLICATIONS /DIR=\\\"" << escapePs(appDir) << "\\\"' -Verb RunAs -Wait\" 2>nul\r\n";
            out << "if \"%ERRORLEVEL%\" neq \"0\" (\r\n";
            out << "    start /wait \"\" \"" << nativePath << "\" /VERYSILENT /SUPPRESSMSGBOXES /NORESTART /CLOSEAPPLICATIONS /DIR=\"" << appDir << "\"\r\n";
            out << ")\r\n";
            out << "del /f /q \"" << appDir << "\\*.bak\" 2>nul\r\n";
            out << "del /f /q \"" << appDir << "\\*.old\" 2>nul\r\n";
            out << "start \"\" \"" << appExePath << "\"\r\n";
            out << "del \"%~f0\"\r\n";
            batFile.close();
        }

        QProcess::startDetached("cmd.exe", QStringList() << "/c" << scriptPath);
        QGuiApplication::quit();
    } else if (nativePath.endsWith(QLatin1String(".exe"), Qt::CaseInsensitive)) {
        // Direct executable replacement via temporary helper script, purging .bak files
        QString scriptPath = QDir::toNativeSeparators(QStandardPaths::writableLocation(QStandardPaths::TempLocation) + QLatin1String("/astraplay_update.bat"));
        QFile batFile(scriptPath);
        if (batFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&batFile);
            out << "@echo off\r\n";
            out << ":wait_pid\r\n";
            out << "tasklist /FI \"PID eq " << currentPid << "\" 2>NUL | find /I /N \"" << currentPid << "\">NUL\r\n";
            out << "if \"%ERRORLEVEL%\"==\"0\" (\r\n";
            out << "    timeout /t 1 /nobreak >nul\r\n";
            out << "    goto wait_pid\r\n";
            out << ")\r\n";
            out << "del /f /q \"" << appDir << "\\*.bak\" 2>nul\r\n";
            out << "copy /y \"" << nativePath << "\" \"" << appExePath << "\"\r\n";
            out << "del /f /q \"" << appDir << "\\*.bak\" 2>nul\r\n";
            out << "start \"\" \"" << appExePath << "\"\r\n";
            out << "del \"" << nativePath << "\"\r\n";
            out << "del \"%~f0\"\r\n";
            batFile.close();
        }

        QProcess::startDetached("cmd.exe", QStringList() << "/c" << scriptPath);
        QGuiApplication::quit();
    } else {
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_downloadedFilePath));
    }
#elif defined(Q_OS_LINUX)
    if (m_downloadedFilePath.endsWith(QLatin1String(".AppImage"), Qt::CaseInsensitive)) {
        // Find installed AppImage target ($APPIMAGE environment variable or standard location)
        QString appImageTarget = qEnvironmentVariable("APPIMAGE");
        const QString optPath = QDir::home().filePath(QStringLiteral(".local/opt/astraplay/AstraPlay.AppImage"));
        if (!appImageTarget.isEmpty()) {
            QFileInfo fi(appImageTarget);
            QFileInfo dirFi(fi.dir().absolutePath());
            if ((fi.exists() && !fi.isWritable()) || !dirFi.isWritable()) {
                qWarning() << "[UpdateManager] APPIMAGE target is not writable:" << appImageTarget << "falling back to" << optPath;
                appImageTarget = optPath;
            }
        } else {
            QString appPath = QCoreApplication::applicationFilePath();
            if (appPath.endsWith(QLatin1String(".AppImage"), Qt::CaseInsensitive)) {
                QFileInfo fi(appPath);
                QFileInfo dirFi(fi.dir().absolutePath());
                if ((!fi.exists() || fi.isWritable()) && dirFi.isWritable()) {
                    appImageTarget = appPath;
                } else {
                    appImageTarget = optPath;
                }
            } else {
                appImageTarget = optPath;
            }
        }

        qint64 currentPid = QCoreApplication::applicationPid();
        QString scriptPath = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation)).filePath(QStringLiteral("astraplay_update.sh"));
        QFile scriptFile(scriptPath);
        if (scriptFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            QTextStream out(&scriptFile);
            out << "#!/bin/sh\n";
            out << "LOG=\"/tmp/astraplay_update.log\"\n";
            out << "echo \"=== AstraPlay Update $(date) ===\" > \"$LOG\" 2>&1\n";
            out << "TARGET=" << escapeSh(appImageTarget) << "\n";
            out << "UPDATE=" << escapeSh(m_downloadedFilePath) << "\n";
            out << "for i in $(seq 1 100); do\n";
            out << "  if ! kill -0 " << currentPid << " 2>/dev/null; then\n";
            out << "    break\n";
            out << "  fi\n";
            out << "  sleep 0.1\n";
            out << "done\n";
            out << "echo \"Process exited or timeout reached.\" >> \"$LOG\"\n";
            out << "if [ ! -f \"$UPDATE\" ]; then\n";
            out << "  echo \"Update file $UPDATE missing!\" >> \"$LOG\"\n";
            out << "  if [ -n \"$TARGET\" ] && [ -x \"$TARGET\" ]; then\n";
            out << "    rm -f \"$0\"\n";
            out << "    exec \"$TARGET\"\n";
            out << "  fi\n";
            out << "  rm -f \"$0\"\n";
            out << "  exit 1\n";
            out << "fi\n";
            out << "chmod +x \"$UPDATE\" >> \"$LOG\" 2>&1\n";
            out << "REPLACED=0\n";
            out << "if [ -n \"$TARGET\" ]; then\n";
            out << "  TARGET_DIR=\"$(dirname \"$TARGET\")\"\n";
            out << "  mkdir -p \"$TARGET_DIR\" >> \"$LOG\" 2>&1\n";
            out << "  TMP_TARGET=\"${TARGET}.new.$$\"\n";
            out << "  echo \"Staging update to $TMP_TARGET\" >> \"$LOG\"\n";
            out << "  if cp -f \"$UPDATE\" \"$TMP_TARGET\" >> \"$LOG\" 2>&1; then\n";
            out << "    chmod +x \"$TMP_TARGET\" >> \"$LOG\" 2>&1\n";
            out << "    if mv -f \"$TMP_TARGET\" \"$TARGET\" >> \"$LOG\" 2>&1; then\n";
            out << "      echo \"Atomic replacement succeeded: $TARGET\" >> \"$LOG\"\n";
            out << "      REPLACED=1\n";
            out << "      rm -f \"$UPDATE\" 2>/dev/null\n";
            out << "      rm -f \"$0\" 2>/dev/null\n";
            out << "      exec \"$TARGET\"\n";
            out << "    else\n";
            out << "      echo \"mv to $TARGET failed!\" >> \"$LOG\"\n";
            out << "      rm -f \"$TMP_TARGET\" 2>/dev/null\n";
            out << "    fi\n";
            out << "  else\n";
            out << "    echo \"cp to $TMP_TARGET failed!\" >> \"$LOG\"\n";
            out << "    rm -f \"$TMP_TARGET\" 2>/dev/null\n";
            out << "  fi\n";
            out << "fi\n";
            out << "if [ \"$REPLACED\" -eq 0 ]; then\n";
            out << "  FALLBACK_TARGET=\"$HOME/.local/opt/astraplay/AstraPlay.AppImage\"\n";
            out << "  if [ \"$TARGET\" != \"$FALLBACK_TARGET\" ]; then\n";
            out << "    echo \"Attempting fallback staging to $FALLBACK_TARGET\" >> \"$LOG\"\n";
            out << "    mkdir -p \"$HOME/.local/opt/astraplay\" >> \"$LOG\" 2>&1\n";
            out << "    if cp -f \"$UPDATE\" \"${FALLBACK_TARGET}.new.$$\" >> \"$LOG\" 2>&1; then\n";
            out << "      chmod +x \"${FALLBACK_TARGET}.new.$$\" >> \"$LOG\" 2>&1\n";
            out << "      if mv -f \"${FALLBACK_TARGET}.new.$$\" \"$FALLBACK_TARGET\" >> \"$LOG\" 2>&1; then\n";
            out << "        echo \"Fallback replacement succeeded: $FALLBACK_TARGET\" >> \"$LOG\"\n";
            out << "        rm -f \"$UPDATE\" 2>/dev/null\n";
            out << "        rm -f \"$0\" 2>/dev/null\n";
            out << "        exec \"$FALLBACK_TARGET\"\n";
            out << "      fi\n";
            out << "    fi\n";
            out << "  fi\n";
            out << "fi\n";
            out << "echo \"Executing UPDATE directly: $UPDATE\" >> \"$LOG\"\n";
            out << "rm -f \"$0\" 2>/dev/null\n";
            out << "exec \"$UPDATE\"\n";
            scriptFile.close();
            QFile::setPermissions(scriptPath, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner | QFile::ReadGroup | QFile::ExeGroup | QFile::ReadOther | QFile::ExeOther);
            QProcess::startDetached(QStringLiteral("/bin/sh"), QStringList() << scriptPath);
            QGuiApplication::quit();
            return;
        }

        QFile::setPermissions(m_downloadedFilePath, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner | QFile::ReadGroup | QFile::ExeGroup | QFile::ReadOther | QFile::ExeOther);
        QProcess::startDetached(m_downloadedFilePath, QStringList());
        QGuiApplication::quit();
    } else {
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_downloadedFilePath));
    }
#elif defined(Q_OS_MACOS)
    // Silent replace-and-relaunch: a helper script waits for our PID to exit,
    // extracts the new .app from the downloaded .zip/.dmg, dittos it over the
    // target bundle, clears Gatekeeper quarantine, applies ad-hoc signature,
    // and reopens it. Handles AppTranslocation, mounted volumes, and user-dir fallback.
    QString appBundlePath = QFileInfo(QCoreApplication::applicationDirPath() + QStringLiteral("/../..")).canonicalFilePath();
    if (appBundlePath.isEmpty())
        appBundlePath = QFileInfo(QCoreApplication::applicationDirPath() + QStringLiteral("/../..")).absoluteFilePath();

    QString lowerPath = m_downloadedFilePath.toLower();
    bool isPayload = lowerPath.endsWith(QLatin1String(".dmg")) || lowerPath.endsWith(QLatin1String(".zip"));

    // If running from AppTranslocation, mounted DMG, or Downloads, target standard /Applications
    QString targetBundle = appBundlePath;
    if (targetBundle.contains(QLatin1String("AppTranslocation"), Qt::CaseInsensitive)
        || targetBundle.startsWith(QLatin1String("/Volumes/"), Qt::CaseInsensitive)
        || targetBundle.contains(QLatin1String("/Downloads/"), Qt::CaseInsensitive)
        || !targetBundle.endsWith(QLatin1String(".app"), Qt::CaseInsensitive)) {
        if (QFileInfo(QStringLiteral("/Applications")).isWritable()) {
            targetBundle = QStringLiteral("/Applications/AstraPlay.app");
        } else {
            targetBundle = QDir::home().filePath(QStringLiteral("Applications/AstraPlay.app"));
        }
    }

    if (isPayload) {
        qint64 currentPid = QCoreApplication::applicationPid();
        bool isDmg = lowerPath.endsWith(QLatin1String(".dmg"));
        QString scriptPath = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation)).filePath(QStringLiteral("astraplay_update.sh"));
        QFile scriptFile(scriptPath);
        if (scriptFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            QTextStream out(&scriptFile);
            out << "#!/bin/sh\n";
            out << "LOG=\"/tmp/astraplay_update.log\"\n";
            out << "echo \"=== AstraPlay macOS Update $(date) ===\" > \"$LOG\" 2>&1\n";
            out << "APP_BUNDLE=" << escapeSh(targetBundle) << "\n";
            out << "UPDATE_FILE=" << escapeSh(m_downloadedFilePath) << "\n";
            out << "echo \"Target bundle: $APP_BUNDLE\" >> \"$LOG\"\n";
            out << "echo \"Update file:   $UPDATE_FILE\" >> \"$LOG\"\n";
            out << "for i in $(seq 1 50); do\n";
            out << "  if ! kill -0 " << currentPid << " 2>/dev/null; then break; fi\n";
            out << "  sleep 0.2\n";
            out << "done\n";
            out << "if kill -0 " << currentPid << " 2>/dev/null; then\n";
            out << "  kill -9 " << currentPid << " 2>/dev/null || true\n";
            out << "fi\n";
            out << "WORKDIR=$(mktemp -d /tmp/astraplay-upd.XXXXXX) || exit 1\n";
            out << "cleanup() {\n";
            out << "  hdiutil detach -force \"$WORKDIR/mnt\" >/dev/null 2>&1 || true\n";
            out << "  hdiutil detach \"/Volumes/AstraPlay\" -force >/dev/null 2>&1 || true\n";
            out << "  rm -rf \"$WORKDIR\"\n";
            out << "}\n";
            out << "trap cleanup EXIT INT TERM\n";
            out << "xattr -c \"$UPDATE_FILE\" 2>/dev/null || true\n";
            if (isDmg) {
                out << "mkdir -p \"$WORKDIR/mnt\"\n";
                out << "hdiutil detach \"/Volumes/AstraPlay\" -force >/dev/null 2>&1 || true\n";
                out << "if ! hdiutil attach -nobrowse -readonly -mountpoint \"$WORKDIR/mnt\" \"$UPDATE_FILE\" >> \"$LOG\" 2>&1; then\n";
                out << "  echo \"Mountpoint attach failed, trying default attach...\" >> \"$LOG\"\n";
                out << "  hdiutil attach -nobrowse -readonly \"$UPDATE_FILE\" >> \"$LOG\" 2>&1 || true\n";
                out << "fi\n";
                out << "NEWAPP=$(find \"$WORKDIR/mnt\" \"/Volumes/AstraPlay\" \"$WORKDIR\" -maxdepth 3 -name \"AstraPlay.app\" -print -quit 2>/dev/null)\n";
                out << "if [ -z \"$NEWAPP\" ]; then\n";
                out << "  NEWAPP=$(find \"$WORKDIR/mnt\" \"/Volumes/AstraPlay\" \"$WORKDIR\" -maxdepth 3 -name \"*.app\" -print -quit 2>/dev/null)\n";
                out << "fi\n";
            } else {
                out << "if ! ditto -x -k \"$UPDATE_FILE\" \"$WORKDIR\" >> \"$LOG\" 2>&1; then\n";
                out << "  echo \"ditto zip extraction failed!\" >> \"$LOG\"\n";
                out << "fi\n";
                out << "NEWAPP=$(find \"$WORKDIR\" -maxdepth 3 -name \"AstraPlay.app\" -print -quit 2>/dev/null)\n";
                out << "if [ -z \"$NEWAPP\" ]; then\n";
                out << "  NEWAPP=$(find \"$WORKDIR\" -maxdepth 3 -name \"*.app\" -print -quit 2>/dev/null)\n";
                out << "fi\n";
            }
            out << "echo \"Discovered new app at: $NEWAPP\" >> \"$LOG\"\n";
            out << "if [ -z \"$NEWAPP\" ] || [ ! -d \"$NEWAPP\" ]; then\n";
            out << "  echo \"Could not find valid .app in payload!\" >> \"$LOG\"\n";
            out << "  cleanup\n";
            out << "  exit 1\n";
            out << "fi\n";
            out << "REPLACED=0\n";
            out << "TARGET_DIR=\"$(dirname \"$APP_BUNDLE\")\"\n";
            out << "mkdir -p \"$TARGET_DIR\" >> \"$LOG\" 2>&1 || true\n";
            out << "if [ -w \"$TARGET_DIR\" ]; then\n";
            out << "  TMP_BUNDLE=\"${APP_BUNDLE}.new.$$\"\n";
            out << "  rm -rf \"$TMP_BUNDLE\" 2>/dev/null\n";
            out << "  echo \"Staging to $TMP_BUNDLE...\" >> \"$LOG\"\n";
            out << "  if ditto \"$NEWAPP\" \"$TMP_BUNDLE\" >> \"$LOG\" 2>&1; then\n";
            out << "    OLD_BUNDLE=\"${APP_BUNDLE}.old.$$\"\n";
            out << "    rm -rf \"$OLD_BUNDLE\" 2>/dev/null\n";
            out << "    mv \"$APP_BUNDLE\" \"$OLD_BUNDLE\" 2>/dev/null || true\n";
            out << "    if mv \"$TMP_BUNDLE\" \"$APP_BUNDLE\" >> \"$LOG\" 2>&1; then\n";
            out << "      rm -rf \"$OLD_BUNDLE\" 2>/dev/null\n";
            out << "      xattr -cr \"$APP_BUNDLE\" 2>/dev/null || true\n";
            out << "      codesign --force --deep --sign - \"$APP_BUNDLE\" >> \"$LOG\" 2>&1 || true\n";
            out << "      echo \"Successfully replaced $APP_BUNDLE\" >> \"$LOG\"\n";
            out << "      REPLACED=1\n";
            out << "    else\n";
            out << "      echo \"mv to $APP_BUNDLE failed!\" >> \"$LOG\"\n";
            out << "      mv \"$OLD_BUNDLE\" \"$APP_BUNDLE\" 2>/dev/null || true\n";
            out << "      rm -rf \"$TMP_BUNDLE\" 2>/dev/null\n";
            out << "    fi\n";
            out << "  fi\n";
            out << "fi\n";
            out << "if [ \"$REPLACED\" -eq 0 ] && [ \"$APP_BUNDLE\" = \"/Applications/AstraPlay.app\" ]; then\n";
            out << "  echo \"Attempting elevated replacement via osascript...\" >> \"$LOG\"\n";
            out << "  CMD=\"rm -rf '$APP_BUNDLE' && cp -R '$NEWAPP' '$APP_BUNDLE' && xattr -cr '$APP_BUNDLE' && codesign --force --deep --sign - '$APP_BUNDLE' 2>/dev/null || true\"\n";
            out << "  if osascript -e \"do shell script \\\"$CMD\\\" with administrator privileges\" >> \"$LOG\" 2>&1; then\n";
            out << "    echo \"Elevated replacement succeeded: $APP_BUNDLE\" >> \"$LOG\"\n";
            out << "    REPLACED=1\n";
            out << "  fi\n";
            out << "fi\n";
            out << "if [ \"$REPLACED\" -eq 0 ]; then\n";
            out << "  USER_APP=\"$HOME/Applications/AstraPlay.app\"\n";
            out << "  echo \"Primary target failed, staging to user directory: $USER_APP\" >> \"$LOG\"\n";
            out << "  mkdir -p \"$HOME/Applications\" >> \"$LOG\" 2>&1\n";
            out << "  TMP_BUNDLE=\"${USER_APP}.new.$$\"\n";
            out << "  rm -rf \"$TMP_BUNDLE\" 2>/dev/null\n";
            out << "  if ditto \"$NEWAPP\" \"$TMP_BUNDLE\" >> \"$LOG\" 2>&1; then\n";
            out << "    rm -rf \"$USER_APP\" 2>/dev/null\n";
            out << "    if mv \"$TMP_BUNDLE\" \"$USER_APP\" >> \"$LOG\" 2>&1; then\n";
            out << "      xattr -cr \"$USER_APP\" 2>/dev/null || true\n";
            out << "      codesign --force --deep --sign - \"$USER_APP\" >> \"$LOG\" 2>&1 || true\n";
            out << "      echo \"Successfully installed to $USER_APP\" >> \"$LOG\"\n";
            out << "      APP_BUNDLE=\"$USER_APP\"\n";
            out << "      REPLACED=1\n";
            out << "    fi\n";
            out << "  fi\n";
            out << "fi\n";
            out << "cleanup\n";
            out << "rm -f \"$UPDATE_FILE\" 2>/dev/null || true\n";
            out << "rm -f \"$0\" 2>/dev/null || true\n";
            out << "if [ \"$REPLACED\" -eq 1 ]; then\n";
            out << "  echo \"Launching updated app: $APP_BUNDLE\" >> \"$LOG\"\n";
            out << "  open \"$APP_BUNDLE\"\n";
            out << "else\n";
            out << "  echo \"Update installation failed!\" >> \"$LOG\"\n";
            out << "fi\n";
            scriptFile.close();
            QFile::setPermissions(scriptPath, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner | QFile::ReadGroup | QFile::ExeGroup | QFile::ReadOther | QFile::ExeOther);
            QProcess::startDetached(QStringLiteral("/bin/sh"), QStringList() << scriptPath);
            QCoreApplication::exit(0);
            return;
        }
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(m_downloadedFilePath));
#else
    QDesktopServices::openUrl(QUrl::fromLocalFile(m_downloadedFilePath));
#endif
}
