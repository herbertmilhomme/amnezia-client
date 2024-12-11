#include "ipcserver.h"

#include <QDateTime>
#include <QFileInfo>
#include <QLocalSocket>
#include <QObject>
#include <QProcess>

#include "logger.h"
#include "router.h"

#include "../client/protocols/protocols_defs.h"
#include "../core/networkUtilities.h"
#ifdef Q_OS_WIN
    #include "../client/platforms/windows/daemon/windowsdaemon.h"
    #include "../client/platforms/windows/daemon/windowsfirewall.h"
    #include "tapcontroller_win.h"
#endif

#ifdef Q_OS_LINUX
    #include "../client/platforms/linux/daemon/linuxfirewall.h"
#endif

#ifdef Q_OS_MACOS
    #include "../client/platforms/macos/daemon/macosfirewall.h"
#endif

IpcServer::IpcServer(QObject *parent) : IpcInterfaceSource(parent)

{
}

int IpcServer::createPrivilegedProcess()
{
#ifdef MZ_DEBUG
    qDebug() << "IpcServer::createPrivilegedProcess";
#endif

#ifdef Q_OS_WIN
    WindowsFirewall::instance()->init();
#endif

    m_localpid++;

    ProcessDescriptor pd(this);

    pd.localServer->setSocketOptions(QLocalServer::WorldAccessOption);

    if (!pd.localServer->listen(amnezia::getIpcProcessUrl(m_localpid))) {
        qDebug() << QString("Unable to start the server: %1.").arg(pd.localServer->errorString());
        return -1;
    }

    // Make sure any connections are handed to QtRO
    QObject::connect(pd.localServer.data(), &QLocalServer::newConnection, this, [pd]() {
        qDebug() << "IpcServer new connection";
        if (pd.serverNode) {
            pd.serverNode->addHostSideConnection(pd.localServer->nextPendingConnection());
            pd.serverNode->enableRemoting(pd.ipcProcess.data());
        }
    });

    QObject::connect(pd.serverNode.data(), &QRemoteObjectHost::error, this, [pd](QRemoteObjectNode::ErrorCode errorCode) {
        qDebug() << "QRemoteObjectHost::error" << errorCode;
    });

    QObject::connect(pd.serverNode.data(), &QRemoteObjectHost::destroyed, this,
                     [pd]() { qDebug() << "QRemoteObjectHost::destroyed"; });

    //    connect(pd.ipcProcess.data(), &IpcServerProcess::finished, this, [this, pid=m_localpid](int exitCode,
    //    QProcess::ExitStatus exitStatus){
    //        qDebug() << "IpcServerProcess finished" << exitCode << exitStatus;
    ////        if (m_processes.contains(pid)) {
    ////            m_processes[pid].ipcProcess.reset();
    ////            m_processes[pid].serverNode.reset();
    ////            m_processes[pid].localServer.reset();
    ////            m_processes.remove(pid);
    ////        }
    //    });

    m_processes.insert(m_localpid, pd);

    return m_localpid;
}

int IpcServer::routeAddList(const QString &gw, const QStringList &ips)
{
#ifdef MZ_DEBUG
    qDebug() << "IpcServer::routeAddList";
#endif

    return Router::routeAddList(gw, ips);
}

bool IpcServer::clearSavedRoutes()
{
#ifdef MZ_DEBUG
    qDebug() << "IpcServer::clearSavedRoutes";
#endif

    return Router::clearSavedRoutes();
}

bool IpcServer::routeDeleteList(const QString &gw, const QStringList &ips)
{
#ifdef MZ_DEBUG
    qDebug() << "IpcServer::routeDeleteList";
#endif

    return Router::routeDeleteList(gw, ips);
}

void IpcServer::flushDns()
{
#ifdef MZ_DEBUG
    qDebug() << "IpcServer::flushDns";
#endif

    return Router::flushDns();
}

void IpcServer::resetIpStack()
{
#ifdef MZ_DEBUG
    qDebug() << "IpcServer::resetIpStack";
#endif

    Router::resetIpStack();
}

bool IpcServer::checkAndInstallDriver()
{
#ifdef MZ_DEBUG
    qDebug() << "IpcServer::checkAndInstallDriver";
#endif

#ifdef Q_OS_WIN
    return TapController::checkAndSetup();
#else
    return true;
#endif
}

QStringList IpcServer::getTapList()
{
#ifdef MZ_DEBUG
    qDebug() << "IpcServer::getTapList";
#endif

#ifdef Q_OS_WIN
    return TapController::getTapList();
#else
    return QStringList();
#endif
}

