#ifndef FEEDPARSER_H
#define FEEDPARSER_H

#include <QString>
#include <QVector>

struct ParsedEpisode {
    QString guid;
    QString title;
    QString audioUrl;
    QString description;
    int durationSec = 0;
    qint64 pubDate = 0;
};

struct ParsedFeed {
    QString title;
    QString url;
    QString imageUrl;
    QString description;
    QVector<ParsedEpisode> episodes;
};

struct OpmlOutline {
    QString title;
    QString xmlUrl;
};

class FeedParser
{
public:
    static bool parseRss(const QByteArray &data, const QString &feedUrl, ParsedFeed *out);
    static QVector<OpmlOutline> parseOpml(const QByteArray &data);
    static int parseDuration(const QString &text);
    static QString cleanDescription(const QString &html);
};

#endif // FEEDPARSER_H
