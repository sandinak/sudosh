/* Minimal vendor of SSSD client protocol pieces needed for sudo socket.
 * Source: https://github.com/SSSD/sssd (sss_client/sss_cli.h and common.c)
 * License: LGPLv3+. Copyright Red Hat.
 */
#ifndef SSS_CLI_MIN_H
#define SSS_CLI_MIN_H
#include <stdint.h>

#define SSS_SUDO_PROTOCOL_VERSION 1
#define SSS_NSS_HEADER_SIZE (sizeof(uint32_t) * 4)

/* Commands */
enum sss_cli_command {
    SSS_GET_VERSION           = 0x0001,
    SSS_SUDO_GET_SUDORULES    = 0x00C1,
    SSS_SUDO_GET_DEFAULTS     = 0x00C2,
};

/* Socket name macros as used by libsss */
#ifndef SSS_SUDO_SOCKET_NAME
#define SSS_SUDO_SOCKET_NAME "/var/lib/sss/pipes/sudo"
#endif

#endif /* SSS_CLI_MIN_H */

