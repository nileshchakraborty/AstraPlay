#include "embeddedrelayserver.h"

#include <QMetaObject>
#include <QtTest>

class EmbeddedRelayServerBdd final : public QObject
{
    Q_OBJECT

private slots:
    void code_is_normalized_and_consumed_once();
    void expired_codes_are_rejected_by_every_operation();
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

QTEST_GUILESS_MAIN(EmbeddedRelayServerBdd)
#include "embeddedrelayserver_bdd.moc"
