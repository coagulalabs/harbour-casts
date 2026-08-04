#include <QtTest/QtTest>

#include "feedparser.h"

class TstFeedParser : public QObject
{
    Q_OBJECT

private slots:
    void parseDuration_data();
    void parseDuration();

    void cleanDescription_data();
    void cleanDescription();

    void parseRss_sample();
    void parseRss_rejectsEmpty();
    void parseRss_rejectsNoAudio();

    void parseOpml_sample();
    void parseOpml_empty();
};

void TstFeedParser::parseDuration_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<int>("expected");

    QTest::newRow("empty") << QString() << 0;
    QTest::newRow("blank") << QStringLiteral("  ") << 0;
    QTest::newRow("seconds") << QStringLiteral("90") << 90;
    QTest::newRow("mm:ss") << QStringLiteral("12:34") << (12 * 60 + 34);
    QTest::newRow("hh:mm:ss") << QStringLiteral("1:02:03") << (1 * 3600 + 2 * 60 + 3);
    QTest::newRow("junk") << QStringLiteral("abc") << 0;
}

void TstFeedParser::parseDuration()
{
    QFETCH(QString, text);
    QFETCH(int, expected);
    QCOMPARE(FeedParser::parseDuration(text), expected);
}

void TstFeedParser::cleanDescription_data()
{
    QTest::addColumn<QString>("html");
    QTest::addColumn<QString>("expected");

    QTest::newRow("plain")
        << QStringLiteral("Hello world")
        << QStringLiteral("Hello world");
    QTest::newRow("tags")
        << QStringLiteral("<p>Hello <b>world</b></p>")
        << QStringLiteral("Hello world");
    QTest::newRow("entities")
        << QStringLiteral("A&nbsp;B &amp; C &lt;D&gt; &quot;E&quot;")
        << QStringLiteral("A B & C <D> \"E\"");
    QTest::newRow("whitespace")
        << QStringLiteral("  foo   \n\t bar  ")
        << QStringLiteral("foo bar");
}

void TstFeedParser::cleanDescription()
{
    QFETCH(QString, html);
    QFETCH(QString, expected);
    QCOMPARE(FeedParser::cleanDescription(html), expected);
}

void TstFeedParser::parseRss_sample()
{
    QFile f(QFINDTESTDATA("fixtures/sample.rss"));
    QVERIFY2(f.open(QIODevice::ReadOnly), qPrintable(f.fileName()));

    ParsedFeed feed;
    QVERIFY(FeedParser::parseRss(f.readAll(), QStringLiteral("https://example.com/feed.xml"), &feed));
    QCOMPARE(feed.title, QStringLiteral("Harbour Test Cast"));
    QCOMPARE(feed.url, QStringLiteral("https://example.com/feed.xml"));
    QCOMPARE(feed.imageUrl, QStringLiteral("https://example.com/cover.png"));
    QVERIFY(feed.description.contains(QStringLiteral("sample feed")));
    QCOMPARE(feed.episodes.size(), 2);

    QCOMPARE(feed.episodes.at(0).guid, QStringLiteral("ep-1"));
    QCOMPARE(feed.episodes.at(0).title, QStringLiteral("Episode One"));
    QCOMPARE(feed.episodes.at(0).audioUrl, QStringLiteral("https://example.com/ep1.mp3"));
    QCOMPARE(feed.episodes.at(0).durationSec, 1 * 3600 + 2 * 60 + 3);
    QVERIFY(feed.episodes.at(0).pubDate > 0);
    QVERIFY(feed.episodes.at(0).description.contains(QStringLiteral("Hello & welcome")));

    QCOMPARE(feed.episodes.at(1).guid, QStringLiteral("ep-2"));
    QCOMPARE(feed.episodes.at(1).audioUrl, QStringLiteral("https://example.com/ep2.m4a"));
    QCOMPARE(feed.episodes.at(1).durationSec, 90);
}

void TstFeedParser::parseRss_rejectsEmpty()
{
    ParsedFeed feed;
    QVERIFY(!FeedParser::parseRss(QByteArray(), QStringLiteral("https://example.com/x"), &feed));
    QVERIFY(!FeedParser::parseRss(QByteArray("<rss/>"), QStringLiteral("https://example.com/x"), nullptr));
}

void TstFeedParser::parseRss_rejectsNoAudio()
{
    const QByteArray xml =
        "<?xml version=\"1.0\"?>"
        "<rss><channel><title>Empty</title>"
        "<item><title>No audio</title><guid>x</guid></item>"
        "</channel></rss>";
    ParsedFeed feed;
    QVERIFY(!FeedParser::parseRss(xml, QStringLiteral("https://example.com/x"), &feed));
}

void TstFeedParser::parseOpml_sample()
{
    QFile f(QFINDTESTDATA("fixtures/sample.opml"));
    QVERIFY2(f.open(QIODevice::ReadOnly), qPrintable(f.fileName()));

    const QVector<OpmlOutline> outlines = FeedParser::parseOpml(f.readAll());
    QCOMPARE(outlines.size(), 2);
    QCOMPARE(outlines.at(0).title, QStringLiteral("Cast A"));
    QCOMPARE(outlines.at(0).xmlUrl, QStringLiteral("https://example.com/a.xml"));
    QCOMPARE(outlines.at(1).title, QStringLiteral("Cast B"));
    QCOMPARE(outlines.at(1).xmlUrl, QStringLiteral("https://example.com/b.xml"));
}

void TstFeedParser::parseOpml_empty()
{
    QCOMPARE(FeedParser::parseOpml(QByteArray()).size(), 0);
    QCOMPARE(FeedParser::parseOpml(QByteArray("<opml><body/></opml>")).size(), 0);
}

QTEST_APPLESS_MAIN(TstFeedParser)
#include "tst_feedparser.moc"
