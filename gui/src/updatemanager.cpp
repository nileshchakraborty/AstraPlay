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
#include <QDebug>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#endif

static const QString REPO_RELEASES_API = QStringLiteral("https://api.github.com/repos/nileshchakraborty/Pylux/releases/latest");

UpdateManager::UpdateManager(Settings *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_nam(new QNetworkAccessManager(this))
{
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
    if (m_settings)
        return m_settings->GetAutoUpdateEnabled();
    return true;
}

void UpdateManager::setAutoCheckEnabled(bool enabled)
{
    if (m_settings) {
        m_settings->SetAutoUpdateEnabled(enabled);
        emit autoCheckEnabledChanged();
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
    if (m_checking)
        return;

    m_userInitiated = userInitiated;
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
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Pylux-Updater/") + QStringLiteral(CHIAKI_VERSION));
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

    m_checking = false;
    emit checkingChanged();

    if (m_checkReply->error() != QNetworkReply::NoError) {
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

    // Look for matching asset for current platform
    m_downloadUrl.clear();
    QJsonArray assets = obj.value("assets").toArray();
    for (const auto &val : assets) {
        QJsonObject asset = val.toObject();
        QString name = asset.value("name").toString();
        QString dlUrl = asset.value("browser_download_url").toString();

#if defined(Q_OS_WIN)
        // Prefer MSI installer (.msi), then exe installer, fallback to portable zip or pylux.exe
        if (name.endsWith(QLatin1String(".msi"), Qt::CaseInsensitive)) {
            m_downloadUrl = dlUrl;
            break;
        } else if (name.endsWith(QLatin1String(".exe"), Qt::CaseInsensitive) &&
                   (name.contains(QLatin1String("installer"), Qt::CaseInsensitive) ||
                    name.contains(QLatin1String("setup"), Qt::CaseInsensitive))) {
            m_downloadUrl = dlUrl;
            break;
        } else if (name.endsWith(QLatin1String(".exe"), Qt::CaseInsensitive) || (name.endsWith(QLatin1String(".zip"), Qt::CaseInsensitive) && name.contains(QLatin1String("windows"), Qt::CaseInsensitive))) {
            if (m_downloadUrl.isEmpty())
                m_downloadUrl = dlUrl;
        }
#elif defined(Q_OS_LINUX)
        if (name.endsWith(QLatin1String(".AppImage"), Qt::CaseInsensitive)) {
            m_downloadUrl = dlUrl;
            break;
        } else if (name.endsWith(QLatin1String(".flatpak"), Qt::CaseInsensitive)) {
            if (m_downloadUrl.isEmpty())
                m_downloadUrl = dlUrl;
        }
#elif defined(Q_OS_MACOS)
        if (name.endsWith(QLatin1String(".dmg"), Qt::CaseInsensitive) || name.endsWith(QLatin1String(".pkg"), Qt::CaseInsensitive)) {
            m_downloadUrl = dlUrl;
            break;
        }
#endif
    }

    m_updateAvailable = hasUpdate;
    emit updateAvailableChanged();

    if (hasUpdate) {
        m_statusMessage = tr("Update available: %1 (Current: %2)").arg(tagName, currentVer);
    } else {
        m_statusMessage = tr("You are up to date (Version %1).").arg(currentVer);
    }
    emit statusMessageChanged();

    emit updateCheckCompleted(hasUpdate, m_userInitiated, tagName);
}

void UpdateManager::startDownload()
{
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
    m_downloadProgress = 0.0;
    emit downloadingChanged();
    emit downloadProgressChanged();

    m_statusMessage = tr("Downloading update...");
    emit statusMessageChanged();

    QUrl url(m_downloadUrl);
    QString filename = QFileInfo(url.path()).fileName();
    if (filename.isEmpty()) {
#if defined(Q_OS_WIN)
        filename = QStringLiteral("pylux-update.exe");
#elif defined(Q_OS_LINUX)
        filename = QStringLiteral("pylux-update.AppImage");
#else
        filename = QStringLiteral("pylux-update.dmg");
#endif
    }

    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    m_downloadedFilePath = QDir(tempDir).filePath(filename);

    QFile file(m_downloadedFilePath);
    if (file.exists())
        file.remove();

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Pylux-Updater/") + QStringLiteral(CHIAKI_VERSION));
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

void UpdateManager::onDownloadDataAvailable()
{
    if (!m_downloadReply)
        return;

    QFile file(m_downloadedFilePath);
    if (file.open(QIODevice::Append)) {
        file.write(m_downloadReply->readAll());
        file.close();
    }
}

void UpdateManager::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    if (bytesTotal > 0) {
        m_downloadProgress = static_cast<qreal>(bytesReceived) / static_cast<qreal>(bytesTotal);
    } else {
        m_downloadProgress = 0.0;
    }
    emit downloadProgressChanged();
}

void UpdateManager::onDownloadFinished()
{
    if (!m_downloadReply)
        return;

    m_downloading = false;
    emit downloadingChanged();

    if (m_downloadReply->error() != QNetworkReply::NoError) {
        qWarning() << "[UpdateManager] Download error:" << m_downloadReply->errorString();
        m_statusMessage = tr("Download failed: %1").arg(m_downloadReply->errorString());
        emit statusMessageChanged();
        emit downloadFailed(m_downloadReply->errorString());
        m_downloadReply->deleteLater();
        m_downloadReply = nullptr;
        return;
    }

    m_downloadReply->deleteLater();
    m_downloadReply = nullptr;

    m_downloadProgress = 1.0;
    emit downloadProgressChanged();

    m_statusMessage = tr("Download complete. Ready to install.");
    emit statusMessageChanged();
    emit downloadFinished(m_downloadedFilePath);
}

void UpdateManager::cancelDownload()
{
    if (m_downloadReply) {
        m_downloadReply->abort();
        m_downloadReply->deleteLater();
        m_downloadReply = nullptr;
    }
    if (m_downloading) {
        m_downloading = false;
        emit downloadingChanged();
    }
}

void UpdateManager::installAndRestart()
{
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
        QString scriptPath = QDir::toNativeSeparators(QStandardPaths::writableLocation(QStandardPaths::TempLocation) + QLatin1String("/pylux_run_msi.bat"));
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
            out << "msiexec /i \"" << nativePath << "\" /qn /norestart ALLUSERS=1\r\n";
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
        // Run setup executable in fully silent mode (/VERYSILENT /SUPPRESSMSGBOXES /NORESTART /CLOSEAPPLICATIONS), purging .bak and .old files
        QString scriptPath = QDir::toNativeSeparators(QStandardPaths::writableLocation(QStandardPaths::TempLocation) + QLatin1String("/pylux_run_installer.bat"));
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
            out << "start /wait \"\" \"" << nativePath << "\" /VERYSILENT /SUPPRESSMSGBOXES /NORESTART /CLOSEAPPLICATIONS /DIR=\"" << appDir << "\"\r\n";
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
        QString scriptPath = QDir::toNativeSeparators(QStandardPaths::writableLocation(QStandardPaths::TempLocation) + QLatin1String("/pylux_update.bat"));
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
        QFile::setPermissions(m_downloadedFilePath, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner | QFile::ReadGroup | QFile::ExeGroup | QFile::ReadOther | QFile::ExeOther);
        QProcess::startDetached(m_downloadedFilePath, QStringList());
        QGuiApplication::quit();
    } else {
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_downloadedFilePath));
    }
#else
    QDesktopServices::openUrl(QUrl::fromLocalFile(m_downloadedFilePath));
#endif
}
