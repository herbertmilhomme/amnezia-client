// The MIT License (MIT)
//
// Copyright (C) 2016 Mostafa Sedaghat Joo (mostafa.sedaghat@gmail.com)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include "qautostart.h"

#include <QCoreApplication>
#include <QTextStream>
#include <QFileInfo>
#include <QSettings>
#include <QProcess>
#include <QString>
#include <QFile>
#include <QDir>
#include <QDebug>


#if defined (Q_OS_WIN)
#define REG_KEY "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"

bool Autostart::isAutostart() {
    QSettings settings(REG_KEY, QSettings::NativeFormat);

    if (settings.value(appName()).isNull()) {
        return false;
    }

    return true;
}

void Autostart::setAutostart(bool autostart) {
    QSettings settings(REG_KEY, QSettings::NativeFormat);

    if (autostart) {
        settings.setValue(appName() , appPath().replace('/','\\'));
    } else {
        settings.remove(appName());
    }
}

QString Autostart::appPath() {
    return QCoreApplication::applicationFilePath() + " --autostart";
}

#elif defined(Q_OS_MACX) || defined(Q_OS_MACOS)

#if defined(MACOS_NE)
#include <QFile>

static QString bundleIdentifier()
{
    return QCoreApplication::applicationName();
}

QString Autostart::launchAgentPlistPath()
{
    // ~/Library/LaunchAgents
    const QString agentsDir = QDir::homePath() + "/Library/LaunchAgents";
    return agentsDir + "/" + bundleIdentifier() + ".plist";
}

static QByteArray buildPlist()
{
    QString plist;
    QTextStream ts(&plist);
    ts << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    ts << "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n";
    ts << "<plist version=\"1.0\">\n";
    ts << "<dict>\n";
    ts << "  <key>Label</key>\n";
    ts << "  <string>" << bundleIdentifier() << "</string>\n";
    ts << "  <key>ProgramArguments</key>\n";
    ts << "  <array>\n";
    ts << "    <string>" << Autostart::appPath() << "</string>\n";
    ts << "  </array>\n";
    ts << "  <key>RunAtLoad</key>\n";
    ts << "  <true/>\n";
    ts << "</dict>\n";
    ts << "</plist>\n";

    return plist.toUtf8();
}

bool Autostart::isAutostart()
{
    const bool exists = QFile::exists(launchAgentPlistPath());
    qDebug() << "isAutostart?" << exists << "plistPath=" << launchAgentPlistPath();
    return exists;
}

void Autostart::setAutostart(bool autostart)
{
    const QString plistPath = launchAgentPlistPath();

    if (!autostart)
    {
        qDebug() << "Removing LaunchAgent plist" << plistPath;
        QFile::remove(plistPath);
        return;
    }

    qDebug() << "Creating LaunchAgent plist" << plistPath;
    QDir().mkpath(QFileInfo(plistPath).absolutePath());

    QFile f(plistPath);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        QByteArray data = buildPlist();
        f.write(data);
        qDebug() << "Written plist bytes=" << data.size();
        f.close();
    }
}

QString Autostart::appPath()
{
    // launchd expects the path to the .app bundle, not the executable.
    QDir appDir = QDir(QCoreApplication::applicationDirPath());
    appDir.cdUp();
    appDir.cdUp();
    return appDir.absolutePath();
}

#else // !MACOS_NE

bool Autostart::isAutostart() {
    QProcess process;
    process.start("osascript", {
        "-e tell application \"System Events\" to get the path of every login item"
    });
    process.waitForFinished(3000);
    const auto output = QString::fromLocal8Bit(process.readAllStandardOutput());
    const bool contains = output.contains(appPath());
    qDebug() << "AppleScript isAutostart?" << contains;
    return contains;
}

void Autostart::setAutostart(bool autostart) {
    // Remove any existing login entry for this app first, in case there was one
    // from a previous installation, that may be under a different launch path.
    qDebug() << "Removing existing login items (non-sandbox)";
    QProcess::execute("osascript", {
        "-e tell application \"System Events\" to delete every login item whose name is \"" + appName() + "\""
    });

    // Now install the login item, if needed.
    if ( autostart )
    {
        qDebug() << "Creating login item (non-sandbox)";
        QProcess::execute("osascript", {
            "-e tell application \"System Events\" to make login item at end with properties {path:\"" + appPath() + "\", hidden:true, name: \"" + appName() + "\"}"
        });
    }
}

QString Autostart::appPath() {
    QDir appDir = QDir(QCoreApplication::applicationDirPath());
    appDir.cdUp();
    appDir.cdUp();
    QString absolutePath = appDir.absolutePath();

    return absolutePath;
}

#endif // MACOS_NE

#elif defined (Q_OS_LINUX)
bool Autostart::isAutostart() {
    QFileInfo check_file(QDir::homePath() + "/.config/autostart/" + appName() +".desktop");

    if (check_file.exists() && check_file.isFile()) {
        return true;
    }

    return false;
}

void Autostart::setAutostart(bool autostart) {
    QString path = QDir::homePath() + "/.config/autostart/";
    QString name = appName() +".desktop";
    QFile file(path+name);

    file.remove();

    if(autostart) {
        QDir dir(path);
        if(!dir.exists()) {
            dir.mkpath(path);
        }

        if (file.open(QIODevice::ReadWrite)) {
            QTextStream stream(&file);
            stream << "[Desktop Entry]" << Qt::endl;
            stream << "Exec=AmneziaVPN" << Qt::endl;
            stream << "Type=Application" << Qt::endl;
            stream << "Name=AmneziaVPN" << Qt::endl;
            stream << "Comment=Client of your self-hosted VPN" << Qt::endl;
            stream << "Icon=/usr/share/pixmaps/AmneziaVPN.png" << Qt::endl;
            stream << "Categories=Network;Qt;Security;" << Qt::endl;
            stream << "Terminal=false" << Qt::endl;
        }
    }
}

QString Autostart::appPath() {
    return QCoreApplication::applicationFilePath() + " --autostart";
}

#else

bool Autostart::isAutostart() {
    return false;
}

void Autostart::setAutostart(bool autostart) {
    Q_UNUSED(autostart);
}

QString Autostart::appPath() {
    return QString();
}
#endif

QString Autostart::appName() {
    return QCoreApplication::applicationName();
}