void IpcServer::cleanUp()
{
#ifdef MZ_DEBUG
    qDebug() << "IpcServer::cleanUp";
#endif

    Logger::deInit();
    Logger::cleanUp();
}

void IpcServer::clearLogs()
{
    Logger::clearLogs(true);
}

bool IpcServer::createTun(const QString &dev, const QString &subnet)
{
    return Router::createTun(dev, subnet);
}

bool IpcServer::deleteTun(const QString &dev)
{
    return Router::deleteTun(dev);
}

bool IpcServer::updateResolvers(const QString &ifname, const QList<QHostAddress> &resolvers)
{
    return Router::updateResolvers(ifname, resolvers);
}

void IpcServer::StartRoutingIpv6()
{
    Router::StartRoutingIpv6();
}
void IpcServer::StopRoutingIpv6()
{
    Router::StopRoutingIpv6();
}

void IpcServer::setLogsEnabled(bool enabled)
{
#ifdef MZ_DEBUG
    qDebug() << "IpcServer::setLogsEnabled";
#endif

    if (enabled) {
        Logger::init(true);
    } else {
        Logger::deInit();
    }
}

bool IpcServer::enableKillSwitch(const QJsonObject &configStr, int vpnAdapterIndex)
{
#ifdef Q_OS_WIN
    return WindowsFirewall::instance()->enableKillSwitch(vpnAdapterIndex);
#endif

#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
    int splitTunnelType = configStr.value("splitTunnelType").toInt();
    QJsonArray splitTunnelSites = configStr.value("splitTunnelSites").toArray();
    bool blockAll = 0;
    bool allowNets = 0;
    bool blockNets = 0;
    QStringList allownets;
    QStringList blocknets;

    if (splitTunnelType == 0) {
        blockAll = true;
        allowNets = true;
        allownets.append(configStr.value("vpnServer").toString());
    } else if (splitTunnelType == 1) {
        blockNets = true;
        for (auto v : splitTunnelSites) {
            blocknets.append(v.toString());
        }
    } else if (splitTunnelType == 2) {
        blockAll = true;
        allowNets = true;
        allownets.append(configStr.value("vpnServer").toString());
        for (auto v : splitTunnelSites) {
            allownets.append(v.toString());
        }
    }
#endif

#ifdef Q_OS_LINUX
    // double-check + ensure our firewall is installed and enabled
    LinuxFirewall::setAnchorEnabled(LinuxFirewall::Both, QStringLiteral("000.allowLoopback"), true);
    LinuxFirewall::setAnchorEnabled(LinuxFirewall::Both, QStringLiteral("100.blockAll"), blockAll);
    LinuxFirewall::setAnchorEnabled(LinuxFirewall::IPv4, QStringLiteral("110.allowNets"), allowNets);
    LinuxFirewall::updateAllowNets(allownets);
    LinuxFirewall::setAnchorEnabled(LinuxFirewall::IPv4, QStringLiteral("120.blockNets"), blockAll);
    LinuxFirewall::updateBlockNets(blocknets);
    LinuxFirewall::setAnchorEnabled(LinuxFirewall::IPv4, QStringLiteral("200.allowVPN"), true);
    LinuxFirewall::setAnchorEnabled(LinuxFirewall::IPv6, QStringLiteral("250.blockIPv6"), true);
    LinuxFirewall::setAnchorEnabled(LinuxFirewall::Both, QStringLiteral("290.allowDHCP"), true);
    LinuxFirewall::setAnchorEnabled(LinuxFirewall::Both, QStringLiteral("300.allowLAN"), true);
    LinuxFirewall::setAnchorEnabled(LinuxFirewall::IPv4, QStringLiteral("310.blockDNS"), true);
    QStringList dnsServers;
    dnsServers.append(configStr.value(amnezia::config_key::dns1).toString());
    dnsServers.append(configStr.value(amnezia::config_key::dns2).toString());
    dnsServers.append("127.0.0.1");
    dnsServers.append("127.0.0.53");
    LinuxFirewall::updateDNSServers(dnsServers);
    LinuxFirewall::setAnchorEnabled(LinuxFirewall::IPv4, QStringLiteral("320.allowDNS"), true);
    LinuxFirewall::setAnchorEnabled(LinuxFirewall::Both, QStringLiteral("400.allowPIA"), true);
#endif

#ifdef Q_OS_MACOS

    // double-check + ensure our firewall is installed and enabled. This is necessary as
    // other software may disable pfctl before re-enabling with their own rules (e.g other VPNs)
    if (!MacOSFirewall::isInstalled())
        MacOSFirewall::install();

    MacOSFirewall::ensureRootAnchorPriority();
    MacOSFirewall::setAnchorEnabled(QStringLiteral("000.allowLoopback"), true);
    MacOSFirewall::setAnchorEnabled(QStringLiteral("100.blockAll"), blockAll);
    MacOSFirewall::setAnchorEnabled(QStringLiteral("110.allowNets"), allowNets);
    MacOSFirewall::setAnchorTable(QStringLiteral("110.allowNets"), allowNets, QStringLiteral("allownets"), allownets);

    MacOSFirewall::setAnchorEnabled(QStringLiteral("120.blockNets"), blockNets);
    MacOSFirewall::setAnchorTable(QStringLiteral("120.blockNets"), blockNets, QStringLiteral("blocknets"), blocknets);
    MacOSFirewall::setAnchorEnabled(QStringLiteral("200.allowVPN"), true);
    MacOSFirewall::setAnchorEnabled(QStringLiteral("250.blockIPv6"), true);
    MacOSFirewall::setAnchorEnabled(QStringLiteral("290.allowDHCP"), true);
    MacOSFirewall::setAnchorEnabled(QStringLiteral("300.allowLAN"), true);

    QStringList dnsServers;
    dnsServers.append(configStr.value(amnezia::config_key::dns1).toString());
    dnsServers.append(configStr.value(amnezia::config_key::dns2).toString());
    MacOSFirewall::setAnchorEnabled(QStringLiteral("310.blockDNS"), true);
    MacOSFirewall::setAnchorTable(QStringLiteral("310.blockDNS"), true, QStringLiteral("dnsaddr"), dnsServers);
#endif

    return true;
}

