# Shell Feature Analysis and Enhancement Plan for sudosh

This document analyzes useful features from bash and zsh and proposes secure, minimal, and maintainable additions to sudosh, aligned with its mission: secure, auditable, sudo-like interactive shell with strong controls.

## Current sudosh capabilities (as of v1.9.4)
- Interactive shell with sudo auth, sudoers validation, and comprehensive logging
- History: persistent ~/.sudosh_history with timestamps; up/down navigation; numeric expansion (!N)
- Tab completion: PATH commands, arguments (files/dirs), cd-only dirs, safe listing for empty prefix, zsh-like "=cmd" expansion and completion
- Prompt: colored user@host:cwd## with detection from PS1/PROMPT; target user awareness
- Alias management: secure creation, storage, import from rc files (opt-in), validation against dangerous commands
- Env management: strict allowlist for export/unset; dangerous vars blocked
- Pipeline execution support; editor/pager secure environment; file locking for editors

## Features from bash/zsh considered
1) Advanced history expansion and search
   - Bash: !-n (relative), !string (last starting with), !?string? (contains), !!
   - Zsh: robust history search; reverse-i-search (Ctrl-R)
   Benefit: Faster recall of prior commands; fewer retypes
   Security: Expansion happens before validate/execute; already logged; safe
   Complexity: Low–moderate
   Compatibility: Extends existing !N; consistent with docs

2) Completion quality
   - Include alias names in command position completion (zsh-like behavior)
   Benefit: Discoverability of aliases and usability
   Security: Aliases already validated; only names surfaced
   Complexity: Low
   Compatibility: Non-breaking

3) Prompt customization (lightweight)
   - Minimal formatting tokens via env var (SUDOSH_PROMPT_FORMAT): %u, %h, %w, %?, %#
   - Show last command status in prompt, common in bash/zsh themes
   Benefit: Clarity during admin sessions; integrates with user workflows
   Security: Read-only formatting; no expansion of untrusted code
   Complexity: Low
   Compatibility: Default prompt unchanged if env not set

4) Reverse incremental search helper (Ctrl-R)
   - Provide a non-interactive helper history_search_last_index("needle") to support Ctrl-R
   Benefit: Enables testable search core; future UI can hook into read_command loop
   Security: Read-only; no execution
   Complexity: Low
   Compatibility: Internal

Deferred/not prioritized (due to scope or security/maintainability):
- Complex parameter expansions, glob qualifiers, process substitution
- Full programmable completion framework
- Jobs control (suspend/fg/bg) as it complicates privilege and audit semantics
- Shell options set/unset; arithmetic evaluation; extended globbing

## Implemented in this change
- History expansion: support !-N and !prefix (in addition to existing !N)
- History search helper: history_search_last_index()
- Command completion includes alias names
- Prompt customization: SUDOSH_PROMPT_FORMAT with %u %h %w %? %# tokens; last exit status wired
- Manpage updates for new history forms and prompt customization

## Tests
- Existing unit tests cover utils, shell enhancements, and logging; these changes are additive and minimal.
- Recommend adding unit tests:
  - expand_history: !-1, !-2, !prefix behaviors (via logging.c functions)
  - complete_command includes alias names (inject a test alias; call complete_command; assert name present)
  - history_search_last_index basic cases
  - prompt: verify format token replacement via small helper or end-to-end by setting SUDOSH_PROMPT_FORMAT and checking printed line (optional; lower priority due to tty)

## Backward compatibility and security
- Backward compatible: defaults unchanged; new behaviors only when invoked (!-N, !prefix, env var)
- Security invariant: expansion remains before validation and execution; no new code execution vectors; env allowlist unchanged
- Maintenance: contained changes in logging.c, utils.c, shell_enhancements.c, headers and docs; no new deps

## Next steps (optional)
- Add interactive Ctrl-R UI using history_search_last_index core with safe key handling
- Add !?substring? if desired (contains search)
- Config flag to toggle prompt customization feature if needed

