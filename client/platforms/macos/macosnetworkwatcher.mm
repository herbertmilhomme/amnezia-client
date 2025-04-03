/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "macosnetworkwatcher.h"
#include "leakdetector.h"
#include "logger.h"

#import <CoreWLAN/CoreWLAN.h>
#import <Network/Network.h>

namespace {
Logger logger("MacOSNetworkWatcher");
}

@interface MacOSNetworkWatcherDelegate : NSObject <CWEventDelegate> {
  MacOSNetworkWatcher* m_watcher;
}
@end

@implementation MacOSNetworkWatcherDelegate

- (id)initWithObject:(MacOSNetworkWatcher*)watcher {
  self = [super init];
  if (self) {
    m_watcher = watcher;
  }
  return self;
}

- (void)bssidDidChangeForWiFiInterfaceWithName:(NSString*)interfaceName {
  logger.debug() << "BSSID changed!" << QString::fromNSString(interfaceName);

  if (m_watcher) {
    m_watcher->checkInterface();
  }
}

@end

void PowerNotificationsListener::registerForNotifications()
{
    rootPowerDomain = IORegisterForSystemPower(this, &notifyPortRef, sleepWakeupCallBack, &notifierObj);
    if (rootPowerDomain == IO_OBJECT_NULL) {
        logger.debug() << "Failed to register for system power notifications!";
        return;
    }

    logger.debug() << "IORegisterForSystemPower OK! Root port:" << rootPowerDomain;

    // add the notification port to the application runloop
    CFRunLoopAddSource(CFRunLoopGetCurrent(), IONotificationPortGetRunLoopSource(notifyPortRef), kCFRunLoopCommonModes);	
}

static void PowerNotificationsListener::sleepWakeupCallBack(void *refParam, io_service_t service, natural_t messageType, void *messageArgument)
{
	Q_UNUSED(service)

    auto listener = static_cast<PowerNotificationsListener *>(refParam);

    switch (messageType) {
    case kIOMessageCanSystemSleep:
        /* Idle sleep is about to kick in. This message will not be sent for forced sleep.
         * Applications have a chance to prevent sleep by calling IOCancelPowerChange.
         * Most applications should not prevent idle sleep. Power Management waits up to
         * 30 seconds for you to either allow or deny idle sleep. If you don’t acknowledge
         * this power change by calling either IOAllowPowerChange or IOCancelPowerChange,
         * the system will wait 30 seconds then go to sleep.
         */

        logger.debug() << "System power message: can system sleep?";

        // Uncomment to cancel idle sleep
        // IOCancelPowerChange(thiz->rootPowerDomain, reinterpret_cast<long>(messageArgument));

        // Allow idle sleep
        IOAllowPowerChange(listener->rootPowerDomain, reinterpret_cast<long>(messageArgument));
        break;

    case kIOMessageSystemWillNotSleep:
        /* Announces that the system has retracted a previous attempt to sleep; it
         * follows `kIOMessageCanSystemSleep`.
         */
        logger.debug() << "System power message: system will NOT sleep.";
        break;

    case kIOMessageSystemWillSleep:
        /* The system WILL go to sleep. If you do not call IOAllowPowerChange or
         * IOCancelPowerChange to acknowledge this message, sleep will be delayed by
         * 30 seconds.
         *
         * NOTE: If you call IOCancelPowerChange to deny sleep it returns kIOReturnSuccess,
         * however the system WILL still go to sleep.
         */

        logger.debug() << "System power message: system WILL sleep.";

        IOAllowPowerChange(listener->rootPowerDomain, reinterpret_cast<long>(messageArgument));
        break;

    case kIOMessageSystemWillPowerOn:
        /* Announces that the system is beginning to power the device tree; most devices
         * are still unavailable at this point.
         */
        /* From the documentation:
         *
         * - kIOMessageSystemWillPowerOn is delivered at early wakeup time, before most hardware
         * has been powered on. Be aware that any attempts to access disk, network, the display,
         * etc. may result in errors or blocking your process until those resources become
         * available.
         *
         * So we do NOT log this event.
         */
        break;

    case kIOMessageSystemHasPoweredOn:
        /* Announces that the system and its devices have woken up. */
        logger.debug() << "System power message: system has powered on.";
        break;

    default:
        logger.debug() << "System power message: other event: " << messageType;
        /* Not a system sleep and wake notification. */
        break;
    }
}

MacOSNetworkWatcher::MacOSNetworkWatcher(QObject* parent) : IOSNetworkWatcher(parent) {
  MZ_COUNT_CTOR(MacOSNetworkWatcher);
}

MacOSNetworkWatcher::~MacOSNetworkWatcher() {
  MZ_COUNT_DTOR(MacOSNetworkWatcher);
  if (m_delegate) {
    CWWiFiClient* client = CWWiFiClient.sharedWiFiClient;
    if (!client) {
      logger.debug() << "Unable to retrieve the CWWiFiClient shared instance";
      return;
    }

    [client stopMonitoringAllEventsAndReturnError:nullptr];
    [static_cast<MacOSNetworkWatcherDelegate*>(m_delegate) dealloc];
    m_delegate = nullptr;
  }
}

void MacOSNetworkWatcher::start() {
  NetworkWatcherImpl::start();

  checkInterface();

  if (m_delegate) {
    logger.debug() << "Delegate already registered";
    return;
  }
  
  m_powerlistener.registerForNotifications(); 

  CWWiFiClient* client = CWWiFiClient.sharedWiFiClient;
  if (!client) {
    logger.error() << "Unable to retrieve the CWWiFiClient shared instance";
    return;
  }

  logger.debug() << "Registering delegate";
  m_delegate = [[MacOSNetworkWatcherDelegate alloc] initWithObject:this];
  [client setDelegate:static_cast<MacOSNetworkWatcherDelegate*>(m_delegate)];
  [client startMonitoringEventWithType:CWEventTypeBSSIDDidChange error:nullptr];
}

void MacOSNetworkWatcher::checkInterface() {
  logger.debug() << "Checking interface";

  if (!isActive()) {
    logger.debug() << "Feature disabled";
    return;
  }

  CWWiFiClient* client = CWWiFiClient.sharedWiFiClient;
  if (!client) {
    logger.debug() << "Unable to retrieve the CWWiFiClient shared instance";
    return;
  }

  CWInterface* interface = [client interface];
  if (!interface) {
    logger.debug() << "No default wifi interface";
    return;
  }

  if (![interface powerOn]) {
    logger.debug() << "The interface is off";
    return;
  }

  NSString* ssidNS = [interface ssid];
  if (!ssidNS) {
    logger.debug() << "WiFi is not in used";
    return;
  }

  QString ssid = QString::fromNSString(ssidNS);
  if (ssid.isEmpty()) {
    logger.debug() << "WiFi doesn't have a valid SSID";
    return;
  }

  CWSecurity security = [interface security];
  if (security == kCWSecurityNone || security == kCWSecurityWEP) {
    logger.debug() << "Unsecured network found!";
    emit unsecuredNetwork(ssid, ssid);
    return;
  }

  logger.debug() << "Secure WiFi interface";
}

