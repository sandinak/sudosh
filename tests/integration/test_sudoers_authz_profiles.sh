#!/usr/bin/env bash
# Integration tests for sudoers authorization profiles in sudosh
# Validates command permissions, NOPASSWD behavior, and -l/-ll listing for multiple user types

set -euo pipefail

BLUE='\033[0;34m'; GREEN='\033[0;32m'; RED='\033[0;31m'; YELLOW='\033[1;33m'; NC='\033[0m'
SUDOSH_BIN="${SUDOSH_BIN:-./bin/sudosh}"
TMP_ROOT="$(mktemp -d)"
SUDOERS_FILE="$TMP_ROOT/sudoers"
SUDOERS_DIR="$TMP_ROOT/sudoers.d"

mkdir -p "$SUDOERS_DIR"
cleanup() {
  rm -rf "$TMP_ROOT"
}
trap cleanup EXIT

# Use sudosh test mode and override sudoers paths
export SUDOSH_TEST_MODE=1
export SUDOSH_SUDOERS_PATH="$SUDOERS_FILE"
export SUDOSH_SUDOERS_DIR="$SUDOERS_DIR"

current_user() { id -un; }
run_with_timeout() {
  local seconds="$1"; shift
  if command -v timeout >/dev/null 2>&1; then timeout "$seconds" "$@"; elif command -v gtimeout >/dev/null 2>&1; then gtimeout "$seconds" "$@"; else "$@"; fi
}

# Helpers
add_rule() { echo "$1" >> "$SUDOERS_FILE"; }
add_inc()  { echo "$1" > "$SUDOERS_DIR/$2"; }
reset_sudoers() { : > "$SUDOERS_FILE"; find "$SUDOERS_DIR" -type f -maxdepth 1 -delete 2>/dev/null || true; }

# Execute sudosh -c and capture exit and output
exec_cmd() {
  local cmd="$1"; shift || true
  local out
  set +e
  out=$(run_with_timeout 5 "$SUDOSH_BIN" -v -c "$cmd" 2>&1)
  local rc=$?
  set -e
  echo "$rc|||$out"
}

# Check -l output quick
list_rules() { run_with_timeout 5 "$SUDOSH_BIN" -l 2>&1 || true; }
# Check -ll output
list_rules_detailed() { run_with_timeout 5 "$SUDOSH_BIN" -ll 2>&1 || true; }

assert_contains() {
  local haystack="$1" needle="$2" desc="$3"
  if echo "$haystack" | grep -Fq "$needle"; then
    echo -e "  ${GREEN}PASS${NC}: $desc"
  else
    echo -e "  ${RED}FAIL${NC}: $desc"
    echo "$haystack" | sed 's/^/    | /'
    return 1
  fi
}

assert_not_contains() {
  local haystack="$1" needle="$2" desc="$3"
  if echo "$haystack" | grep -Fq "$needle"; then
    echo -e "  ${RED}FAIL${NC}: $desc (found '$needle')"
    echo "$haystack" | sed 's/^/    | /'
    return 1
  else
    echo -e "  ${GREEN}PASS${NC}: $desc"
  fi
}

assert_rc() {
  local got="$1" want="$2" desc="$3"
  if [ "$got" = "$want" ]; then
    echo -e "  ${GREEN}PASS${NC}: $desc (rc=$got)"
  else
    echo -e "  ${RED}FAIL${NC}: $desc (rc=$got want=$want)"
    return 1
  fi
}

assert_blocked_output() {
  local out="$1" desc="$2"
  if echo "$out" | grep -Eqi "not allowed|requires sudo|not in the sudoers"; then
    echo -e "  ${GREEN}PASS${NC}: $desc"
  else
    echo -e "  ${RED}FAIL${NC}: $desc"
    echo "$out" | sed 's/^/    | /'
    return 1
  fi
}

