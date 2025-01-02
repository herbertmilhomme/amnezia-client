#include "updateController.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QVersionNumber>
#include <QtConcurrent>

#include "amnezia_application.h"
#include "core/errorstrings.h"
#include "core/scripts_registry.h"
#include "logger.h"
#include "version.h"

namespace
{
    Logger logger("UpdateController");

#ifdef Q_OS_MACOS
    const QString installerPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/AmneziaVPN.dmg";
#elif defined Q_OS_WINDOWS
    const QString installerPath =
            QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/AmneziaVPN_installer.exe";
#elif defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    const QString installerPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/AmneziaVPN.tar.zip";
#endif
}

UpdateController::UpdateController(const std::shared_ptr<Settings> &settings, QObject *parent)
    : QObject(parent), m_settings(settings)
{
}

QString UpdateController::getHeaderText()
{
    return tr("New version released: %1 (%2)").arg(m_version, m_releaseDate);
}

QString UpdateController::getChangelogText()
{
    return m_changelogText;
}

void UpdateController::checkForUpdates()
{
    QNetworkRequest request;
    request.setTransferTimeout(7000);
    QString endpoint = "https://api.github.com/repos/amnezia-vpn/amnezia-client/releases/latest";
    request.setUrl(endpoint);

    QNetworkReply *reply = amnApp->manager()->get(request);

    QObject::connect(reply, &QNetworkReply::finished, [this, reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            QString contents = QString::fromUtf8(reply->readAll());
            QJsonObject data = QJsonDocument::fromJson(contents.toUtf8()).object();
            m_version = data.value("tag_name").toString();

            auto currentVersion = QVersionNumber::fromString(QString(APP_VERSION));
            auto newVersion = QVersionNumber::fromString(m_version);
            if (newVersion > currentVersion) {
                m_changelogText = data.value("body").toString();

                QString dateString = data.value("published_at").toString();
                QDateTime dateTime = QDateTime::fromString(dateString, "yyyy-MM-ddTHH:mm:ssZ");
                m_releaseDate = dateTime.toString("MMM dd yyyy");

                QJsonArray assets = data.value("assets").toArray();

                for (auto asset : assets) {
                    QJsonObject assetObject = asset.toObject();
#ifdef Q_OS_WINDOWS
                    if (assetObject.value("name").toString().endsWith(".exe")) {
                        m_downloadUrl = assetObject.value("browser_download_url").toString();
                    }
#elif defined(Q_OS_MACOS)
                    if (assetObject.value("name").toString().endsWith(".dmg")) {
                        m_downloadUrl = assetObject.value("browser_download_url").toString();
                    }
#elif defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
                    if (assetObject.value("name").toString().contains(".tar.zip")) {
                        m_downloadUrl = assetObject.value("browser_download_url").toString();
                    }
#endif
                }

                emit updateFound();
            }
        } else {
            if (reply->error() == QNetworkReply::NetworkError::OperationCanceledError
                || reply->error() == QNetworkReply::NetworkError::TimeoutError) {
                logger.error() << errorString(ErrorCode::ApiConfigTimeoutError);
            } else {
                QString err = reply->errorString();
                logger.error() << QString::fromUtf8(reply->readAll());
                logger.error() << "Network error code:" << QString::number(static_cast<int>(reply->error()));
                logger.error() << "Error message:" << err;
                logger.error() << "HTTP status:" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                logger.error() << errorString(ErrorCode::ApiConfigDownloadError);
            }
        }

        reply->deleteLater();
    });

    QObject::connect(reply, &QNetworkReply::errorOccurred, [this, reply](QNetworkReply::NetworkError error) {
        logger.error() << "Network error occurred:" << reply->errorString() << error;
    });
    connect(reply, &QNetworkReply::sslErrors, [this, reply](const QList<QSslError> &errors) {
        QStringList errorStrings;
        for (const QSslError &error : errors) {
            errorStrings << error.errorString();
        }
        logger.error() << "SSL errors:" << errorStrings;
        logger.error() << errorString(ErrorCode::ApiConfigSslError);
    });
}

