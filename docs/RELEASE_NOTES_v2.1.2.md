# Sudosh v2.1.2 Release Notes

Date: 2025-08-28

## Highlights

- Security: Tilde (~) redirection to $HOME is now allowed while absolute /root and /var/root remain blocked
- Reliability: Enforce a sane minimum terminal height to reduce CI flakiness for pager tests
- Detection: Fine-tune Ansible detection threshold to reduce false negatives in CI-like environments
- Tests: Added unit test covering tilde expansion redirection behavior
- Docs: Manpage and changelog updated

## Changes

### Security
- Redirection validation now distinguishes between `~` expansion (allowed to $HOME) and explicit absolute targets like `/root` or `/var/root` (still blocked). This maintains strong protections while enabling standard shell semantics for HOME-based redirects.

### Reliability
- `get_terminal_height()` now guarantees a minimum height to avoid edge-case zeros reported in some CI environments.

### Ansible Detection
- Treat `context_score >= 20` as sufficient when at least one Ansible-related hint is present, improving detection in constrained environments.

### Testing
- New unit test validates `is_safe_redirection_target("~/ok.txt") == 1` while `is_safe_redirection_target("/root/bad.txt") == 0`.

### Documentation
- Manpage updated to explicitly document tilde (~) behavior for redirection.
- Changelog updated.

## Validation

- Built with `WERROR=1` on Ubuntu and macOS toolchains
- Full test suite passes locally: `make -j test`
- CI: GitHub Actions matrix (Ubuntu gcc/clang, macOS clang) green

## Upgrade Notes

- No configuration changes required
- Behavior is strictly more permissive for HOME-only tilde redirection while maintaining system path blocks

## Acknowledgements

Co-authored by Augment Code

