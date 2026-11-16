// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#include "xboxauthmanager.h"
#include "credentialvault.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>
#include <QUuid>
#include <QDateTime>
#include <QDebug>

XboxAuthManager *XboxAuthManager::s_instance = nullptr;

static const char *MSAL_CLIENT_ID = "1f907974-e22b-4810-a9de-d9647380c97e";
static const char *MSAL_SCOPE = "xboxlive.signin openid profile offline_access";

XboxAuthManager *XboxAuthManager::instance()
{
    if (!s_instance) {
        s_instance = new XboxAuthManager();
    }
    return s_instance;
}

XboxAuthManager::XboxAuthManager(QObject *parent)
    : QObject(parent),
      m_nam(new QNetworkAccessManager(this)),
      m_pollTimer(new QTimer(this))
{
    s_instance = this;
    m_popSigner.init();

    m_pollTimer->setSingleShot(false);
    connect(m_pollTimer, &QTimer::timeout, this, &XboxAuthManager::onDeviceCodePollTimer);
}

XboxAuthManager::~XboxAuthManager()
{
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

QJsonArray XboxAuthManager::getConsolesJson() const
{
    return m_consoles;
}

bool XboxAuthManager::checkSavedCredentials()
{
    QString savedRefresh = CredentialVault::readSecret(QStringLiteral("xbox_refresh_token"));
    if (savedRefresh.isEmpty()) {
        return false;
    }

    m_refreshToken = savedRefresh;
    refreshTokens();
    return true;
}

void XboxAuthManager::startDeviceCodeLogin()
{
    cancelLogin();

    QUrl url(QStringLiteral("https://login.microsoftonline.com/consumers/oauth2/v2.0/devicecode"));
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("client_id"), QString::fromLatin1(MSAL_CLIENT_ID));
    query.addQueryItem(QStringLiteral("scope"), QString::fromLatin1(MSAL_SCOPE));

    QByteArray body = query.toString(QUrl::FullyEncoded).toUtf8();
    QNetworkReply *reply = m_nam->post(req, body);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit authFailed(reply->errorString());
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QJsonObject obj = doc.object();

        QString userCode = obj.value(QStringLiteral("user_code")).toString();
        m_deviceCode = obj.value(QStringLiteral("device_code")).toString();
        QString verifyUri = obj.value(QStringLiteral("verification_uri")).toString();
        int expiresIn = obj.value(QStringLiteral("expires_in")).toInt(900);
        int interval = obj.value(QStringLiteral("interval")).toInt(5);

        m_pollIntervalMs = qMax(1, interval) * 1000;
        m_pollRemainingSec = expiresIn;

        emit deviceCodeReceived(userCode, verifyUri, verifyUri, expiresIn);

        m_pollTimer->start(m_pollIntervalMs);
    });
}

void XboxAuthManager::cancelLogin()
{
    m_pollTimer->stop();
    m_deviceCode.clear();
}

void XboxAuthManager::onDeviceCodePollTimer()
{
    m_pollRemainingSec -= (m_pollIntervalMs / 1000);
    if (m_pollRemainingSec <= 0) {
        cancelLogin();
        emit authFailed(tr("Device code login timed out. Please try again."));
        return;
    }

    QUrl url(QStringLiteral("https://login.microsoftonline.com/consumers/oauth2/v2.0/token"));
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("urn:ietf:params:oauth:grant-type:device_code"));
    query.addQueryItem(QStringLiteral("client_id"), QString::fromLatin1(MSAL_CLIENT_ID));
    query.addQueryItem(QStringLiteral("device_code"), m_deviceCode);

    QByteArray body = query.toString(QUrl::FullyEncoded).toUtf8();
    QNetworkReply *reply = m_nam->post(req, body);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        QByteArray respBytes = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(respBytes);
        QJsonObject obj = doc.object();

        if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200) {
            m_pollTimer->stop();
            m_accessToken = obj.value(QStringLiteral("access_token")).toString();
            m_refreshToken = obj.value(QStringLiteral("refresh_token")).toString();

            if (!m_refreshToken.isEmpty()) {
                CredentialVault::writeSecret(QStringLiteral("xbox_refresh_token"), m_refreshToken);
            }

            exchangeAccessToken(m_accessToken, m_refreshToken);
        } else {
            QString err = obj.value(QStringLiteral("error")).toString();
            if (err == QStringLiteral("authorization_pending")) {
                // Keep polling
            } else if (err == QStringLiteral("slow_down")) {
                m_pollIntervalMs += 5000;
                m_pollTimer->setInterval(m_pollIntervalMs);
            } else if (err == QStringLiteral("expired_token")) {
                cancelLogin();
                emit authFailed(tr("Login expired. Please try again."));
            } else {
                cancelLogin();
                emit authFailed(obj.value(QStringLiteral("error_description")).toString(err));
            }
        }
    });
}

