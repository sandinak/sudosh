# Sudosh Test Suite

This directory contains security-focused test scripts intended to catch command injection, environment tampering, and known bash/sudo CVEs.

Key files:
- security/security_cve_tests.sh – end-to-end CVE and hardening checks

Notes on the test harness:
- The test script attempts to run each command with a timeout.
  - It prefers `timeout` (GNU coreutils) or `gtimeout` (Homebrew on macOS).
  - If neither is installed, it falls back to a simple background process with a watcher that kills the PID on timeout.
  - If `setsid` is available, the watcher will terminate the entire process group for robustness; otherwise it targets only the PID.
- The script forces non-interactive behavior by exporting `SUDOSH_TEST_MODE=1` when launching commands, so tests do not require TTY or password prompts.
- When a test fails, the harness records the exit code and stderr snippet in `/tmp/sudosh_security_test.log` to help diagnose issues.

Recommendations:
- Install one of `timeout` or `gtimeout` for more robust timeout handling.
- If you see `setsid: command not found` errors in the log, install `util-linux` (Linux) or rely on the fallback which targets only the child PID.
- You can tail the diagnostic log: `tail -n 200 /tmp/sudosh_security_test.log`.

CI setup note:
- On macOS runners, ensure `gtimeout` is installed (e.g., `brew install coreutils`).
- On Linux runners, ensure `timeout` is available (coreutils) and optionally `util-linux` for `setsid`.
- Export `SUDOSH_TEST_MODE=1` for CI runs so tests remain non-interactive.

Example GitHub Actions workflow:

```yaml
name: Sudosh Tests

on:
  push:
  pull_request:

jobs:
  test:
    runs-on: ${{ matrix.os }}
    strategy:
      matrix:
        os: [ubuntu-latest, macos-latest]
    steps:
      - name: Checkout
        uses: actions/checkout@v4

      - name: Install dependencies (macOS)
        if: runner.os == 'macOS'
        run: |
          brew update
          brew install coreutils

      - name: Install dependencies (Ubuntu)
        if: runner.os == 'Linux'
        run: |
          sudo apt-get update
          # coreutils is preinstalled on ubuntu-latest but ensure util-linux for setsid
          sudo apt-get install -y util-linux

      - name: Build
        run: make -j4

      - name: Run CVE security tests
        env:
          SUDOSH_TEST_MODE: '1'
        run: ./tests/security/security_cve_tests.sh
```

