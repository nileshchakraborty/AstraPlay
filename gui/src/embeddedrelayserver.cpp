// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
#include "embeddedrelayserver.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QUrlQuery>
#include <QFile>
#include <QNetworkInterface>
#include <QUdpSocket>
#include <QDebug>

constexpr qint64 RELAY_CODE_TTL_MS = 15LL * 60LL * 1000LL; // 15 minutes

static bool isVirtualSubnet(const QString &ipStr)
{
    return ipStr.startsWith(QStringLiteral("10.0.2.")) ||       // QEMU / Android emulator NAT
           ipStr.startsWith(QStringLiteral("10.0.3.")) ||       // BlueStacks / Genymotion NAT
           ipStr.startsWith(QStringLiteral("192.168.56.")) ||   // VirtualBox Host-Only default
           ipStr.startsWith(QStringLiteral("192.168.232.")) ||  // BlueStacks alternate NAT
           ipStr.startsWith(QStringLiteral("192.168.240.")) ||  // Waydroid default bridge
           ipStr.startsWith(QStringLiteral("192.168.245.")) ||  // Waydroid alternate bridge
           ipStr.startsWith(QStringLiteral("192.168.250.")) ||  // Waydroid alternate bridge
           ipStr.startsWith(QStringLiteral("192.168.122.")) ||  // KVM / libvirt default bridge
           ipStr.startsWith(QStringLiteral("172.17.")) ||       // Docker default bridge
           ipStr.startsWith(QStringLiteral("172.18.")) ||       // Docker default bridge
           ipStr.startsWith(QStringLiteral("172.19."));        // Docker default bridge
}

static bool isVirtualInterfaceName(const QString &rawName)
{
    QString name = rawName.toLower();
    return name.contains(QStringLiteral("docker")) || name.contains(QStringLiteral("veth")) ||
           name.contains(QStringLiteral("tun")) || name.contains(QStringLiteral("tap")) ||
           name.contains(QStringLiteral("bridge")) || name.contains(QStringLiteral("utun")) ||
           name.contains(QStringLiteral("waydroid")) || name.contains(QStringLiteral("virbr")) ||
           name.contains(QStringLiteral("vboxnet")) || name.contains(QStringLiteral("virtualbox")) ||
           name.contains(QStringLiteral("vbox")) || name.contains(QStringLiteral("bluestacks")) ||
           name.contains(QStringLiteral("bstk")) || name.contains(QStringLiteral("nox")) ||
           name.contains(QStringLiteral("ldplayer")) || name.contains(QStringLiteral("memu")) ||
           name.contains(QStringLiteral("vethernet")) || name.contains(QStringLiteral("hyper-v")) ||
           name.contains(QStringLiteral("wsl")) || name.contains(QStringLiteral("lxc")) ||
           name.contains(QStringLiteral("anbox")) || name.contains(QStringLiteral("vmnet")) ||
           name.contains(QStringLiteral("vmware")) || name.contains(QStringLiteral("tailscale")) ||
           name.contains(QStringLiteral("wg")) || name.contains(QStringLiteral("zerotier"));
}

EmbeddedRelayServer::EmbeddedRelayServer(QObject *parent, std::function<qint64()> now)
    : QObject(parent)
    , m_server(new QTcpServer(this))
    , m_cleanupTimer(new QTimer(this))
    , m_now(now ? std::move(now) : [] { return QDateTime::currentMSecsSinceEpoch(); })
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
    // Bind to all network interfaces so other devices (phone, laptop) on LAN can access http://<local-ip>:25525/psstream/
    if (!m_server->listen(QHostAddress::Any, m_port)) {
        // If preferred port is taken, try port 0 (OS assigns an available port)
        qWarning() << "[EmbeddedRelayServer] Failed to listen on port" << m_port << ":" << m_server->errorString();
        if (!m_server->listen(QHostAddress::Any, 0)) {
            qCritical() << "[EmbeddedRelayServer] Failed to start on any port:" << m_server->errorString();
            return false;
        }
    }
    m_port = m_server->serverPort();

    m_cleanupTimer->start();
    qInfo() << "[EmbeddedRelayServer] Standalone PSN Relay Server listening on port" << m_port;
    qInfo() << "[EmbeddedRelayServer] Local access URL:" << serverUrl() + "/psstream/";
    return true;
}

void EmbeddedRelayServer::stop()
{
    if (m_cleanupTimer->isActive())
        m_cleanupTimer->stop();

    m_socketBuffers.clear();

    if (m_server && m_server->isListening()) {
        m_server->close();
        qInfo() << "[EmbeddedRelayServer] Server stopped.";
    }
}

