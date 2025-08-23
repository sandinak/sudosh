# Sudosh v2.1.0 Release Notes

Date: 2025-08-23

## Highlights

- CI hardening: Build Matrix across Ubuntu and macOS with WERROR, ASan/UBSan, and coverage
- Static analysis stability: cppcheck configured to avoid spurious failures
- Sudoers parsing fixes: privilege escalation handling and euid restore correctness
- Security regression tests: pipeline and pager protections validated in CI

## Changes

### Added
- GitHub Actions Build Matrix (WERROR + Sanitizers + Coverage)
- Manual workflow dispatch enabling for on-demand CI runs

### Changed
- Initialize saved_euid in sudoers parsing under UBSan to satisfy -Werror
- Improved test link commands in CI for whitelist and pager security tests
- Makefile: ensure sanitizer flags propagate to link; coverage artifacts via gcovr

### Fixed
- False cppcheck unmatchedSuppression failures when suppression has no active hits
- Ubuntu WERROR failures by checking seteuid() return and cleaning unused variables

## Packaging
- Makefile provides `packages`, `rpm`, and `deb` targets
- Packaging templates present under packaging/ (RPM spec, Debian control/rules scripts)

## Acknowledgments
- Co-authored by Augment Code

## Upgrade Notes
- No breaking changes
- Recommended to rebuild and reinstall to benefit from CI and security test improvements