void XboxAuthManager::refreshTokens()
{
    if (m_refreshToken.isEmpty()) {
        emit authFailed(tr("No refresh token available."));
        return;
    }

    QUrl url(QStringLiteral("https://login.microsoftonline.com/consumers/oauth2/v2.0/token"));
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("refresh_token"));
    query.addQueryItem(QStringLiteral("client_id"), QString::fromLatin1(MSAL_CLIENT_ID));
    query.addQueryItem(QStringLiteral("scope"), QString::fromLatin1(MSAL_SCOPE));
    query.addQueryItem(QStringLiteral("refresh_token"), m_refreshToken);

    QByteArray body = query.toString(QUrl::FullyEncoded).toUtf8();
    QNetworkReply *reply = m_nam->post(req, body);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200) {
            QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            QJsonObject obj = doc.object();
            m_accessToken = obj.value(QStringLiteral("access_token")).toString();
            m_refreshToken = obj.value(QStringLiteral("refresh_token")).toString();

            if (!m_refreshToken.isEmpty()) {
                CredentialVault::writeSecret(QStringLiteral("xbox_refresh_token"), m_refreshToken);
            }

            exchangeAccessToken(m_accessToken, m_refreshToken);
        } else {
            emit authFailed(tr("Failed to refresh Xbox credentials. Please log in again."));
        }
    });
}

void XboxAuthManager::exchangeAccessToken(const QString &accessToken, const QString &refreshToken)
{
    Q_UNUSED(refreshToken);
    exchangeXblUserToken(accessToken);
}

void XboxAuthManager::exchangeXblUserToken(const QString &accessToken)
{
    QUrl url(QStringLiteral("https://user.auth.xboxlive.com/user/authenticate"));
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setRawHeader("x-xbl-contract-version", "1");
    req.setRawHeader("Accept", "application/json");

    QJsonObject props;
    props[QStringLiteral("AuthMethod")] = QStringLiteral("RPS");
    props[QStringLiteral("RpsTicket")] = QStringLiteral("d=") + accessToken;
    props[QStringLiteral("SiteName")] = QStringLiteral("user.auth.xboxlive.com");

    QJsonObject payload;
    payload[QStringLiteral("Properties")] = props;
    payload[QStringLiteral("RelyingParty")] = QStringLiteral("http://auth.xboxlive.com");
    payload[QStringLiteral("TokenType")] = QStringLiteral("JWT");

    QByteArray body = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    QNetworkReply *reply = m_nam->post(req, body);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200) {
            QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            QJsonObject obj = doc.object();
            m_xblUserToken = obj.value(QStringLiteral("Token")).toString();

            QJsonObject displayClaims = obj.value(QStringLiteral("DisplayClaims")).toObject();
            QJsonArray xui = displayClaims.value(QStringLiteral("xui")).toArray();
            if (!xui.isEmpty()) {
                m_userHash = xui.first().toObject().value(QStringLiteral("uhs")).toString();
            }

            exchangeXstsTokens();
        } else {
            emit authFailed(tr("Xbox Live user authentication failed."));
        }
    });
}

