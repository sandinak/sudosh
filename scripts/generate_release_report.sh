#!/usr/bin/env bash
set -euo pipefail

# Generate release report under reports/v$VERSION with test and coverage summaries
VERSION=${1:-"$(grep -E '^VERSION \?= ' Makefile | awk '{print $3}')"}
REPORT_DIR="reports/v${VERSION}"
mkdir -p "$REPORT_DIR"

# Build with coverage and run test suite
make clean >/dev/null
make -j4 WERROR=1 COVERAGE=1 tests >/dev/null

# Run all tests capturing logs
TEST_LOG="$REPORT_DIR/test_results.txt"
echo "Running full test suite (SUDOSH_TEST_MODE=1)..." | tee "$TEST_LOG"
for t in bin/test_*; do
  echo "Running $t..." | tee -a "$TEST_LOG"
  SUDOSH_TEST_MODE=1 "$t" >>"$TEST_LOG" 2>&1 || { echo "FAIL: $t" | tee -a "$TEST_LOG"; exit 1; }
  echo "" >>"$TEST_LOG"
done

# Coverage report
COV_LOG="$REPORT_DIR/coverage.txt"
make coverage-report COVERAGE=1 WERROR=1 >"$COV_LOG" 2>&1 || true

# Static analysis (optional)
STATIC_LOG="$REPORT_DIR/static_analysis.txt"
make static-analysis >"$STATIC_LOG" 2>&1 || echo "cppcheck not available" >"$STATIC_LOG"

# Git summary of changes since last tag or previous release notes
GIT_LOG="$REPORT_DIR/changes_since_last_release.txt"
if git describe --tags --abbrev=0 >/dev/null 2>&1; then
  LAST_TAG=$(git describe --tags --abbrev=0)
  echo "Changes since $LAST_TAG:" >"$GIT_LOG"
  git --no-pager log --oneline "$LAST_TAG"..HEAD >>"$GIT_LOG"
else
  echo "Changes in current branch:" >"$GIT_LOG"
  git --no-pager log --oneline -n 200 >>"$GIT_LOG"
fi

# Quick summary file
SUMMARY="$REPORT_DIR/summary.md"
cat >"$SUMMARY" <<EOF
# Pre-release Summary for v${VERSION}

- Version: v${VERSION}
- Date: $(date -u +%Y-%m-%dT%H:%M:%SZ)
- Branch: $(git rev-parse --abbrev-ref HEAD)

## Build & Tests
- Build: C99 with -Wall -Wextra -Werror
- All unit/integration/regression tests executed; see test_results.txt

## Coverage
- See coverage.txt (gcov/llvm-cov output)

## Static Analysis
- See static_analysis.txt

## Changes
- See changes_since_last_release.txt

## Notes
- Manual review: security, SSSD integration, sudoers parsing, env policy
- Known issues: TBD
- Follow-ups: TBD
EOF

echo "Report generated in $REPORT_DIR"
