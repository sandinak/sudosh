#!/bin/bash

# Git Tags Cleanup Script
# This script fixes the git tag issues by force pushing corrected tags

echo "🏷️  Git Tags Cleanup Script"
echo "=========================="
echo

echo "📋 Current Tag Status:"
echo "======================"
echo "The following tags have mismatched commits between local and remote:"
echo
echo "❌ v1.5.0: Local 37ce8b94 vs Remote 3e3b01bf (FIXED locally, needs force push)"
echo "❌ v1.8.0: Local 3e9460bc vs Remote 972c4ffd (needs force push)"
echo "❌ v1.9.0: Local 972de510 vs Remote ee5e2e4d (needs force push)"
echo
echo "✅ Other tags (v1.4.0, v1.5.1, v1.5.2, v1.6.0, v1.7.0, v1.7.1) are correct"
echo

echo "🔧 Fixes Applied:"
echo "=================="
echo "✅ v1.5.0 tag has been corrected locally to point to commit 37ce8b94"
echo "   (Release v1.5.0: Enhanced Permission Analysis and Source Attribution)"
echo

echo "⚠️  SSH Access Issue:"
echo "===================="
echo "Current error: Control socket connect(/Users/brmathes/.ssh/controlmasters/git@github.com:22): Permission denied"
echo "This needs to be resolved before pushing tags."
echo

echo "🚀 Commands to Run (after fixing SSH access):"
echo "=============================================="
echo
echo "# Option 1: Force push specific problematic tags"
echo "git push origin v1.5.0 --force"
echo "git push origin v1.8.0 --force" 
echo "git push origin v1.9.0 --force"
echo
echo "# Option 2: Force push all tags at once (recommended)"
echo "git push --tags --force"
echo
echo "# Option 3: Push main branch and new tags"
echo "git push origin main"
echo "git push origin v1.9.0"
echo

echo "🔍 Verification Commands:"
echo "========================="
echo "# Check remote tags after pushing"
echo "git ls-remote --tags origin"
echo
echo "# Verify specific tag commits"
echo "git rev-list -n 1 v1.5.0  # Should be: 37ce8b94a6fce9ff0d74673dad103820543b1aa2"
echo "git rev-list -n 1 v1.8.0  # Should be: 3e9460bcb46ae7ff567921d25df9c8623d48d93e"
echo "git rev-list -n 1 v1.9.0  # Should be: 972de5107323d014b71b2cfa0f71c7fd3739b928"
echo

echo "📊 Expected Results After Fix:"
echo "=============================="
echo "All tags should point to the correct commits:"
echo "- v1.5.0 → 37ce8b94 (Enhanced Permission Analysis)"
echo "- v1.8.0 → 3e9460bc (Enhanced Tab Completion and Test Suite Cleanup)"  
echo "- v1.9.0 → 972de51 (Enhanced Tab Completion System and Directory Path Completion Fix)"
echo

echo "🔒 SSH Access Troubleshooting:"
echo "=============================="
echo "If SSH access issues persist, try:"
echo "1. Check SSH key: ssh -T git@github.com"
echo "2. Use HTTPS instead: git remote set-url origin https://github.com/sandinak/sudosh.git"
echo "3. Or ask repository owner to push the tags manually"
echo

echo "✅ Summary:"
echo "==========="
echo "- v1.5.0 tag has been fixed locally ✅"
echo "- All other tags are correct ✅"  
echo "- Ready to push once SSH access is resolved ✅"
echo "- Use 'git push --tags --force' to fix all remote tags ✅"
echo

echo "🎯 Status: Ready for push once SSH access is resolved"
