#include "enginecatalog.h"

#include "versionutils.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>

#include <algorithm>

namespace {

// GitHub returns at most 100 tags per page, so a shorter page is the last one.
constexpr int tagsPerPage = 100;

// Release tags are plain dotted numbers such as "1.10" or "1.8.6512.2".
bool isVersionTag(const QString &tag)
{
    static const QRegularExpression pattern(QStringLiteral("^\\d+(\\.\\d+)*$"));
    return pattern.match(tag).hasMatch();
}

} // namespace

EngineCatalog::EngineCatalog(QObject *parent)
    : QObject(parent)
{
}

void EngineCatalog::fetch()
{
    if (m_fetching)
        return;

    m_fetching = true;
    requestPage(1, {});
}

// Tags are paged, so each page is requested in turn until a short one arrives.
void EngineCatalog::requestPage(int page, const QStringList &tags)
{
    const QString url = QStringLiteral("https://api.github.com/repos/FlaxEngine/FlaxEngine/tags?per_page=%1&page=%2")
                            .arg(tagsPerPage)
                            .arg(page);
    QNetworkRequest request{QUrl(url)};
    // GitHub rejects API requests that have no User-Agent.
    request.setRawHeader("User-Agent", "FlaxHub");
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setTransferTimeout(15000);

    QNetworkReply *reply = m_network.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, page, tags]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "Could not fetch engine versions:" << reply->errorString();
            m_fetching = false;
            emit fetchFailed();
            return;
        }

        const std::optional<QStringList> pageTags = parseTagNames(reply->readAll());
        if (!pageTags) {
            qWarning() << "Could not read engine versions: GitHub did not return a tag list";
            m_fetching = false;
            emit fetchFailed();
            return;
        }

        const QStringList allTags = tags + *pageTags;

        if (isLastPage(pageTags->size()))
            finishFetch(allTags);
        else
            requestPage(page + 1, allTags);
    });
}

void EngineCatalog::finishFetch(const QStringList &tags)
{
    m_fetching = false;
    emit versionsFetched(versionTags(tags));
}

std::optional<QStringList> EngineCatalog::parseTagNames(const QByteArray &body)
{
    const QJsonDocument document = QJsonDocument::fromJson(body);
    if (!document.isArray())
        return std::nullopt;

    QStringList names;
    const QJsonArray tags = document.array();
    for (const QJsonValue &tag : tags)
        names.append(tag.toObject().value("name").toString());
    return names;
}

bool EngineCatalog::isLastPage(int tagCount)
{
    return tagCount < tagsPerPage;
}

QStringList EngineCatalog::versionTags(const QStringList &tags)
{
    QStringList versions;
    for (const QString &tag : tags) {
        if (isVersionTag(tag))
            versions.append(tag);
    }
    std::sort(versions.begin(), versions.end(), isNewerVersion);
    return versions;
}