void XboxAuthManager::exchangeXstsTokens()
{
    if (m_xblUserToken.isEmpty()) {
        emit authFailed(tr("Missing XBL user token."));
        return;
    }

    // 1. Exchange for Web XSTS Token (http://xboxlive.com) to retrieve Gamertag and list consoles
    QUrl webUrl(QStringLiteral("https://xsts.auth.xboxlive.com/xsts/authorize"));
    QNetworkRequest webReq(webUrl);
    webReq.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    webReq.setRawHeader("x-xbl-contract-version", "1");
    webReq.setRawHeader("Accept", "application/json");

    QJsonObject webProps;
    webProps[QStringLiteral("SandboxId")] = QStringLiteral("RETAIL");
    QJsonArray userTokens;
    userTokens.append(m_xblUserToken);
    webProps[QStringLiteral("UserTokens")] = userTokens;

    QJsonObject webPayload;
    webPayload[QStringLiteral("Properties")] = webProps;
    webPayload[QStringLiteral("RelyingParty")] = QStringLiteral("http://xboxlive.com");
    webPayload[QStringLiteral("TokenType")] = QStringLiteral("JWT");

    QByteArray webBody = QJsonDocument(webPayload).toJson(QJsonDocument::Compact);
    QNetworkReply *webReply = m_nam->post(webReq, webBody);

    connect(webReply, &QNetworkReply::finished, this, [this, webReply]() {
        webReply->deleteLater();
        if (webReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200) {
            QJsonDocument doc = QJsonDocument::fromJson(webReply->readAll());
            QJsonObject obj = doc.object();
            m_webToken = obj.value(QStringLiteral("Token")).toString();

            QJsonObject displayClaims = obj.value(QStringLiteral("DisplayClaims")).toObject();
            QJsonArray xui = displayClaims.value(QStringLiteral("xui")).toArray();
            if (!xui.isEmpty()) {
                m_gamertag = xui.first().toObject().value(QStringLiteral("gtg")).toString();
                if (m_userHash.isEmpty()) {
                    m_userHash = xui.first().toObject().value(QStringLiteral("uhs")).toString();
                }
            }

            // 2. Exchange for GSSV XSTS Token (http://gssv.xboxlive.com/)
            QUrl gssvUrl(QStringLiteral("https://xsts.auth.xboxlive.com/xsts/authorize"));
            QNetworkRequest gssvReq(gssvUrl);
            gssvReq.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
            gssvReq.setRawHeader("x-xbl-contract-version", "1");
            gssvReq.setRawHeader("Accept", "application/json");

            QJsonObject gssvProps;
            gssvProps[QStringLiteral("SandboxId")] = QStringLiteral("RETAIL");
            QJsonArray gssvUserTokens;
            gssvUserTokens.append(m_xblUserToken);
            gssvProps[QStringLiteral("UserTokens")] = gssvUserTokens;

            QJsonObject gssvPayload;
            gssvPayload[QStringLiteral("Properties")] = gssvProps;
            gssvPayload[QStringLiteral("RelyingParty")] = QStringLiteral("http://gssv.xboxlive.com/");
            gssvPayload[QStringLiteral("TokenType")] = QStringLiteral("JWT");

            QByteArray gssvBody = QJsonDocument(gssvPayload).toJson(QJsonDocument::Compact);
            QNetworkReply *gssvReply = m_nam->post(gssvReq, gssvBody);

            connect(gssvReply, &QNetworkReply::finished, this, [this, gssvReply]() {
                gssvReply->deleteLater();
                if (gssvReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200) {
                    QJsonDocument gdoc = QJsonDocument::fromJson(gssvReply->readAll());
                    m_gssvToken = gdoc.object().value(QStringLiteral("Token")).toString();

                    m_isLoggedIn = true;
                    emit loggedInChanged(true);
                    emit authenticated(m_gamertag, m_userHash);

                    fetchConsoles();
                } else {
                    emit authFailed(tr("Failed to obtain Xbox GSSV streaming token."));
                }
            });
        } else {
            emit authFailed(tr("Failed to obtain Xbox Live authorization."));
        }
    });
}

void XboxAuthManager::fetchConsoles()
{
    if (!m_isLoggedIn || m_webToken.isEmpty() || m_userHash.isEmpty()) {
        return;
    }

    QUrl url(QStringLiteral("https://xccs.xboxlive.com/lists/devices?queryCurrentDevice=false&includeStorageDevices=true"));
    QNetworkRequest req(url);
    QString authHeader = QStringLiteral("XBL3.0 x=") + m_userHash + QStringLiteral(";") + m_webToken;
    req.setRawHeader("Authorization", authHeader.toUtf8());
    req.setRawHeader("x-xbl-contract-version", "4");
    req.setRawHeader("skillplatform", "RemoteManagement");
    req.setRawHeader("Accept", "application/json");

    QNetworkReply *reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200) {
            QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            QJsonObject root = doc.object();
            QJsonArray result = root.value(QStringLiteral("result")).toArray();

            QJsonArray consoles;
            for (const auto &val : result) {
                QJsonObject dev = val.toObject();
                QJsonObject console;
                console[QStringLiteral("id")] = dev.value(QStringLiteral("id")).toString();
                console[QStringLiteral("name")] = dev.value(QStringLiteral("name")).toString();
                console[QStringLiteral("powerState")] = dev.value(QStringLiteral("powerState")).toString();
                console[QStringLiteral("consoleType")] = dev.value(QStringLiteral("consoleType")).toString();
                console[QStringLiteral("isOnline")] = (dev.value(QStringLiteral("powerState")).toString() == QStringLiteral("On") ||
                                                      dev.value(QStringLiteral("powerState")).toString() == QStringLiteral("ConnectedStandby"));
                consoles.append(console);
            }

            m_consoles = consoles;
            emit consolesUpdated(m_consoles);
        }
    });
}

