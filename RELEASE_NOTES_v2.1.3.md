# sudosh v2.1.3 (Pre-release)

## Highlights
- sudo-compat: Honor global NOPASSWD for command execution and `sudo -v`
- Skip authentication entirely when already running setuid root (euid==0)
- Robust PAM detection across CentOS 7, AlmaLinux 8, etc. (link-probe for -lpam/-lpam_misc)
- Environment policy: env_reset baseline with whitelist restoration and TERM normalization
- SSSD helpers and tests for effective options and NOPASSWD checks

## Changes since v2.1.2
- Prefer global NOPASSWD (sudoers/SSSD) when executing commands in sudo-compat mode
- Add regression tests:
  - `sudo -n -v` under NOPASSWD
  - `sudo <cmd>` under NOPASSWD
  - Existing `sudo make install` compat test remains green
- Apply env_reset and env_keep whitelist restoration, baseline PATH/TERM handling
- Harden SSSD test API defaults and env_keep handling in security module
- Makefile: improve PAM probing for wider distro compatibility; version bump

## Quality and Testing
- All builds pass under C99 with `-Wall -Wextra -Werror`
- Unit, integration, and regression tests pass
- Coverage and static analysis logs available under `reports/v2.1.3`

## Security Notes
- NOPASSWD logic is conservative: prefer explicit global NOPASSWD; else fall back to command-aware checks
- euid==0 detection prevents unnecessary PAM interactions when already privileged
- Environment reset normalizes unsafe state and restores only whitelisted variables

## Known Issues / Future Work
- Full SSSD/LDAP sudo rule parity is ongoing; current implementation covers common cases robustly
- Additional negative tests for malformed sudoers/SSSD payloads can further harden the parser