QString EmbeddedRelayServer::serverUrl() const
{
    // 1. Try kernel outbound routing probe via a zero-traffic connectionless UDP socket connect.
    // The OS routing table automatically selects the physical adapter used for default gateway/outbound
    // traffic, bypassing virtual bridges (BlueStacks, Waydroid, VirtualBox, Hyper-V, Docker, WSL, VMware).
    {
        QUdpSocket probeSocket;
        probeSocket.connectToHost(QHostAddress(QStringLiteral("8.8.8.8")), 10002);
        QHostAddress routeAddr = probeSocket.localAddress();
        if (!routeAddr.isNull() && routeAddr.protocol() == QAbstractSocket::IPv4Protocol && !routeAddr.isLoopback()) {
            QString routeIp = routeAddr.toString();
            bool isPrivateLan = routeIp.startsWith(QStringLiteral("192.168.")) ||
                                routeIp.startsWith(QStringLiteral("10.")) ||
                                routeIp.startsWith(QStringLiteral("172."));
            if (isPrivateLan && !isVirtualSubnet(routeIp)) {
                return QStringLiteral("http://%1:%2").arg(routeIp).arg(m_port);
            }
        }
    }

    // 2. Fallback to physical LAN interface enumeration (checking both system name and human-readable name)
    QString localIp;
    QString physicalLanIp;
    QString genericLanIp;

    const QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &iface : interfaces) {
        if (!(iface.flags() & QNetworkInterface::IsUp) || !(iface.flags() & QNetworkInterface::IsRunning))
            continue;
        if (iface.flags() & QNetworkInterface::IsLoopBack)
            continue;

        if (isVirtualInterfaceName(iface.name()) || isVirtualInterfaceName(iface.humanReadableName()))
            continue;

        QString name = iface.name().toLower();
        QString friendly = iface.humanReadableName().toLower();
        bool isPhysicalAdapter = name.startsWith("wl") || name.startsWith("wifi") ||
                                 name.startsWith("en") || name.startsWith("eth") ||
                                 name.startsWith("lan") || friendly.contains("wi-fi") ||
                                 friendly.contains("ethernet") || friendly.contains("wlan");

        const QList<QNetworkAddressEntry> entries = iface.addressEntries();
        for (const QNetworkAddressEntry &entry : entries) {
            QHostAddress addr = entry.ip();
            if (addr.protocol() == QAbstractSocket::IPv4Protocol && !addr.isLoopback()) {
                QString ipStr = addr.toString();
                if (isVirtualSubnet(ipStr))
                    continue;

                // Standard private LAN subnets: 192.168.x.x, 10.x.x.x, 172.16-31.x.x
                bool isPrivateLan = ipStr.startsWith("192.168.") || ipStr.startsWith("10.") || ipStr.startsWith("172.");
                if (isPhysicalAdapter && isPrivateLan) {
                    if (physicalLanIp.isEmpty()) {
                        physicalLanIp = ipStr;
                    }
                } else if (isPrivateLan) {
                    if (genericLanIp.isEmpty()) {
                        genericLanIp = ipStr;
                    }
                }

                if (localIp.isEmpty()) {
                    localIp = ipStr;
                }
            }
        }
    }

    if (!physicalLanIp.isEmpty()) {
        return QStringLiteral("http://%1:%2").arg(physicalLanIp).arg(m_port);
    }
    if (!genericLanIp.isEmpty()) {
        return QStringLiteral("http://%1:%2").arg(genericLanIp).arg(m_port);
    }
    if (localIp.isEmpty()) {
        localIp = QStringLiteral("127.0.0.1");
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
    entry.createdAt = m_now();
    m_codes.insert(trimmed, entry);
    qDebug() << "[EmbeddedRelayServer] Registered login code";
    return true;
}

bool EmbeddedRelayServer::getTokens(const QString &code, QString &outNpsso)
{
    QString trimmed = code.trimmed().toUpper();
    RelayCodeEntry *entry = activeCode(trimmed);
    if (!entry)
        return false;

    if (entry->npsso.isEmpty())
        return false;

    outNpsso = entry->npsso;
    m_codes.remove(trimmed); // One-time consumption
    qDebug() << "[EmbeddedRelayServer] Retrieved one-time login token";
    return true;
}

bool EmbeddedRelayServer::validateCode(const QString &code)
{
    QString trimmed = code.trimmed().toUpper();
    return activeCode(trimmed) != nullptr;
}

bool EmbeddedRelayServer::setTokens(const QString &code, const QString &npsso)
{
    QString trimmed = code.trimmed().toUpper();
    RelayCodeEntry *entry = activeCode(trimmed);
    if (!entry || npsso.trimmed().isEmpty())
        return false;

    entry->npsso = npsso.trimmed();
    qDebug() << "[EmbeddedRelayServer] Stored login token";
    return true;
}