void XboxAuthManager::wakeConsole(const QString &consoleId)
{
    if (!m_isLoggedIn || m_webToken.isEmpty() || m_userHash.isEmpty()) return;

    QUrl url(QStringLiteral("https://xccs.xboxlive.com/commands"));
    QNetworkRequest req(url);
    QString authHeader = QStringLiteral("XBL3.0 x=") + m_userHash + QStringLiteral(";") + m_webToken;
    req.setRawHeader("Authorization", authHeader.toUtf8());
    req.setRawHeader("x-xbl-contract-version", "4");
    req.setRawHeader("skillplatform", "RemoteManagement");
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

    QJsonObject payload;
    payload[QStringLiteral("destination")] = QStringLiteral("Xbox");
    payload[QStringLiteral("type")] = QStringLiteral("Power");
    payload[QStringLiteral("command")] = QStringLiteral("WakeUp");
    payload[QStringLiteral("sessionId")] = QUuid::createUuid().toString(QUuid::WithoutBraces);
    payload[QStringLiteral("sourceId")] = QStringLiteral("com.microsoft.smartglass");
    payload[QStringLiteral("parameters")] = QJsonArray();
    payload[QStringLiteral("linkedXboxId")] = consoleId;

    QByteArray body = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    QNetworkReply *reply = m_nam->post(req, body);
    connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);
}

void XboxAuthManager::turnOffConsole(const QString &consoleId)
{
    if (!m_isLoggedIn || m_webToken.isEmpty() || m_userHash.isEmpty()) return;

    QUrl url(QStringLiteral("https://xccs.xboxlive.com/commands"));
    QNetworkRequest req(url);
    QString authHeader = QStringLiteral("XBL3.0 x=") + m_userHash + QStringLiteral(";") + m_webToken;
    req.setRawHeader("Authorization", authHeader.toUtf8());
    req.setRawHeader("x-xbl-contract-version", "4");
    req.setRawHeader("skillplatform", "RemoteManagement");
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

    QJsonObject payload;
    payload[QStringLiteral("destination")] = QStringLiteral("Xbox");
    payload[QStringLiteral("type")] = QStringLiteral("Power");
    payload[QStringLiteral("command")] = QStringLiteral("TurnOff");
    payload[QStringLiteral("sessionId")] = QUuid::createUuid().toString(QUuid::WithoutBraces);
    payload[QStringLiteral("sourceId")] = QStringLiteral("com.microsoft.smartglass");
    payload[QStringLiteral("parameters")] = QJsonArray();
    payload[QStringLiteral("linkedXboxId")] = consoleId;

    QByteArray body = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    QNetworkReply *reply = m_nam->post(req, body);
    connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);
}

void XboxAuthManager::getStreamingToken(const QString &offeringId, std::function<void(bool success, const QString &token, const QString &host)> callback)
{
    if (!m_isLoggedIn || m_gssvToken.isEmpty()) {
        callback(false, QString(), QString());
        return;
    }

    QString hostName = offeringId + QStringLiteral(".gssv-play-prod.xboxlive.com");
    QUrl url(QStringLiteral("https://") + hostName + QStringLiteral("/v2/login/user"));
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setRawHeader("x-gssv-client", "XboxComBrowser");
    req.setRawHeader("Accept", "application/json");

    QJsonObject payload;
    payload[QStringLiteral("token")] = m_gssvToken;
    payload[QStringLiteral("offeringId")] = offeringId;

    QByteArray body = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    QNetworkReply *reply = m_nam->post(req, body);

    connect(reply, &QNetworkReply::finished, this, [reply, callback, hostName]() {
        reply->deleteLater();
        if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200) {
            QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            QJsonObject obj = doc.object();
            QString gsToken = obj.value(QStringLiteral("gsToken")).toString();
            callback(!gsToken.isEmpty(), gsToken, hostName);
        } else {
            callback(false, QString(), QString());
        }
    });
}

void XboxAuthManager::logout()
{
    cancelLogin();
    m_isLoggedIn = false;
    m_gamertag.clear();
    m_userHash.clear();
    m_accessToken.clear();
    m_refreshToken.clear();
    m_xblUserToken.clear();
    m_gssvToken.clear();
    m_webToken.clear();
    m_consoles = QJsonArray();

    CredentialVault::deleteSecret(QStringLiteral("xbox_refresh_token"));
    CredentialVault::deleteSecret(QStringLiteral("xbox_pop_key"));

    emit loggedInChanged(false);
    emit loggedOut();
    emit consolesUpdated(m_consoles);
}
