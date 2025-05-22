#include "updateController.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QVersionNumber>
#include <QtConcurrent>
#include <QUrl>

#include "amnezia_application.h"
#include "core/errorstrings.h"
#include "core/scripts_registry.h"
#include "logger.h"
#include "version.h"
#include "core/controllers/gatewayController.h"

namespace
{
    Logger logger("UpdateController");

#ifdef Q_OS_MACOS
    const QString installerPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/AmneziaVPN.dmg";
#elif defined Q_OS_WINDOWS
    const QString installerPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/AmneziaVPN_installer.exe";
#elif defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    const QString installerPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/AmneziaVPN.tar.zip";
#endif
}

UpdateController::UpdateController(const std::shared_ptr<Settings> &settings, QObject *parent) : QObject(parent), m_settings(settings)
{
}

QString UpdateController::getHeaderText()
{
    return tr("New version released: %1 (%2)").arg(m_version, m_releaseDate);
}

QString UpdateController::getChangelogText()
{
    QStringList lines = m_changelogText.split("\n");
    QStringList filteredChangeLogText;
    bool add = false;
    QString osSection;

#ifdef Q_OS_WINDOWS
    osSection = "### Windows";
#elif defined(Q_OS_MACOS)
    osSection = "### macOS";
#elif defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    osSection = "### Linux";
#endif

    for (const QString &line : lines) {
        if (line.startsWith("### General")) {
            add = true;
        } else if (line.startsWith("### ") && line != osSection) {
            add = false;
        } else if (line == osSection) {
            add = true;
        }

        if (add) {
            filteredChangeLogText.append(line);
        }
    }

    return filteredChangeLogText.join("\n");
}

