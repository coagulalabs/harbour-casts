#include "feedparser.h"

#include <QDateTime>
#include <QRegularExpression>
#include <QXmlStreamReader>

namespace {

QString localName(const QXmlStreamReader &xml)
{
    return xml.name().toString().section(':', -1);
}

QString readText(QXmlStreamReader &xml)
{
    return xml.readElementText().trimmed();
}

QString readLimitedText(QXmlStreamReader &xml, int maxChars)
{
    QString result;
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isEndElement()) break;
        if (xml.isCharacters()) {
            result += xml.text().toString();
            if (result.size() >= maxChars) {
                while (!(xml.atEnd() || xml.isEndElement()))
                    xml.readNext();
                break;
            }
        }
    }
    return result.trimmed();
}

qint64 epochFromText(const QString &text)
{
    const QDateTime dt = QDateTime::fromString(text.trimmed(), Qt::RFC2822Date);
    return dt.isValid() ? dt.toMSecsSinceEpoch() / 1000 : 0;
}

bool looksLikeAudio(const QString &type, const QString &medium, const QString &url)
{
    return type.startsWith(QStringLiteral("audio"))
        || medium == QLatin1String("audio")
        || url.contains(QStringLiteral(".mp"), Qt::CaseInsensitive)
        || url.contains(QStringLiteral(".ogg"), Qt::CaseInsensitive)
        || url.contains(QStringLiteral(".m4a"), Qt::CaseInsensitive)
        || url.contains(QStringLiteral("audio"), Qt::CaseInsensitive);
}

void setAudioUrl(ParsedEpisode *ep, const QString &url)
{
    if (ep && ep->audioUrl.isEmpty() && !url.isEmpty())
        ep->audioUrl = url;
}

void readAudioElement(QXmlStreamReader &xml, ParsedEpisode *ep)
{
    const QXmlStreamAttributes attrs = xml.attributes();
    const QString type = attrs.value(QStringLiteral("type")).toString();
    const QString medium = attrs.value(QStringLiteral("medium")).toString();
    QString candidate = attrs.value(QStringLiteral("url")).toString();
    if (candidate.isEmpty())
        candidate = attrs.value(QStringLiteral("href")).toString();
    if (looksLikeAudio(type, medium, candidate))
        setAudioUrl(ep, candidate);
    xml.skipCurrentElement();
}

void readChannelImage(QXmlStreamReader &xml, ParsedFeed *out)
{
    const QXmlStreamAttributes attrs = xml.attributes();
    if (attrs.hasAttribute(QStringLiteral("href"))) {
        out->imageUrl = attrs.value(QStringLiteral("href")).toString();
        xml.skipCurrentElement();
        return;
    }
    while (!(xml.atEnd() || (xml.isEndElement() && localName(xml) == QLatin1String("image")))) {
        if (xml.isStartElement() && localName(xml) == QLatin1String("url"))
            out->imageUrl = readText(xml);
        else
            xml.readNext();
    }
}

} // namespace

int FeedParser::parseDuration(const QString &text)
{
    const QString t = text.trimmed();
    if (t.isEmpty()) return 0;
    bool ok = false;
    const int plain = t.toInt(&ok);
    if (ok) return plain;

    const QStringList parts = t.split(':');
    if (parts.size() == 3)
        return parts.at(0).toInt() * 3600 + parts.at(1).toInt() * 60 + parts.at(2).toInt();
    if (parts.size() == 2)
        return parts.at(0).toInt() * 60 + parts.at(1).toInt();
    return 0;
}

QString FeedParser::cleanDescription(const QString &html)
{
    QString text = html;
    text.remove(QRegularExpression(QStringLiteral("<[^>]*>")));
    text.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
    text.replace(QStringLiteral("&nbsp;"), QStringLiteral(" "));
    text.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
    text.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
    text.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
    text.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
    text = text.trimmed();
    if (text.length() > 4000)
        text = text.left(4000).trimmed() + QStringLiteral("…");
    return text;
}

