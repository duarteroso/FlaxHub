#include "enginecatalog.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

class EngineCatalogTest : public QObject
{
    Q_OBJECT

private slots:
    void parsesTagNames();
    void rejectsBodyThatIsNotATagList();
    void pageOfHundredIsNotTheLast();
};

namespace {

QByteArray tagList(const QStringList &names)
{
    QJsonArray tags;
    for (const QString &name : names)
        tags.append(QJsonObject{{QStringLiteral("name"), name}});
    return QJsonDocument(tags).toJson();
}

} // namespace

void EngineCatalogTest::parsesTagNames()
{
    const QStringList names = {QStringLiteral("1.10"), QStringLiteral("nightly"), QStringLiteral("1.9")};
    const std::optional<QStringList> parsed = EngineCatalog::parseTagNames(tagList(names));
    QVERIFY(parsed.has_value());
    QCOMPARE(*parsed, names);

    const std::optional<QStringList> empty = EngineCatalog::parseTagNames(QByteArrayLiteral("[]"));
    QVERIFY(empty.has_value());
    QVERIFY(empty->isEmpty());
}

void EngineCatalogTest::rejectsBodyThatIsNotATagList()
{
    QVERIFY(!EngineCatalog::parseTagNames(QByteArrayLiteral("{\"message\": \"API rate limit exceeded\"}")).has_value());
    QVERIFY(!EngineCatalog::parseTagNames(QByteArrayLiteral("not json")).has_value());
    QVERIFY(!EngineCatalog::parseTagNames(QByteArray()).has_value());
}

// GitHub fills every page but the last with 100 tags, so a shorter page ends the fetch and a full one asks for more.
void EngineCatalogTest::pageOfHundredIsNotTheLast()
{
    QVERIFY(EngineCatalog::isLastPage(0));
    QVERIFY(EngineCatalog::isLastPage(99));
    QVERIFY(!EngineCatalog::isLastPage(100));
}

QTEST_APPLESS_MAIN(EngineCatalogTest)

#include "tst_enginecatalog.moc"
