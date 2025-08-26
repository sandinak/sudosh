SSSD/LDAP Integration Status and Guidance

Overview
- Sudosh aims for full sudo parity for SSSD/LDAP managed rules without relying on `sudo -l`.
- Current state: foundational SSSD plumbing exists and NSS fallbacks work, but comprehensive rule parsing like sudoers/LDAP is in progress.

Goals
- Parse SSSD/LDAP sudo rules directly (host/user/netgroup cmnd specs), similar to sudo’s internals.
- No dependency on `sudo -l` for rule discovery.
- Deterministic, testable behavior with security-first defaults.

What works today
- NSS-based user/group checks for sudo-like decisions (without sudo).
- Rule output command categorization (Always Safe, Always Blocked, Conditionally Blocked) consistent with v2 tests.
- Sudo-compat mode behavior for flags, without `sudo -E` support by design.

What’s coming
- Full SSSD rule parsing, including order/negation precedence, per-sudoers semantics.
- Remote directory and netgroup lookups with safe timeouts and caching.
- Extensive unit/integration/regression tests and docs.

How to run without sudo -l
- Use sudosh -r/--rules to display effective categories; these do not call sudo.
- For LDAP/SSSD environments, ensure sssd and nsswitch are configured; sudosh will not shell out to sudo.

Testing notes
- CI tests do not require SSSD; they validate NSS fallbacks and rule output formatting.
- Security tests enforce that sudo-compat mode rejects `-E` and that insecure shells/editors are blocked.

Contact/Contrib
- Please file issues/PRs describing your SSSD layout (filters, cn, host/network specs).
- We will expand the parser with fixtures to cover your scenarios.

