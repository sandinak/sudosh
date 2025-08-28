## Sudosh Security Model

This document describes what can run under sudosh, under which circumstances, and with what requirements. It reflects v2.1.x behavior including recent updates for dangerous-command authorization and install-time behaviors.

Derived behaviors:
- make install always uses the system sudo (never the sudosh symlink)
- make test-suid and clean-suid always use system sudo to add/remove suid bit with chmod
- sudosh su and sudosh su - redirect to the sudosh interactive shell (no direct su)
- Direct interactive language interpreters (python, perl, ruby, irb, ipython, etc.) are explicitly blocked (do not drop to a shell); non-interactive script execution remains subject to normal validation

### Identity, privileges, and modes
- Identity: sudosh determines the effective user via getpwuid() and stores a global current_username used in all validation and authorization checks.
- Privilege sources:
  - Sudoers (including includedir) and NSS user/group lookups.
  - SSSD/LDAP hooks exist but default to conservative behavior if not configured.
- Modes of operation:
  - Interactive shell (default)
  - Single-command execution (bare command or -c)
  - Sudo-compat invocation (argv[0] == "sudo"): implements a controlled subset of sudo UX

### Authentication requirements
- If user has NOPASSWD: ALL (global), privileged commands run without prompting.
- If user has NOPASSWD for a specific command, that command runs without prompting.
- Otherwise, commands that require privileges prompt for password and honor caching.
- Editor environment overrides can require password even with NOPASSWD (see below).

---

## Command categories and requirements

### 1) Safe read-only commands
- Examples: ls, pwd, whoami, id, date, uptime, w, who, last, echo
- Behavior: Allowed unprivileged

### 2) Text processing (strict validation)
- Examples: grep/egrep/fgrep, sed, awk/gawk, cut, sort, uniq, head, tail, wc, cat
- Behavior: Allowed unprivileged if the command and its arguments pass strict validation
- Redirection limited to safe locations (HOME, /tmp, /var/tmp)
- Pipelines allowed only if each stage individually validates

### 3) Secure editors/pagers (hardened)
- Examples: vi, vim, view, nano, pico
- Behavior: Allowed in a hardened environment (no shell escape)
- File locking: If locking cannot be initialized, secure editors continue with warnings; non-secure editors are blocked

### 4) Non-secure editors
- Examples: nvim, emacs, joe, mcedit, ed, ex
- Behavior: Blocked (can escape to shells)

### 5) Shells and shell-like commands
- Examples: sh, bash, zsh, csh/tcsh, ksh, fish, dash; shell -c patterns
- Behavior: Blocked; in sudo-compat mode, direct shells are redirected to sudosh interactive shell

### 6) Privilege escalation commands
- Examples: su, sudo, pkexec
- Behavior: Blocked (use sudosh directly)

### 7) Conditionally blocked groups (allowed with authorization)
- System control: init, shutdown, halt, reboot, poweroff, systemctl poweroff/reboot/halt/emergency/rescue; macOS launchctl (including reboot, bootout, kickstart variants)
- Disk operations: fdisk, parted, mkfs, fsck, dd, shred, wipe, mount/umount, swapon/swapoff
- Network security: iptables/ip6tables, ufw, firewall-cmd
- Communication: wall, write, mesg
- Behavior: Allowed if user is authorized by sudoers (explicit command rule or ALL). NOPASSWD is honored when present; otherwise a password prompt is required

### 8) Dangerous system operations (allowed with authorization)
- Examples include: rm/rmdir/unlink/shred/wipe; mv/cp; dd/rsync; chmod/chown/chgrp/chattr/setfacl; ln/link/symlink; mkdir/touch/truncate; tar/gzip/gunzip/zip/unzip; make/gcc/g++/cc/ld; passwd/chpasswd; useradd/usermod/userdel; mount/umount
- Behavior: Allowed if any of the following are true for the user:
  - check_command_permission(user, command)
  - check_command_permission(user, "ALL")
  - check_global_nopasswd_privileges_enhanced(user)
  - check_nopasswd_privileges_enhanced(user)
  If none are true, the command is blocked with guidance. Even when allowed, password prompts occur unless NOPASSWD applies
- Special case: chmod 777 on system paths is always blocked

### 9) Pipelines
- Every pipeline stage must validate/authorize
- Restricted pipeline sinks/sources; insecure pipelines are rejected

### 10) Redirection
- Only to safe locations (HOME, /tmp, /var/tmp by default)
- Redirection into system directories is blocked with a clear error

### 11) Injection mitigation
- Blocks: ;, &&, ||, $(), backticks, unsafe quotes; env assignments; non-ASCII/control chars; overly long commands

### 12) Editor environment overrides
- In-editor actions deemed risky may require a password even if NOPASSWD would otherwise apply (prevents trivial escalation via editor flows)

---

## Sudo-compat specifics
- Invoked as sudo (symlink to sudosh):
  - Supports -V, -v, -p
  - Unsupported sudo flags (-E, -H, -i, -s, -A, -S, -b) are rejected
  - Direct shells are redirected to sudosh interactive shell
  - All commands still pass sudosh validation/authorization

- Invoked as sudosh: no non-suid fallback (see Install policy below)

---

## Install and setuid policy
- make install uses the system sudo explicitly and always targets the real system sudo path, never the sudosh symlink
- sudosh must be installed setuid root to enforce policy; non-suid fallback to exec the system sudo has been removed
- Testing helpers: make test-suid and make clean-suid use the system sudo path (not the symlink)

---

## Logging and auditing
- All decisions and security violations are logged with context
- Session logging (-L) provides comprehensive audit trails

---

## Notes
- Test mode (SUDOSH_TEST_MODE=1) relaxes some privilege requirements for tests but keeps validation/security logic intact
- Defaults are conservative; additional commands can be whitelisted through policy updates

