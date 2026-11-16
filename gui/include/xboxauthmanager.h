// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
#pragma once

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QJsonArray>
#include <QJsonObject>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QNetworkReply>

#include "xboxpopsigner.h"

struct XboxConsoleInfo
{
    QString id;
    QString name;
    QString powerState; // "ConnectedStandby", "On", "Off"
    QString consoleType; // "XboxOne", "XboxSeriesX", "XboxSeriesS"
    bool isOnline;
};

class XboxAuthManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isLoggedIn READ isLoggedIn NOTIFY loggedInChanged)
    Q_PROPERTY(QString gamertag READ getGamertag NOTIFY loggedInChanged)
    Q_PROPERTY(QString userHash READ getUserHash NOTIFY loggedInChanged)
    Q_PROPERTY(QJsonArray consoles READ getConsolesJson NOTIFY consolesUpdated)
    Q_PROPERTY(QJsonArray cloudTitles READ getCloudTitlesJson NOTIFY cloudTitlesUpdated)
    Q_PROPERTY(bool isFetchingTitles READ isFetchingTitles NOTIFY isFetchingTitlesChanged)

public:
    static XboxAuthManager *instance();
    explicit XboxAuthManager(QObject *parent = nullptr);
    ~XboxAuthManager() override;

    bool isLoggedIn() const { return m_isLoggedIn; }
    QString getGamertag() const { return m_gamertag; }
    QString getUserHash() const { return m_userHash; }
    QString getGssvToken() const { return m_gssvToken; }
    QString getWebToken() const { return m_webToken; }
    QString getRefreshToken() const { return m_refreshToken; }
    QJsonArray getConsolesJson() const;
    QJsonArray getCloudTitlesJson() const { return m_cloudTitles; }
    bool isFetchingTitles() const { return m_isFetchingTitles; }

    // Device code authentication flow
    Q_INVOKABLE void startDeviceCodeLogin();
    Q_INVOKABLE void cancelLogin();
    Q_INVOKABLE void logout();

    // Token management
    Q_INVOKABLE void refreshTokens();
    bool checkSavedCredentials();

    // Console management (Smartglass via XCCS)
    Q_INVOKABLE void fetchConsoles();
    Q_INVOKABLE void wakeConsole(const QString &consoleId);
    Q_INVOKABLE void turnOffConsole(const QString &consoleId);

    // Xbox Cloud Gaming catalog
    Q_INVOKABLE void fetchCloudTitles();

    // Get streaming token for a specific offering ("xhome", "xgpuweb")
    void getStreamingToken(const QString &offeringId, std::function<void(bool success, const QString &token, const QString &host)> callback);

signals:
    void loggedInChanged(bool loggedIn);
    void deviceCodeReceived(const QString &userCode, const QString &verificationUri, const QString &qrUrl, int expiresIn);
    void authenticated(const QString &gamertag, const QString &uhs);
    void authFailed(const QString &errorMessage);
    void loginCancelled();
    void loggedOut();
    void consolesUpdated(const QJsonArray &consoles);
    void cloudTitlesUpdated(const QJsonArray &titles);
    void isFetchingTitlesChanged(bool isFetching);

private slots:
    void onDeviceCodePollTimer();

private:
    static XboxAuthManager *s_instance;

    QNetworkAccessManager *m_nam;
    XboxPoPSigner m_popSigner;

    bool m_isLoggedIn = false;
    QString m_gamertag;
    QString m_userHash;
    QString m_accessToken;
    QString m_refreshToken;
    QString m_xblUserToken;
    QString m_gssvToken;
    QString m_webToken;
    QDateTime m_tokenExpiry;

    // Device code login state
    QString m_deviceCode;
    QTimer *m_pollTimer;
    int m_pollIntervalMs = 5000;
    int m_pollRemainingSec = 900;

    // Discovered consoles
    QJsonArray m_consoles;

    // Cloud titles catalog
    QJsonArray m_cloudTitles;
    bool m_isFetchingTitles = false;
    void loadCachedCatalog();
    void saveCachedCatalog();
    void hydrateCloudCatalog(const QStringList &ids, int offset);

    // Internal request helpers
    void exchangeAccessToken(const QString &accessToken, const QString &refreshToken);
    void exchangeXblUserToken(const QString &accessToken);
    void exchangeXstsTokens();
};
