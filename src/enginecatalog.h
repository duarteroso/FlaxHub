#ifndef ENGINECATALOG_H
#define ENGINECATALOG_H

#include <QByteArray>
#include <QNetworkAccessManager>
#include <QObject>
#include <QStringList>

#include <optional>

// Fetches the FlaxEngine release tags from GitHub and keeps the ones that are versions. The tags come a page at a
// time, so each page is requested in turn until a short one arrives. The result is handed out through a signal, and
// the owner decides what to do with it. Only one fetch runs at a time.
class EngineCatalog : public QObject
{
    Q_OBJECT

public:
    explicit EngineCatalog(QObject *parent = nullptr);

    // Starts a fetch. Does nothing while one is running. Ends with exactly one of versionsFetched or fetchFailed.
    void fetch();

    // The tag names in one page of GitHub's tag list, or nothing when the body is not a JSON array.
    static std::optional<QStringList> parseTagNames(const QByteArray &body);
    // Whether a page that held this many tags is the last one, since GitHub fills every page but the last.
    static bool isLastPage(int tagCount);
    // The tags that are versions, newest first.
    static QStringList versionTags(const QStringList &tags);

signals:
    // The version tags, newest first. Empty when the repository has none.
    void versionsFetched(const QStringList &versions);
    void fetchFailed();

private:
    void requestPage(int page, const QStringList &tags);
    void finishFetch(const QStringList &tags);

    QNetworkAccessManager m_network;
    bool m_fetching = false;
};

#endif