bool FeedParser::parseRss(const QByteArray &data, const QString &feedUrl, ParsedFeed *out)
{
    if (!out || data.isEmpty()) return false;
    *out = ParsedFeed();
    out->url = feedUrl;

    QXmlStreamReader xml(data);
    bool inChannel = false;
    ParsedEpisode currentItem;
    bool inItem = false;

    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement()) {
            const QString tag = localName(xml);
            if (tag == QLatin1String("channel") || tag == QLatin1String("feed")) {
                inChannel = true;
            } else if (inChannel && tag == QLatin1String("title") && out->title.isEmpty() && !inItem) {
                out->title = readText(xml);
            } else if (inChannel && tag == QLatin1String("description") && out->description.isEmpty() && !inItem) {
                out->description = FeedParser::cleanDescription(readLimitedText(xml, 2000));
            } else if (inChannel && tag == QLatin1String("image") && out->imageUrl.isEmpty() && !inItem) {
                readChannelImage(xml, out);
            } else if (inChannel && (tag == QLatin1String("item") || tag == QLatin1String("entry"))) {
                inItem = true;
                currentItem = ParsedEpisode();
            } else if (inItem) {
                if (tag == QLatin1String("title"))
                    currentItem.title = readText(xml);
                else if (tag == QLatin1String("guid") || tag == QLatin1String("id"))
                    currentItem.guid = readText(xml);
                else if (tag == QLatin1String("description") || tag == QLatin1String("summary")
                         || tag == QLatin1String("encoded")) {
                    if (currentItem.description.isEmpty())
                        currentItem.description = FeedParser::cleanDescription(readLimitedText(xml, 8000));
                    else
                        xml.skipCurrentElement();
                }
                else if (tag == QLatin1String("content")) {
                    const QString url = xml.attributes().value(QStringLiteral("url")).toString();
                    if (!url.isEmpty())
                        readAudioElement(xml, &currentItem);
                    else
                        xml.skipCurrentElement();
                } else if (tag == QLatin1String("pubDate") || tag == QLatin1String("published") || tag == QLatin1String("updated"))
                    currentItem.pubDate = epochFromText(readText(xml));
                else if (tag == QLatin1String("duration"))
                    currentItem.durationSec = parseDuration(readText(xml));
                else if (tag == QLatin1String("enclosure") || tag == QLatin1String("link"))
                    readAudioElement(xml, &currentItem);
                else
                    xml.skipCurrentElement();
            } else if (inChannel) {
                // Unknown channel metadata (itunes:*, atom:link, etc.)
                xml.skipCurrentElement();
            }
            // Root wrappers (<rss>, <RDF>) are left open so <channel>/<feed> is reached.
        } else if (xml.isEndElement()) {
            const QString tag = localName(xml);
            if (tag == QLatin1String("item") || tag == QLatin1String("entry")) {
                if (!currentItem.audioUrl.isEmpty()) {
                    if (currentItem.guid.isEmpty())
                        currentItem.guid = currentItem.audioUrl;
                    if (currentItem.title.isEmpty())
                        currentItem.title = currentItem.guid;
                    out->episodes.append(currentItem);
                }
                inItem = false;
            }
        }
    }

    return !out->title.isEmpty() && !out->episodes.isEmpty();
}

QVector<OpmlOutline> FeedParser::parseOpml(const QByteArray &data)
{
    QVector<OpmlOutline> outlines;
    QXmlStreamReader xml(data);
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement() && localName(xml) == QLatin1String("outline")) {
            const QXmlStreamAttributes attrs = xml.attributes();
            const QString xmlUrl = attrs.value(QStringLiteral("xmlUrl")).toString();
            if (!xmlUrl.isEmpty()) {
                OpmlOutline o;
                o.xmlUrl = xmlUrl;
                o.title = attrs.value(QStringLiteral("title")).toString();
                if (o.title.isEmpty())
                    o.title = attrs.value(QStringLiteral("text")).toString();
                outlines.append(o);
            }
        }
    }
    return outlines;
}
