#include "embeddedrelayserver.h"

#include <QMetaObject>
#include <QtTest>

class EmbeddedRelayServerBdd final : public QObject
{
    Q_OBJECT

private slots:
    void code_is_normalized_and_consumed_once();
    void expired_codes_are_rejected_by_every_operation();
    void server_url_returns_valid_http_format();
    void fragmented_http_post_is_reassembled_and_handled();
    void search_endpoint_handles_missing_username_and_mock_id();
};

void EmbeddedRelayServerBdd::code_is_normalized_and_consumed_once()
{
    qint64 now = 1000;
    EmbeddedRelayServer relay(nullptr, [&now] { return now; });

    QVERIFY(relay.createCode(QStringLiteral("  aBc123 \n")));
    QVERIFY(relay.validateCode(QStringLiteral("ABC123")));
    QString token;
    QVERIFY(!relay.getTokens(QStringLiteral("ABC123"), token));
    QVERIFY(relay.setTokens(QStringLiteral("abc123"), QStringLiteral(" secret ")));

    QVERIFY(relay.getTokens(QStringLiteral(" AbC123 "), token));
    QCOMPARE(token, QStringLiteral("secret"));
    QVERIFY(!relay.validateCode(QStringLiteral("ABC123")));
    QVERIFY(!relay.getTokens(QStringLiteral("ABC123"), token));
}

void EmbeddedRelayServerBdd::expired_codes_are_rejected_by_every_operation()
{
    qint64 now = 1000;
    EmbeddedRelayServer relay(nullptr, [&now] { return now; });
    QVERIFY(relay.createCode(QStringLiteral("EXPIRE1")));
    now += 15LL * 60LL * 1000LL;

    QVERIFY(!relay.validateCode(QStringLiteral("EXPIRE1")));
    QVERIFY(!relay.setTokens(QStringLiteral("EXPIRE1"), QStringLiteral("secret")));
    QString token;
    QVERIFY(!relay.getTokens(QStringLiteral("EXPIRE1"), token));
    QVERIFY(QMetaObject::invokeMethod(&relay, "cleanupExpiredCodes"));
}

void EmbeddedRelayServerBdd::server_url_returns_valid_http_format()
{
    EmbeddedRelayServer relay(nullptr);
    QString url = relay.serverUrl();
    QVERIFY(url.startsWith(QStringLiteral("http://")));
    QVERIFY(!url.contains(QStringLiteral("192.168.240.")));
    QVERIFY(!url.contains(QStringLiteral("192.168.245.")));
    QVERIFY(!url.contains(QStringLiteral("192.168.250.")));
    QVERIFY(!url.contains(QStringLiteral("10.0.2.")));
    QVERIFY(!url.contains(QStringLiteral("10.0.3.")));
    QVERIFY(!url.contains(QStringLiteral("192.168.56.")));
    QVERIFY(!url.contains(QStringLiteral("192.168.232.")));
    QVERIFY(!url.contains(QStringLiteral("192.168.122.")));
}

void EmbeddedRelayServerBdd::fragmented_http_post_is_reassembled_and_handled()
{
    EmbeddedRelayServer relay(nullptr);
    QVERIFY(relay.start(0));
    quint16 port = relay.port();
    QVERIFY(port > 0);

    QTcpSocket client;
    client.connectToHost(QStringLiteral("127.0.0.1"), port);
    QVERIFY(client.waitForConnected(2000));

    QByteArray body = "{\"code\":\"FRAG01\"}";
    QByteArray headers = QString("POST /astraplay/create-code HTTP/1.1\r\n"
                                 "Host: 127.0.0.1\r\n"
                                 "Content-Type: application/json\r\n"
                                 "Content-Length: %1\r\n\r\n").arg(body.size()).toUtf8();

    // Send packet 1: Headers only
    client.write(headers);
    client.flush();
    QTest::qWait(40);

    // Send packet 2: First fragment of body
    client.write(body.left(body.size() / 2));
    client.flush();
    QTest::qWait(40);

    // Send packet 3: Second fragment of body
    client.write(body.mid(body.size() / 2));
    client.flush();

    // Await server response (run event loop so server socket processes incoming chunks)
    QByteArray response;
    QTRY_VERIFY_WITH_TIMEOUT(client.bytesAvailable() > 0, 3000);
    response = client.readAll();

    QVERIFY(!response.isEmpty());
    QVERIFY(response.startsWith("HTTP/1.1 200 OK"));
    QVERIFY(response.contains("\"result\":\"success\""));
    QVERIFY(relay.validateCode(QStringLiteral("FRAG01")));
}

void EmbeddedRelayServerBdd::search_endpoint_handles_missing_username_and_mock_id()
{
    EmbeddedRelayServer relay(nullptr);
    QVERIFY(relay.start(0));
    quint16 port = relay.port();
    QVERIFY(port > 0);

    // 1. Missing username request
    {
        QTcpSocket client;
        client.connectToHost(QStringLiteral("127.0.0.1"), port);
        QVERIFY(client.waitForConnected(2000));
        client.write("GET /search.php HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n");
        client.flush();
        QTRY_VERIFY_WITH_TIMEOUT(client.bytesAvailable() > 0, 3000);
        QByteArray resp = client.readAll();
        QVERIFY(resp.startsWith("HTTP/1.1 200 OK"));
        QVERIFY(resp.contains("\"error\":\"Username required\""));
    }

    // 2. Direct lookup without mock ID explains requirement
    {
        QTcpSocket client;
        client.connectToHost(QStringLiteral("127.0.0.1"), port);
        QVERIFY(client.waitForConnected(2000));
        client.write("GET /search.php?username=Alice HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n");
        client.flush();
        QTRY_VERIFY_WITH_TIMEOUT(client.bytesAvailable() > 0, 3000);
        QByteArray resp = client.readAll();
        QVERIFY(resp.startsWith("HTTP/1.1 200 OK"));
        QVERIFY(resp.contains("Lookup disabled"));
    }

    // 3. Mock ID lookup provides encoded_id response
    {
        QTcpSocket client;
        client.connectToHost(QStringLiteral("127.0.0.1"), port);
        QVERIFY(client.waitForConnected(2000));
        client.write("GET /search.php?username=Alice&mock_id=bW9ja0lkMTIz HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n");
        client.flush();
        QTRY_VERIFY_WITH_TIMEOUT(client.bytesAvailable() > 0, 3000);
        QByteArray resp = client.readAll();
        QVERIFY(resp.startsWith("HTTP/1.1 200 OK"));
        QVERIFY(resp.contains("\"encoded_id\":\"bW9ja0lkMTIz\""));
        QVERIFY(resp.contains("\"username\":\"Alice\""));
    }
}

QTEST_GUILESS_MAIN(EmbeddedRelayServerBdd)
#include "embeddedrelayserver_bdd.moc"
