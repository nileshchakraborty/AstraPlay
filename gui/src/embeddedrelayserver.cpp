// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
#include "embeddedrelayserver.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QUrlQuery>
#include <QFile>
#include <QNetworkInterface>
#include <QDebug>

constexpr qint64 RELAY_CODE_TTL_MS = 15LL * 60LL * 1000LL; // 15 minutes

EmbeddedRelayServer::EmbeddedRelayServer(QObject *parent)
    : QObject(parent)
    , m_server(new QTcpServer(this))
    , m_cleanupTimer(new QTimer(this))
{
    connect(m_server, &QTcpServer::newConnection, this, &EmbeddedRelayServer::onNewConnection);

    m_cleanupTimer->setInterval(60 * 1000); // 1 minute
    connect(m_cleanupTimer, &QTimer::timeout, this, &EmbeddedRelayServer::cleanupExpiredCodes);
}

EmbeddedRelayServer::~EmbeddedRelayServer()
{
    stop();
}

bool EmbeddedRelayServer::start(quint16 port)
{
    if (isRunning())
        return true;

    m_port = port;
    // Bind to all network interfaces so other devices (phone, laptop) on LAN can access http://<local-ip>:8080/psstream/
    if (!m_server->listen(QHostAddress::Any, m_port)) {
        // If preferred port is taken, try port 0 (OS assigns an available port)
        qWarning() << "[EmbeddedRelayServer] Failed to listen on port" << m_port << ":" << m_server->errorString();
        if (!m_server->listen(QHostAddress::Any, 0)) {
            qCritical() << "[EmbeddedRelayServer] Failed to start on any port:" << m_server->errorString();
            return false;
        }
        m_port = m_server->serverPort();
    }

    m_cleanupTimer->start();
    qInfo() << "[EmbeddedRelayServer] Standalone PSN Relay Server listening on port" << m_port;
    qInfo() << "[EmbeddedRelayServer] Local access URL:" << serverUrl() + "/psstream/";
    return true;
}

void EmbeddedRelayServer::stop()
{
    if (m_cleanupTimer->isActive())
        m_cleanupTimer->stop();

    if (m_server && m_server->isListening()) {
        m_server->close();
        qInfo() << "[EmbeddedRelayServer] Server stopped.";
    }
}

QString EmbeddedRelayServer::serverUrl() const
{
    // Try to find local LAN IP for display/QR code
    QString localIp = QStringLiteral("127.0.0.1");
    const QList<QHostAddress> ipAddressesList = QNetworkInterface::allAddresses();
    for (const QHostAddress &address : ipAddressesList) {
        if (address != QHostAddress::LocalHost && address.toIPv4Address()) {
            localIp = address.toString();
            break;
        }
    }
    return QStringLiteral("http://%1:%2").arg(localIp).arg(m_port);
}

bool EmbeddedRelayServer::createCode(const QString &code)
{
    QString trimmed = code.trimmed().toUpper();
    if (trimmed.isEmpty())
        return false;

    RelayCodeEntry entry;
    entry.npsso = QString();
    entry.createdAt = QDateTime::currentMSecsSinceEpoch();
    m_codes.insert(trimmed, entry);
    qDebug() << "[EmbeddedRelayServer] Registered code:" << trimmed;
    return true;
}

bool EmbeddedRelayServer::getTokens(const QString &code, QString &outNpsso)
{
    QString trimmed = code.trimmed().toUpper();
    if (!m_codes.contains(trimmed))
        return false;

    RelayCodeEntry entry = m_codes.value(trimmed);
    if (entry.npsso.isEmpty())
        return false;

    outNpsso = entry.npsso;
    m_codes.remove(trimmed); // One-time consumption
    qDebug() << "[EmbeddedRelayServer] Token retrieved for code:" << trimmed;
    return true;
}

