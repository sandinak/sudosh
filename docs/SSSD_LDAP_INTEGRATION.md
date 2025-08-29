SSSD/LDAP Integration Status and Guidance

Overview
- Sudosh aims for full sudo parity for SSSD/LDAP managed rules without relying on `sudo -l`.
- Current state: library-first SSSD sudo integration is implemented (dynamic `libsss_sudo.so`); a segmented sudo responder socket fallback is provided for hosts without the library. No `sudo -l` or `getent` calls are used.

Behavior and order of operations
1) Try to load `libsss_sudo.so` (symbols: `sss_sudo_send_recv`, etc.). If present, we use it to fetch sudo rules and convert to Sudosh’s internal structures.
2) If the library is unavailable or incomplete, connect to the SSSD sudo responder socket using the segmented protocol (native-endian headers + NUL-delimited segments) observed in the field.
3) Parse TLVs for `sudoCommand`, `sudoRunAsUser`, and options (e.g., `!authenticate`). A heuristic string scan is used as a last resort for environments that interleave LDAP attributes.

Environment flags (for debugging/forcing behavior)
- `SUDOSH_SSSD_FORCE_SOCKET=1`: skip libsss probe and use the socket path.
- `SUDOSH_SSSD_SOCKET_SEGMENTED=1`: ensure segmented mode is used (helpful on responders that require it).
- `SUDOSH_DEBUG_SSSD=1`: verbose logging for socket I/O, headers, payload prefixes (hex dump).
- `SUDOSH_SSSD_REPLAY=/path/to/trace`: developer-only replay of captured exchanges for offline debugging.

Security considerations
- Sudosh never shells out to `sudo -l` or `getent` for rule discovery, preventing privilege confusion and TOCTOU concerns.
- Setuid/escalation for SSSD queries is minimal and dropped promptly; operations are logged with detail when debugging is enabled.

What works today
- Library-first and socket fallback rule discovery.
- NSS-based user/group checks for sudo-like decisions without sudo.
- Rule output command categorization (Always Safe, Always Blocked, Conditionally Blocked) consistent with tests.

Roadmap: full rule parity
- Implement complete SSSD rule semantics (order, negation, host/netgroup, Cmnd_Alias, RunAs constraints) mirroring sudo.
- Expand fixtures and regression tests to achieve parity across LDAP-backed deployments.

How to run without sudo -l
- Use `sudosh -r/--rules` to display effective rule categories; these do not call sudo.
- Ensure SSSD and nsswitch are configured; Sudosh will not shell out to sudo.

Testing notes
- CI tests validate TLV decoding and do not require a live SSSD responder.
- Security tests enforce sudo-compat mode behaviors (e.g., reject `-E`) and hardened editors/shells logic.

Contributing / Feedback
- Please share your SSSD sudo schema usage (filters, attributes, host/network specs). We’ll extend parsing and tests to cover your scenarios.
