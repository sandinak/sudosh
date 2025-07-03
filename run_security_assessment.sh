#!/bin/bash

# Comprehensive Security Assessment Script for sudosh
# This script runs all security tests and generates a detailed report

echo "🔒 Sudosh Security Assessment Suite"
echo "=================================="
echo

# Check if security tests are built
if [ ! -d "bin" ] || [ ! -f "bin/test_security_command_injection" ]; then
    echo "Building security tests..."
    make security-tests
    if [ $? -ne 0 ]; then
        echo "❌ Failed to build security tests"
        exit 1
    fi
    echo "✅ Security tests built successfully"
    echo
fi

# Create logs directory
mkdir -p /tmp/sudosh_security_logs
LOG_DIR="/tmp/sudosh_security_logs"

echo "📋 Running Security Assessment..."
echo "Logs will be saved to: $LOG_DIR"
echo

# Initialize summary variables
TOTAL_TESTS=0
TOTAL_PASSED=0
TOTAL_FAILED=0
CRITICAL_ISSUES=0

# Function to run a security test and capture results
run_security_test() {
    local test_name="$1"
    local test_binary="$2"
    local category="$3"
    
    echo "🔍 Testing: $test_name"
    echo "   Category: $category"
    
    # Run the test and capture output
    local output_file="$LOG_DIR/${test_binary##*/}.log"
    ./$test_binary > "$output_file" 2>&1
    local exit_code=$?
    
    # Parse results
    local tests=$(grep "Total tests:" "$output_file" | sed 's/.*Total tests: //')
    local passed=$(grep "Secure.*:" "$output_file" | sed 's/.*: //')
    local failed=$(grep "Vulnerable.*:" "$output_file" | sed 's/.*: //')
    
    # Update totals
    TOTAL_TESTS=$((TOTAL_TESTS + tests))
    TOTAL_PASSED=$((TOTAL_PASSED + passed))
    TOTAL_FAILED=$((TOTAL_FAILED + failed))
    
    # Determine severity
    if [ "$failed" -gt 0 ]; then
        if [[ "$category" == *"Command Injection"* ]] || [[ "$category" == *"Privilege Escalation"* ]]; then
            CRITICAL_ISSUES=$((CRITICAL_ISSUES + failed))
            echo "   Status: ❌ CRITICAL - $failed vulnerabilities found"
        else
            echo "   Status: ⚠️  WARNING - $failed vulnerabilities found"
        fi
    else
        echo "   Status: ✅ SECURE - No vulnerabilities detected"
    fi
    
    echo "   Results: $tests tests, $passed passed, $failed failed"
    echo
    
    return $exit_code
}

# Run all security test categories
echo "Starting comprehensive security assessment..."
echo

run_security_test "Command Injection Vulnerabilities" \
                  "bin/test_security_command_injection" \
                  "Command Injection"

run_security_test "Privilege Escalation Attacks" \
                  "bin/test_security_privilege_escalation" \
                  "Privilege Escalation"

run_security_test "Authentication Bypass Attempts" \
                  "bin/test_security_auth_bypass" \
                  "Authentication Bypass"

run_security_test "Logging and Monitoring Evasion" \
                  "bin/test_security_logging_evasion" \
                  "Logging Evasion"

run_security_test "Race Conditions and Timing Attacks" \
                  "bin/test_security_race_conditions" \
                  "Race Conditions"

# Generate summary report
echo "📊 Security Assessment Summary"
echo "=============================="
echo
echo "Total Security Tests Run: $TOTAL_TESTS"
echo "Tests Passed (Secure): $TOTAL_PASSED"
echo "Tests Failed (Vulnerable): $TOTAL_FAILED"
echo "Critical Security Issues: $CRITICAL_ISSUES"
echo

# Calculate percentages
if [ $TOTAL_TESTS -gt 0 ]; then
    PASS_PERCENT=$((TOTAL_PASSED * 100 / TOTAL_TESTS))
    FAIL_PERCENT=$((TOTAL_FAILED * 100 / TOTAL_TESTS))
    echo "Security Score: $PASS_PERCENT% secure ($FAIL_PERCENT% vulnerable)"
else
    echo "Security Score: Unable to calculate (no tests run)"
fi

echo

# Overall security assessment
if [ $TOTAL_FAILED -eq 0 ]; then
    echo "🎉 OVERALL ASSESSMENT: SECURE"
    echo "   No security vulnerabilities detected!"
    echo "   sudosh appears to be well-protected against common attacks."
elif [ $CRITICAL_ISSUES -gt 0 ]; then
    echo "🚨 OVERALL ASSESSMENT: CRITICAL VULNERABILITIES DETECTED"
    echo "   $CRITICAL_ISSUES critical security issues found!"
    echo "   Immediate action required before production use."
elif [ $TOTAL_FAILED -le 5 ]; then
    echo "⚠️  OVERALL ASSESSMENT: MINOR SECURITY ISSUES"
    echo "   $TOTAL_FAILED security issues found."
    echo "   Review and address before production use."
else
    echo "❌ OVERALL ASSESSMENT: MULTIPLE SECURITY VULNERABILITIES"
    echo "   $TOTAL_FAILED security vulnerabilities detected!"
    echo "   Comprehensive security review required."
fi

echo

# Recommendations
echo "🔧 Security Recommendations"
echo "==========================="
echo

if [ $TOTAL_FAILED -gt 0 ]; then
    echo "Immediate Actions Required:"
    echo "1. Review detailed logs in $LOG_DIR"
    echo "2. Address all critical vulnerabilities first"
    echo "3. Implement input validation and sanitization"
    echo "4. Review privilege management mechanisms"
    echo "5. Enhance logging and monitoring capabilities"
    echo
    
    echo "Security Hardening Checklist:"
    echo "□ Implement strict input validation"
    echo "□ Add command whitelist/blacklist filtering"
    echo "□ Enhance environment variable sanitization"
    echo "□ Improve privilege dropping mechanisms"
    echo "□ Strengthen authentication validation"
    echo "□ Add comprehensive audit logging"
    echo "□ Implement rate limiting for authentication"
    echo "□ Add integrity checks for configuration files"
    echo
else
    echo "Maintenance Recommendations:"
    echo "1. Continue regular security assessments"
    echo "2. Monitor for new attack vectors"
    echo "3. Keep security tests updated"
    echo "4. Review logs regularly for anomalies"
    echo
fi

# Generate detailed report
echo "📄 Generating Detailed Security Report..."
if [ -x "bin/test_security_comprehensive" ]; then
    ./bin/test_security_comprehensive > "$LOG_DIR/comprehensive_report.log" 2>&1
    echo "   Comprehensive report: $LOG_DIR/comprehensive_report.log"
fi

echo "   Individual test logs: $LOG_DIR/"
echo "   Vulnerability details: /tmp/sudosh_vulnerabilities.log"

echo

# Final recommendations based on results
if [ $CRITICAL_ISSUES -gt 0 ]; then
    echo "🚨 CRITICAL: Do not deploy sudosh in production until critical issues are resolved!"
    exit 2
elif [ $TOTAL_FAILED -gt 0 ]; then
    echo "⚠️  WARNING: Address security issues before production deployment."
    exit 1
else
    echo "✅ SUCCESS: sudosh passed all security tests!"
    exit 0
fi