void UpdateController::runInstaller()
{
    if (m_downloadUrl.isEmpty()) {
        logger.error() << "Download URL is empty";
        return;
    }

    QNetworkRequest request;
    request.setTransferTimeout(7000);
    request.setUrl(m_downloadUrl);

    QNetworkReply *reply = amnApp->manager()->get(request);

    QObject::connect(reply, &QNetworkReply::finished, [this, reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            QFile file(installerPath);
            if (!file.open(QIODevice::WriteOnly)) {
                logger.error() << "Failed to open installer file for writing:" << installerPath
                               << "Error:" << file.errorString();
                reply->deleteLater();
                return;
            }

            if (file.write(reply->readAll()) == -1) {
                logger.error() << "Failed to write installer data to file:" << installerPath
                               << "Error:" << file.errorString();
                file.close();
                reply->deleteLater();
                return;
            }

            file.close();

#if defined(Q_OS_WINDOWS)
            runWindowsInstaller(installerPath);
#elif defined(Q_OS_MACOS)
            runMacInstaller(installerPath);
#elif defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
            runLinuxInstaller(installerPath);
#endif
        } else {
            if (reply->error() == QNetworkReply::NetworkError::OperationCanceledError
                || reply->error() == QNetworkReply::NetworkError::TimeoutError) {
                logger.error() << errorString(ErrorCode::ApiConfigTimeoutError);
            } else {
                QString err = reply->errorString();
                logger.error() << QString::fromUtf8(reply->readAll());
                logger.error() << "Network error code:" << QString::number(static_cast<int>(reply->error()));
                logger.error() << "Error message:" << err;
                logger.error() << "HTTP status:" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                logger.error() << errorString(ErrorCode::ApiConfigDownloadError);
            }
        }
        reply->deleteLater();
    });
}

#if defined(Q_OS_WINDOWS)
int UpdateController::runWindowsInstaller(const QString &installerPath)
{
    qint64 pid;
    bool success = QProcess::startDetached(installerPath, QStringList(), QString(), &pid);

    if (success) {
        logger.info() << "Installation process started with PID:" << pid;
    } else {
        logger.error() << "Failed to start installation process";
        return -1;
    }

    return 0;
}
#endif

#if defined(Q_OS_MACOS)
int UpdateController::runMacInstaller(const QString &installerPath)
{
    // Create temporary directory for extraction
    QTemporaryDir extractDir;
    extractDir.setAutoRemove(false);
    if (!extractDir.isValid()) {
        logger.error() << "Failed to create temporary directory";
        return -1;
    }
    logger.info() << "Temporary directory created:" << extractDir.path();

    // Create script file in the temporary directory
    QString scriptPath = extractDir.path() + "/mac_installer.sh";
    QFile scriptFile(scriptPath);
    if (!scriptFile.open(QIODevice::WriteOnly)) {
        logger.error() << "Failed to create script file";
        return -1;
    }

    // Get script content from registry
    QString scriptContent = amnezia::scriptData(amnezia::ClientScriptType::mac_installer);
    if (scriptContent.isEmpty()) {
        logger.error() << "macOS installer script content is empty";
        scriptFile.close();
        return -1;
    }

    scriptFile.write(scriptContent.toUtf8());
    scriptFile.close();
    logger.info() << "Script file created:" << scriptPath;

    // Make script executable
    QFile::setPermissions(scriptPath, QFile::permissions(scriptPath) | QFile::ExeUser);

    // Start detached process
    qint64 pid;
    bool success = QProcess::startDetached(
            "/bin/bash", QStringList() << scriptPath << extractDir.path() << installerPath, extractDir.path(), &pid);

    if (success) {
        logger.info() << "Installation process started with PID:" << pid;
    } else {
        logger.error() << "Failed to start installation process";
        return -1;
    }

    return 0;
}
#endif

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
int UpdateController::runLinuxInstaller(const QString &installerPath)
{
    // Create temporary directory for extraction
    QTemporaryDir extractDir;
    extractDir.setAutoRemove(false);
    if (!extractDir.isValid()) {
        logger.error() << "Failed to create temporary directory";
        return -1;
    }
    logger.info() << "Temporary directory created:" << extractDir.path();

    // Create script file in the temporary directory
    QString scriptPath = extractDir.path() + "/installer.sh";
    QFile scriptFile(scriptPath);
    if (!scriptFile.open(QIODevice::WriteOnly)) {
        logger.error() << "Failed to create script file";
        return -1;
    }

    // Get script content from registry
    QString scriptContent = amnezia::scriptData(amnezia::ClientScriptType::linux_installer);
    scriptFile.write(scriptContent.toUtf8());
    scriptFile.close();
    logger.info() << "Script file created:" << scriptPath;

    // Make script executable
    QFile::setPermissions(scriptPath, QFile::permissions(scriptPath) | QFile::ExeUser);

    // Start detached process
    qint64 pid;
    bool success = QProcess::startDetached(
            "/bin/bash", QStringList() << scriptPath << extractDir.path() << installerPath, extractDir.path(), &pid);

    if (success) {
        logger.info() << "Installation process started with PID:" << pid;
    } else {
        logger.error() << "Failed to start installation process";
        return -1;
    }

    return 0;
}
#endif
