#ifndef SCRIPTS_REGISTRY_H
#define SCRIPTS_REGISTRY_H

#include "containers/containers_defs.h"
#include "core/defs.h"
#include <QLatin1String>

namespace amnezia
{

    enum SharedScriptType {
        // General scripts
        prepare_host,
        install_docker,
        build_container,
        remove_container,
        remove_all_containers,
        setup_host_firewall,
        check_connection,
        check_server_is_busy,
        check_user_in_sudo
    };

    enum ProtocolScriptType {
        // Protocol scripts
        dockerfile,
        run_container,
        configure_container,
        container_startup,
        openvpn_template,
        wireguard_template,
        awg_template,
        xray_template
    };

    enum ClientScriptType {
        // Client-side scripts
        linux_installer
    };

    QString scriptFolder(DockerContainer container);

    QString scriptName(SharedScriptType type);
    QString scriptName(ProtocolScriptType type);
    QString scriptName(ClientScriptType type);

    QString scriptData(SharedScriptType type);
    QString scriptData(ProtocolScriptType type, DockerContainer container);
    QString scriptData(ClientScriptType type);
}

#endif // SCRIPTS_REGISTRY_H