void UpdateController::checkForUpdates()
{
    qDebug() << "checkForUpdates";
    GatewayController gatewayController(m_settings->getGatewayEndpoint(),
                                        m_settings->isDevGatewayEnv(),
                                        7000,
                                        m_settings->isStrictKillSwitchEnabled());

    QByteArray gatewayResponse;
    auto err = gatewayController.get(QStringLiteral("%1v1/updater_endpoint"), gatewayResponse);
    if (err != ErrorCode::NoError) {
        logger.error() << errorString(err);
        return;
    }
    QJsonObject gatewayData = QJsonDocument::fromJson(gatewayResponse).object();
    qDebug() << "gatewayData:" << gatewayData;
    QString baseUrl = gatewayData.value("url").toString();
    if (baseUrl.endsWith('/')) {
        baseUrl.chop(1);
    }

    // Fetch version file
    QNetworkRequest versionReq;
    versionReq.setTransferTimeout(7000);
    versionReq.setUrl(QUrl(baseUrl + "/VERSION"));
    QNetworkReply* versionReply = amnApp->networkManager()->get(versionReq);
    // Handle network and SSL errors for VERSION fetch
    QObject::connect(versionReply, &QNetworkReply::errorOccurred, [this, versionReply](QNetworkReply::NetworkError error) {
        logger.error() << "Network error occurred while fetching VERSION:" << versionReply->errorString() << error;
    });
    QObject::connect(versionReply, &QNetworkReply::sslErrors, [this, versionReply](const QList<QSslError> &errors) {
        QStringList errorStrings;
        for (const QSslError &err : errors) errorStrings << err.errorString();
        logger.error() << "SSL errors while fetching VERSION:" << errorStrings;
    });
    QObject::connect(versionReply, &QNetworkReply::finished, [this, versionReply, baseUrl]() {
        if (versionReply->error() == QNetworkReply::NoError) {
            QByteArray versionData = versionReply->readAll();
            qDebug() << "versionReply data:" << QString::fromUtf8(versionData);
            m_version = QString::fromUtf8(versionData).trimmed();
            auto currentVersion = QVersionNumber::fromString(QString(APP_VERSION));
            auto newVersion = QVersionNumber::fromString(m_version);
            if (newVersion > currentVersion) {
                // Fetch changelog file
                QNetworkRequest changelogReq;
                changelogReq.setTransferTimeout(7000);
                changelogReq.setUrl(QUrl(baseUrl + "/CHANGELOG"));
                QNetworkReply* changelogReply = amnApp->networkManager()->get(changelogReq);
                // Handle network and SSL errors for CHANGELOG fetch
                QObject::connect(changelogReply, &QNetworkReply::errorOccurred, [this, changelogReply](QNetworkReply::NetworkError error) {
                    logger.error() << "Network error occurred while fetching CHANGELOG:" << changelogReply->errorString() << error;
                });
                QObject::connect(changelogReply, &QNetworkReply::sslErrors, [this, changelogReply](const QList<QSslError> &errors) {
                    QStringList errorStrings;
                    for (const QSslError &err : errors) errorStrings << err.errorString();
                    logger.error() << "SSL errors while fetching CHANGELOG:" << errorStrings;
                });
                QObject::connect(changelogReply, &QNetworkReply::finished, [this, changelogReply, baseUrl]() {
                    if (changelogReply->error() == QNetworkReply::NoError) {
                        m_changelogText = QString::fromUtf8(changelogReply->readAll());
                    } else {
                        if (changelogReply->error() == QNetworkReply::NetworkError::OperationCanceledError
                            || changelogReply->error() == QNetworkReply::NetworkError::TimeoutError) {
                            logger.error() << errorString(ErrorCode::ApiConfigTimeoutError);
                        } else {
                            QString err = changelogReply->errorString();
                            logger.error() << QString::fromUtf8(changelogReply->readAll());
                            logger.error() << "Network error code:" << QString::number(static_cast<int>(changelogReply->error()));
                            logger.error() << "Error message:" << err;
                            logger.error() << "HTTP status:" << changelogReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                            logger.error() << errorString(ErrorCode::ApiConfigDownloadError);
                        }
                        m_changelogText = tr("Failed to load changelog text");
                    }
                    changelogReply->deleteLater();
                    m_releaseDate = QStringLiteral("TBD");

                    QString fileName;
#if defined(Q_OS_WINDOWS)
                    fileName = QString("AmneziaVPN_%1_x64.exe").arg(m_version);
#elif defined(Q_OS_MACOS)
                    fileName = QString("AmneziaVPN_%1_macos.dmg").arg(m_version);
#elif defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
                    fileName = QString("AmneziaVPN_%1_linux.tar.zip").arg(m_version);
#endif
                    m_downloadUrl = baseUrl + "/" + fileName;
                    qDebug() << "m_downloadUrl:" << m_downloadUrl;

                    emit updateFound();
                });
            }
        } else {
            // Detailed error logging for VERSION fetch
            if (versionReply->error() == QNetworkReply::NetworkError::OperationCanceledError
                || versionReply->error() == QNetworkReply::NetworkError::TimeoutError) {
                logger.error() << errorString(ErrorCode::ApiConfigTimeoutError);
            } else {
                QString err = versionReply->errorString();
                logger.error() << QString::fromUtf8(versionReply->readAll());
                logger.error() << "Network error code:" << QString::number(static_cast<int>(versionReply->error()));
                logger.error() << "Error message:" << err;
                logger.error() << "HTTP status:" << versionReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                logger.error() << errorString(ErrorCode::ApiConfigDownloadError);
            }
        }
        versionReply->deleteLater();
    });
}

void UpdateController::runInstaller()
{
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS)
    if (m_downloadUrl.isEmpty()) {
        logger.error() << "Download URL is empty";
        return;
    }

    QNetworkRequest request;
    request.setTransferTimeout(7000);
    request.setUrl(m_downloadUrl);

    QNetworkReply *reply = amnApp->networkManager()->get(request);

    QObject::connect(reply, &QNetworkReply::finished, [this, reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            QFile file(installerPath);
            if (!file.open(QIODevice::WriteOnly)) {
                logger.error() << "Failed to open installer file for writing:" << installerPath << "Error:" << file.errorString();
                reply->deleteLater();
                return;
            }

            if (file.write(reply->readAll()) == -1) {
                logger.error() << "Failed to write installer data to file:" << installerPath << "Error:" << file.errorString();
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
#endif
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
    bool success =
            QProcess::startDetached("/bin/bash", QStringList() << scriptPath << extractDir.path() << installerPath, extractDir.path(), &pid);

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
    bool success =
            QProcess::startDetached("/bin/bash", QStringList() << scriptPath << extractDir.path() << installerPath, extractDir.path(), &pid);

    if (success) {
        logger.info() << "Installation process started with PID:" << pid;
    } else {
        logger.error() << "Failed to start installation process";
        return -1;
    }

    return 0;
}
#endif
