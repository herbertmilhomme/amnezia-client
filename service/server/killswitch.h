#ifndef KILLSWITCH_H
#define KILLSWITCH_H

#include <QJsonObject>
#include <QSharedPointer>

#include "secure_qsettings.h"

class KillSwitch : public QObject
{
    Q_OBJECT
public:
    static KillSwitch *instance();
    bool init();
    bool disableKillSwitch();
    bool disableAllTraffic();
    bool enablePeerTraffic( const QJsonObject &configStr);
    bool enableKillSwitch( const QJsonObject &configStr, int vpnAdapterIndex);
    bool allowTrafficTo(const QStringList &ranges);
    bool isStrictKillSwitchEnabled();

private:
    KillSwitch(QObject* parent) {};
    QSharedPointer<SecureQSettings> m_appSettigns;

};

#endif // KILLSWITCH_H