bool IpcServer::disableKillSwitch()
{
#ifdef Q_OS_WIN
    return WindowsFirewall::instance()->disableKillSwitch();
#endif

#ifdef Q_OS_LINUX
    LinuxFirewall::uninstall();
#endif

#ifdef Q_OS_MACOS
    MacOSFirewall::uninstall();
#endif

    return true;
}

bool IpcServer::enablePeerTraffic(const QJsonObject &configStr)
{
#ifdef Q_OS_WIN
    InterfaceConfig config;
    config.m_dnsServer = configStr.value(amnezia::config_key::dns1).toString();
    config.m_serverPublicKey = "openvpn";
    config.m_serverIpv4Gateway = configStr.value("vpnGateway").toString();
    config.m_serverIpv4AddrIn = configStr.value("vpnServer").toString();
    int vpnAdapterIndex = configStr.value("vpnAdapterIndex").toInt();
    int inetAdapterIndex = configStr.value("inetAdapterIndex").toInt();

    int splitTunnelType = configStr.value("splitTunnelType").toInt();
    QJsonArray splitTunnelSites = configStr.value("splitTunnelSites").toArray();

    QStringList AllowedIPAddesses;

    // Use APP split tunnel
    if (splitTunnelType == 0 || splitTunnelType == 2) {
        config.m_allowedIPAddressRanges.append(IPAddress(QHostAddress("0.0.0.0"), 0));
        config.m_allowedIPAddressRanges.append(IPAddress(QHostAddress("::"), 0));
    }

    if (splitTunnelType == 1) {
        for (auto v : splitTunnelSites) {
            QString ipRange = v.toString();
            if (ipRange.split('/').size() > 1) {
                config.m_allowedIPAddressRanges.append(
                        IPAddress(QHostAddress(ipRange.split('/')[0]), atoi(ipRange.split('/')[1].toLocal8Bit())));
            } else {
                config.m_allowedIPAddressRanges.append(IPAddress(QHostAddress(ipRange), 32));
            }
        }
    }

    config.m_excludedAddresses.append(configStr.value("vpnServer").toString());
    if (splitTunnelType == 2) {
        for (auto v : splitTunnelSites) {
            QString ipRange = v.toString();
            config.m_excludedAddresses.append(ipRange);
        }
    }

    for (const QJsonValue &i : configStr.value(amnezia::config_key::splitTunnelApps).toArray()) {
        if (!i.isString()) {
            break;
        }
        config.m_vpnDisabledApps.append(i.toString());
    }

    // killSwitch toggle
    if (QVariant(configStr.value(amnezia::config_key::killSwitchOption).toString()).toBool()) {
        WindowsFirewall::instance()->enablePeerTraffic(config);
    }

    WindowsDaemon::instance()->prepareActivation(config, inetAdapterIndex);
    WindowsDaemon::instance()->activateSplitTunnel(config, vpnAdapterIndex);
#endif
    return true;
}

