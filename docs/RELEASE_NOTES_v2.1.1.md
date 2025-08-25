# sudosh v2.1.1 Release Notes

Date: 2025-08-25

## Highlights
- Fixes a regression that broke `--version` and clarifies `-V` support under sudo-compat mode
- Adds `-p, --prompt` compatibility for custom password prompts and documents it
- Updates manpage and help output accordingly
- Adds tests for version and prompt behavior

## Changes

### Fixed
- Consolidated CLI parsing for `--version` and `-V` into a single branch to avoid fall-through

### Added
- Sudo-compat `-V` support (when invoked as `sudo`)
- `-p, --prompt` option to customize password prompts; available regardless of setuid

### Documentation
- Updated help output to include `-p/--prompt`
- Manpage updated to document `-p/--prompt` in sudo-compat mode

### Testing
- Unit test: tests/unit/test_prompt_option.c verifies setter/getter
- Integration test: tests/integration/test_sudo_compat.c validates `-p` behavior and `-V`/`--version`

## Installation

### From Source
```bash
make
sudo make install
```

### Packages
- RPM (DNF-based): built on Fedora in CI; artifacts attached to the GitHub Release
- DEB (APT-based): built on Ubuntu in CI; artifacts attached to the GitHub Release

## Credits
- Co-authored by Augment Code (https://www.augmentcode.com/?utm_source=github&utm_medium=release_notes&utm_campaign=attribution)