bool EmbeddedRelayServer::validateCode(const QString &code)
{
    QString trimmed = code.trimmed().toUpper();
    return m_codes.contains(trimmed);
}

bool EmbeddedRelayServer::setTokens(const QString &code, const QString &npsso)
{
    QString trimmed = code.trimmed().toUpper();
    if (!m_codes.contains(trimmed) || npsso.trimmed().isEmpty())
        return false;

    RelayCodeEntry entry = m_codes.value(trimmed);
    entry.npsso = npsso.trimmed();
    m_codes.insert(trimmed, entry);
    qDebug() << "[EmbeddedRelayServer] Stored NPSSO token for code:" << trimmed;
    return true;
}

void EmbeddedRelayServer::onNewConnection()
{
    while (m_server->hasPendingConnections()) {
        QTcpSocket *socket = m_server->nextPendingConnection();
        connect(socket, &QTcpSocket::readyRead, this, &EmbeddedRelayServer::onClientReadyRead);
        connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);
    }
}

void EmbeddedRelayServer::onClientReadyRead()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
    if (!socket)
        return;

    QByteArray requestData = socket->readAll();
    handleHttpRequest(socket, requestData);
}

void EmbeddedRelayServer::handleHttpRequest(QTcpSocket *socket, const QByteArray &requestData)
{
    int lineEnd = requestData.indexOf("\r\n");
    if (lineEnd == -1) {
        sendResponse(socket, 400, "Bad Request", "text/plain", "Bad Request");
        return;
    }

    QByteArray requestLine = requestData.left(lineEnd);
    QList<QByteArray> parts = requestLine.split(' ');
    if (parts.size() < 2) {
        sendResponse(socket, 400, "Bad Request", "text/plain", "Bad Request");
        return;
    }

    QByteArray method = parts[0].toUpper();
    QUrl parsedUrl(QString::fromUtf8(parts[1]));
    QString path = parsedUrl.path();

    // CORS pre-flight
    if (method == "OPTIONS") {
        sendResponse(socket, 204, "No Content", "text/plain", "");
        return;
    }

    // Extract body if POST
    QByteArray body;
    int bodyStart = requestData.indexOf("\r\n\r\n");
    if (bodyStart != -1) {
        body = requestData.mid(bodyStart + 4);
    }

    // --- API: /psstream/create-code or /pylux/create-code ---
    if ((path == "/psstream/create-code" || path == "/pylux/create-code") && method == "POST") {
        QJsonObject json = QJsonDocument::fromJson(body).object();
        QString code = json.value("code").toString();
        if (createCode(code)) {
            QJsonObject resp;
            resp["result"] = "success";
            resp["code"] = code.toUpper();
            sendJsonResponse(socket, 200, resp);
        } else {
            QJsonObject resp;
            resp["result"] = "error";
            resp["error"] = "Invalid code";
            sendJsonResponse(socket, 400, resp);
        }
        return;
    }

    // --- API: /psstream/get-tokens or /pylux/get-tokens ---
    if ((path == "/psstream/get-tokens" || path == "/pylux/get-tokens") && method == "POST") {
        QJsonObject json = QJsonDocument::fromJson(body).object();
        QString code = json.value("code").toString();
        QString npsso;
        if (getTokens(code, npsso)) {
            QJsonObject resp;
            resp["result"] = "success";
            resp["npsso"] = npsso;
            sendJsonResponse(socket, 200, resp);
        } else {
            QJsonObject resp;
            resp["result"] = "error";
            resp["error"] = "Token not ready yet or invalid code";
            sendJsonResponse(socket, 404, resp);
        }
        return;
    }

    // --- API: /pylux/validate-code or /psstream/validate-code ---
    if ((path == "/pylux/validate-code" || path == "/psstream/validate-code") && method == "POST") {
        QJsonObject json = QJsonDocument::fromJson(body).object();
        QString code = json.value("code").toString();
        if (validateCode(code)) {
            QJsonObject resp;
            resp["result"] = "valid";
            sendJsonResponse(socket, 200, resp);
        } else {
            QJsonObject resp;
            resp["result"] = "invalid";
            resp["error"] = "Invalid or expired code";
            sendJsonResponse(socket, 404, resp);
        }
        return;
    }

    // --- API: /pylux/connect or /psstream/connect ---
    if ((path == "/pylux/connect" || path == "/psstream/connect") && method == "POST") {
        QJsonObject json = QJsonDocument::fromJson(body).object();
        QString code = json.value("code").toString();
        QString npsso = json.value("npsso").toString();
        if (setTokens(code, npsso)) {
            QJsonObject resp;
            resp["result"] = "success";
            sendJsonResponse(socket, 200, resp);
        } else {
            QJsonObject resp;
            resp["result"] = "error";
            resp["error"] = "Invalid code or expired";
            sendJsonResponse(socket, 404, resp);
        }
        return;
    }

    // --- API: Local search.php helper ---
    if (path == "/search.php" && method == "GET") {
        QJsonObject resp;
        resp["error"] = "Lookup disabled. Please use the direct PSN Login on this device.";
        sendJsonResponse(socket, 200, resp);
        return;
    }

    // --- Static files from embedded Qt resources ---
    if (path == "/" || path == "/psstream" || path == "/psstream/" || path == "/pylux" || path == "/pylux/" || path == "/index.html") {
        QFile file(":/relay/index.html");
        if (file.open(QIODevice::ReadOnly)) {
            sendResponse(socket, 200, "OK", "text/html; charset=utf-8", file.readAll());
            return;
        }
    } else if (path.endsWith("styles.css")) {
        QFile file(":/relay/styles.css");
        if (file.open(QIODevice::ReadOnly)) {
            sendResponse(socket, 200, "OK", "text/css; charset=utf-8", file.readAll());
            return;
        }
    } else if (path.endsWith("script.js")) {
        QFile file(":/relay/script.js");
        if (file.open(QIODevice::ReadOnly)) {
            sendResponse(socket, 200, "OK", "application/javascript; charset=utf-8", file.readAll());
            return;
        }
    }

    sendResponse(socket, 404, "Not Found", "text/plain", "Not Found");
}

