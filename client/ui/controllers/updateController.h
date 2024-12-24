#ifndef UPDATECONTROLLER_H
#define UPDATECONTROLLER_H

#include <QObject>

#include "settings.h"

class UpdateController : public QObject
{
    Q_OBJECT
public:
    explicit UpdateController(const std::shared_ptr<Settings> &settings, QObject *parent = nullptr);

    Q_PROPERTY(QString changelogText READ getChangelogText NOTIFY updateFound)
    Q_PROPERTY(QString headerText READ getHeaderText NOTIFY updateFound)
public slots:
    QString getHeaderText();
    QString getChangelogText();

    void checkForUpdates();
    void runInstaller();
signals:
    void updateFound();

private:
    std::shared_ptr<Settings> m_settings;

    QString m_changelogText;
    QString m_version;
    QString m_releaseDate;
    QString m_downloadUrl;

#if defined(Q_OS_WINDOWS)
    int runWindowsInstaller(const QString &installerPath);
#elif defined(Q_OS_MACOS)
    int runMacInstaller(const QString &installerPath);
#elif defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    int runLinuxInstaller(const QString &installerPath);
#endif
};

#endif // UPDATECONTROLLER_H