# Scenario 1: Unrestricted Admin User (NOPASSWD: ALL)
scenario_admin_nopasswd() {
  echo -e "${BLUE}Scenario 1: Unrestricted Admin (NOPASSWD ALL)${NC}"
  reset_sudoers
  add_rule "$(current_user) ALL=(ALL) NOPASSWD: ALL"

  local res out rc
  res=$(exec_cmd "id") ; rc=${res%%|||*} ; out=${res#*|||}
  assert_rc "$rc" 0 "NOPASSWD admin can run id without auth"

  # Dangerous but should run
  res=$(exec_cmd "/usr/sbin/iptables -L") ; rc=${res%%|||*} ; out=${res#*|||}
  # May not exist; accept rc 0 or non-zero but not blocked by auth
  if echo "$out" | grep -qi "authentication required\|failed"; then
    echo -e "  ${RED}FAIL${NC}: Should not prompt for password for iptables"
    return 1
  else
    echo -e "  ${GREEN}PASS${NC}: No password prompt for iptables"
  fi

  local list
  list=$(list_rules)
  assert_contains "$list" "Sudo privileges for" "-l header present"
  assert_contains "$list" "Direct Sudoers Rules" "-l direct rules section present"
  assert_contains "$list" "$SUDOERS_FILE" "-l attributes rule source to temp sudoers file"
  assert_contains "$list" "NOPASSWD:" "-l shows NOPASSWD flag for ALL"
  assert_not_contains "$list" "ANY (NOPASSWD)" "-l does not render unexpected summary string"

  # Detailed listing (-ll) includes safe/blocked sections
  local listd
  listd=$(list_rules_detailed)
  assert_contains "$listd" "Always safe commands" "-ll details contain safe commands"
  assert_contains "$listd" "Command Security Controls" "-ll details contain blocked command categories"
}

# Scenario 2: Standard Admin (ALL requires password)
scenario_admin_pw() {
  echo -e "${BLUE}Scenario 2: Standard Admin (ALL with password)${NC}"
  reset_sudoers
  add_rule "$(current_user) ALL=(ALL) ALL"

  local res out rc
  res=$(exec_cmd "id") ; rc=${res%%|||*} ; out=${res#*|||}
  # In test mode authenticate_user() returns failure; expect auth required path -> failure
  assert_rc "$rc" 1 "Password-required admin fails in test mode (no auth)"
  assert_contains "$out" "authentication failed" "Output indicates authentication failure"

  local list
  list=$(list_rules)
  assert_contains "$list" "ANY" "-l shows ANY without NOPASSWD"
  assert_not_contains "$list" "NOPASSWD:" "-l does not show NOPASSWD"

  local listd
  listd=$(list_rules_detailed)
  assert_contains "$listd" "Always safe commands" "-ll details present for standard admin"
}

# Scenario 3: Docker User (only docker/compose)
scenario_docker_user() {
  echo -e "${BLUE}Scenario 3: Docker user${NC}"
  reset_sudoers
  add_rule "$(current_user) ALL=(ALL) NOPASSWD: /usr/bin/docker, /usr/bin/docker-compose"

  local res out rc
  res=$(exec_cmd "/usr/bin/docker ps") ; rc=${res%%|||*} ; out=${res#*|||}
  # Execution allowed; may fail due to docker not running, but must not be denied by policy
  if echo "$out" | grep -Eqi "not allowed|requires sudo|not in the sudoers"; then
    echo -e "  ${RED}FAIL${NC}: docker should be permitted by policy"
    echo "$out" | sed 's/^/    | /'
    return 1
  else
    echo -e "  ${GREEN}PASS${NC}: docker allowed per sudoers"
  fi

  res=$(exec_cmd "fdisk -l") ; rc=${res%%|||*} ; out=${res#*|||}
  assert_blocked_output "$out" "non-docker privileged command blocked"

  local list
  list=$(list_rules)
  assert_contains "$list" "/usr/bin/docker" "-l shows docker rule"
  assert_contains "$list" "/usr/bin/docker-compose" "-l shows docker-compose rule"
  assert_contains "$list" "$SUDOERS_FILE" "-l attributes docker rules to source file"
}

# Scenario 4: System Service User (systemctl start/stop/restart)
scenario_systemctl_user() {
  echo -e "${BLUE}Scenario 4: Systemctl service user${NC}"
  reset_sudoers
  add_rule "$(current_user) ALL=(ALL) NOPASSWD: /usr/bin/systemctl restart *, /usr/bin/systemctl start *, /usr/bin/systemctl stop *"

  local res out rc
  res=$(exec_cmd "/usr/bin/systemctl restart sshd") ; rc=${res%%|||*} ; out=${res#*|||}
  if echo "$out" | grep -Eqi "not allowed|not in the sudoers|requires sudo"; then
    echo -e "  ${RED}FAIL${NC}: systemctl restart should be allowed"
    echo "$out" | sed 's/^/    | /'
    return 1
  else
    echo -e "  ${GREEN}PASS${NC}: systemctl restart allowed"
  fi

  res=$(exec_cmd "iptables -L") ; rc=${res%%|||*} ; out=${res#*|||}
  assert_blocked_output "$out" "unrelated privileged command blocked"

  local list
  list=$(list_rules)
  assert_contains "$list" "/usr/bin/systemctl restart *" "-l shows restart rule"
  assert_contains "$list" "/usr/bin/systemctl start *" "-l shows start rule"
  assert_contains "$list" "/usr/bin/systemctl stop *" "-l shows stop rule"
}

# Scenario 5: Dangerous Commands User (delegated NOPASSWD)
scenario_dangerous_user() {
  echo -e "${BLUE}Scenario 5: Dangerous commands delegated (NOPASSWD)${NC}"
  reset_sudoers
  add_rule "$(current_user) ALL=(ALL) NOPASSWD: /usr/sbin/iptables, /usr/bin/rm -rf *, /usr/sbin/fdisk"

  local res out rc
  res=$(exec_cmd "/usr/sbin/iptables -L") ; rc=${res%%|||*} ; out=${res#*|||}
  if echo "$out" | grep -qi "authentication required\|failed"; then
    echo -e "  ${RED}FAIL${NC}: iptables should not ask for password"
    return 1
  else
    echo -e "  ${GREEN}PASS${NC}: iptables allowed without password"
  fi

  res=$(exec_cmd "whoami") ; rc=${res%%|||*} ; out=${res#*|||}
  # whoami is safe; should run regardless. We only verify profile didn't break basics
  assert_rc "$rc" 0 "whoami still runs as safe command"

  local list
  list=$(list_rules)
  assert_contains "$list" "/usr/sbin/iptables" "-l shows iptables delegated"
  assert_contains "$list" "/usr/bin/rm -rf *" "-l shows rm -rf delegation"
  assert_contains "$list" "/usr/sbin/fdisk" "-l shows fdisk delegated"
}

main() {
  echo -e "${BLUE}=== Sudoers Authorization Profiles Tests ===${NC}"
  local failures=0
  scenario_admin_nopasswd || failures=$((failures+1))
  scenario_admin_pw || failures=$((failures+1))
  scenario_docker_user || failures=$((failures+1))
  scenario_systemctl_user || failures=$((failures+1))
  scenario_dangerous_user || failures=$((failures+1))

  if [ "$failures" -eq 0 ]; then
    echo -e "${GREEN}All sudoers authorization profile tests passed${NC}"
    exit 0
  else
    echo -e "${RED}$failures profile test(s) failed${NC}"
    exit 1
  fi
}

main "$@"