void EmbeddedRelayServer::sendResponse(QTcpSocket *socket, int statusCode, const QString &statusText,
                                       const QByteArray &contentType, const QByteArray &body)
{
    QByteArray header;
    header.append(QString("HTTP/1.1 %1 %2\r\n").arg(statusCode).arg(statusText).toUtf8());
    header.append("Server: Pylux-Embedded-Relay/1.0\r\n");
    header.append(QString("Content-Type: %1\r\n").arg(QString::fromUtf8(contentType)).toUtf8());
    header.append(QString("Content-Length: %1\r\n").arg(body.size()).toUtf8());
    header.append("Access-Control-Allow-Origin: *\r\n");
    header.append("Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n");
    header.append("Access-Control-Allow-Headers: Content-Type\r\n");
    header.append("Connection: close\r\n\r\n");

    socket->write(header);
    socket->write(body);
    socket->flush();
    socket->disconnectFromHost();
}

void EmbeddedRelayServer::sendJsonResponse(QTcpSocket *socket, int statusCode, const QJsonObject &json)
{
    QJsonDocument doc(json);
    QByteArray body = doc.toJson(QJsonDocument::Compact);
    sendResponse(socket, statusCode, (statusCode == 200 ? "OK" : "Error"), "application/json", body);
}

void EmbeddedRelayServer::cleanupExpiredCodes()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    for (auto it = m_codes.begin(); it != m_codes.end();) {
        if (now - it.value().createdAt > RELAY_CODE_TTL_MS) {
            qDebug() << "[EmbeddedRelayServer] Expiring stale code:" << it.key();
            it = m_codes.erase(it);
        } else {
            ++it;
        }
    }
}
