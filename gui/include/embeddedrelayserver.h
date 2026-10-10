// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
#pragma once

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QMap>
#include <QDateTime>
#include <QTimer>
#include <QHostAddress>

struct RelayCodeEntry {
    QString npsso;
    qint64 createdAt;
};

class EmbeddedRelayServer : public QObject
{
    Q_OBJECT

public:
    explicit EmbeddedRelayServer(QObject *parent = nullptr);
    ~EmbeddedRelayServer() override;

    bool start(quint16 port = 8080);
    void stop();

    bool isRunning() const { return m_server && m_server->isListening(); }
    quint16 port() const { return m_port; }
    QString serverUrl() const;

    // Direct C++ API for QmlBackend to use directly without loopback network latency
    bool createCode(const QString &code);
    bool getTokens(const QString &code, QString &outNpsso);
    bool validateCode(const QString &code);
    bool setTokens(const QString &code, const QString &npsso);

private slots:
    void onNewConnection();
    void onClientReadyRead();
    void cleanupExpiredCodes();

private:
    void handleHttpRequest(QTcpSocket *socket, const QByteArray &requestData);
    void sendResponse(QTcpSocket *socket, int statusCode, const QString &statusText,
                      const QByteArray &contentType, const QByteArray &body);
    void sendJsonResponse(QTcpSocket *socket, int statusCode, const QJsonObject &json);

    QTcpServer *m_server = nullptr;
    quint16 m_port = 8080;
    QMap<QString, RelayCodeEntry> m_codes;
    QTimer *m_cleanupTimer = nullptr;
};