int IpcServer::mountDmg(const QString &path, bool mount)
{
#ifdef Q_OS_MACOS
    qDebug() << path;
    auto res = QProcess::execute(QString("sudo hdiutil %1 %2").arg(mount ? "attach" : "unmount", path));
    qDebug() << res;
    return res;
#endif
    return 0;
}

int IpcServer::installApp(const QString &path)
{
    Logger logger("IpcServer");
    logger.info() << "Installing app from:" << path;

#ifdef Q_OS_WINDOWS
    QProcess process;
    logger.info() << "Launching installer with elevated privileges...";
    process.start("powershell.exe",
                  QStringList() << "Start-Process" << path << "-Verb"
                                << "RunAs"
                                << "-Wait");
    logger.info() << "Installer stdout:" << process.readAllStandardOutput();
    logger.info() << "Installer stderr:" << process.readAllStandardError();
    process.waitForFinished();

    if (process.exitCode() != 0) {
        logger.error() << "Installation error:" << process.readAllStandardError();
    }
    return process.exitCode();

#elif defined(Q_OS_MACOS)
    QProcess process;
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString mountPoint = tempDir + "/AmneziaVPN_mount";

    // Create mount point
    QDir dir(mountPoint);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    // Mount DMG image
    logger.info() << "Mounting DMG image...";
    process.start("hdiutil", QStringList() << "attach" << path << "-mountpoint" << mountPoint << "-nobrowse");
    process.waitForFinished();

    if (process.exitCode() != 0) {
        logger.error() << "Failed to mount DMG:" << process.readAllStandardError();
        return process.exitCode();
    }

    // Look for .app bundle in mounted image
    QDirIterator it(mountPoint, QStringList() << "*.app", QDir::Dirs);
    if (!it.hasNext()) {
        logger.error() << "No .app bundle found in DMG";
        return -1;
    }

    QString appPath = it.next();
    QString targetPath = "/Applications/" + QFileInfo(appPath).fileName();

    // Copy application to /Applications
    logger.info() << "Copying app to Applications folder...";
    process.start("cp", QStringList() << "-R" << appPath << targetPath);
    process.waitForFinished();

    // Unmount DMG
    logger.info() << "Unmounting DMG...";
    process.start("hdiutil", QStringList() << "detach" << mountPoint);
    process.waitForFinished();

    if (process.exitCode() != 0) {
        logger.error() << "Installation error:" << process.readAllStandardError();
    }
    return process.exitCode();

#elif defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    QProcess process;
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString extractDir = tempDir + "/amnezia_update";

    logger.info() << "Using temp directory:" << extractDir;

    // Create extraction directory if it doesn't exist
    QDir dir(extractDir);
    if (!dir.exists()) {
        dir.mkpath(".");
        logger.info() << "Created extraction directory";
    }

    // First, extract the zip archive
    logger.info() << "Extracting ZIP archive...";
    process.start("unzip", QStringList() << path << "-d" << extractDir);
    process.waitForFinished();
    if (process.exitCode() != 0) {
        logger.error() << "ZIP extraction error:" << process.readAllStandardError();
        return process.exitCode();
    }
    logger.info() << "ZIP archive extracted successfully";

    // Look for tar file in extracted files
    logger.info() << "Looking for TAR file...";
    QDirIterator tarIt(extractDir, QStringList() << "*.tar", QDir::Files);
    if (!tarIt.hasNext()) {
        logger.error() << "TAR file not found in the extracted archive";
        return -1;
    }

    // Extract found tar archive
    QString tarPath = tarIt.next();
    logger.info() << "Found TAR file:" << tarPath;
    logger.info() << "Extracting TAR archive...";

    process.start("tar", QStringList() << "-xf" << tarPath << "-C" << extractDir);
    process.waitForFinished();
    if (process.exitCode() != 0) {
        logger.error() << "TAR extraction error:" << process.readAllStandardError();
        return process.exitCode();
    }
    logger.info() << "TAR archive extracted successfully";

    // Remove tar file as it's no longer needed
    QFile::remove(tarPath);
    logger.info() << "Removed temporary TAR file";

    // Find executable file and run it
    logger.info() << "Looking for executable file...";
    QDirIterator it(extractDir, QDir::Files | QDir::Executable, QDirIterator::Subdirectories);
    if (it.hasNext()) {
        QString execPath = it.next();
        logger.info() << "Found executable:" << execPath;
        logger.info() << "Launching installer...";
        process.start("sudo", QStringList() << execPath);
        process.waitForFinished();
        logger.info() << "Installer finished with exit code:" << process.exitCode();
        return process.exitCode();
    }

    logger.error() << "No executable file found";
    return -1; // Executable not found
#endif
    return 0;
}