void EmbeddedRelayServer::onNewConnection()
{
    while (m_server->hasPendingConnections()) {
        QTcpSocket *socket = m_server->nextPendingConnection();
        connect(socket, &QTcpSocket::readyRead, this, &EmbeddedRelayServer::onClientReadyRead);
        connect(socket, &QTcpSocket::disconnected, this, [this, socket]() {
            m_socketBuffers.remove(socket);
            socket->deleteLater();
        });
    }
}

void EmbeddedRelayServer::onClientReadyRead()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
    if (!socket)
        return;

    QByteArray &buf = m_socketBuffers[socket];
    buf.append(socket->readAll());

    while (!buf.isEmpty()) {
        int headerEnd = buf.indexOf("\r\n\r\n");
        if (headerEnd == -1) {
            // Check if buffer exceeded max allowed header size (64 KB)
            if (buf.size() > 64 * 1024) {
                sendResponse(socket, 400, "Bad Request", "text/plain", "Header Too Large");
                m_socketBuffers.remove(socket);
                socket->close();
            }
            return;
        }

        // Parse Content-Length if present
        QByteArray headers = buf.left(headerEnd);
        int contentLength = 0;
        int clPos = headers.indexOf("Content-Length:");
        if (clPos == -1)
            clPos = headers.indexOf("content-length:");
        if (clPos != -1) {
            int lineEnd = headers.indexOf("\r\n", clPos);
            QByteArray clLine = (lineEnd != -1) ? headers.mid(clPos, lineEnd - clPos) : headers.mid(clPos);
            int colon = clLine.indexOf(':');
            if (colon != -1) {
                contentLength = clLine.mid(colon + 1).trimmed().toInt();
            }
        }

        int totalExpected = headerEnd + 4 + contentLength;
        if (buf.size() < totalExpected) {
            // Incomplete body, await more TCP packets
            return;
        }

        QByteArray requestData = buf.left(totalExpected);
        buf.remove(0, totalExpected);

        handleHttpRequest(socket, requestData);
    }
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

    // --- API: /psstream/create-code or /astraplay/create-code ---
    if ((path == "/psstream/create-code" || path == "/astraplay/create-code") && method == "POST") {
        QJsonObject json = QJsonDocument::fromJson(body).object();
        QString code = json.value("code").toString();
        if (createCode(code)) {
            QJsonObject resp;
            resp["result"] = "success";
            resp["code"] = code.trimmed().toUpper();
            sendJsonResponse(socket, 200, resp);
        } else {
            QJsonObject resp;
            resp["result"] = "error";
            resp["error"] = "Invalid code";
            sendJsonResponse(socket, 400, resp);
        }
        return;
    }

    // --- API: /psstream/get-tokens or /astraplay/get-tokens ---
    if ((path == "/psstream/get-tokens" || path == "/astraplay/get-tokens") && method == "POST") {
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

    // --- API: /astraplay/validate-code or /psstream/validate-code ---
    if ((path == "/astraplay/validate-code" || path == "/psstream/validate-code") && method == "POST") {
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

    // --- API: /astraplay/connect or /psstream/connect ---
    if ((path == "/astraplay/connect" || path == "/psstream/connect") && method == "POST") {
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
        QUrlQuery query(parsedUrl);
        QString username = query.queryItemValue("username");
        QString mockId = query.queryItemValue("mock_id");
        QJsonObject resp;
        if (username.isEmpty()) {
            resp["error"] = "Username required";
        } else if (!mockId.isEmpty()) {
            resp["username"] = username;
            resp["encoded_id"] = mockId;
        } else {
            resp["error"] = "Lookup disabled. Please use the direct PSN Login on this device.";
        }
        sendJsonResponse(socket, 200, resp);
        return;
    }

    // --- Static files from embedded Qt resources ---
    if (path == "/" || path == "/psstream" || path == "/psstream/" || path == "/astraplay" || path == "/astraplay/" || path == "/index.html") {
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
    header.append("Server: AstraPlay-Embedded-Relay/1.0\r\n");
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
    const qint64 now = m_now();
    for (auto it = m_codes.begin(); it != m_codes.end();) {
        if (now - it.value().createdAt >= RELAY_CODE_TTL_MS) {
            qDebug() << "[EmbeddedRelayServer] Expiring stale login code";
            it = m_codes.erase(it);
        } else {
            ++it;
        }
    }
}

RelayCodeEntry *EmbeddedRelayServer::activeCode(const QString &normalizedCode)
{
    auto it = m_codes.find(normalizedCode);
    if (it == m_codes.end())
        return nullptr;
    if (m_now() - it.value().createdAt >= RELAY_CODE_TTL_MS) {
        m_codes.erase(it);
        return nullptr;
    }
    return &it.value();
}
