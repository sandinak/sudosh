# Enhanced 'which' Command Implementation

## Overview

The `which` command in sudosh has been enhanced to identify both binaries in the PATH and any loaded aliases, providing a comprehensive command lookup functionality similar to modern shells.

## Implementation Details

### Files Modified

1. **sudosh/src/shell_env.c**
   - Enhanced `handle_which_command()` function to check for aliases
   - Added alias checking between builtin and PATH binary checks

2. **sudosh/src/sudosh.h**
   - Uncommented `extern char **environ;` declaration to fix compilation

### Code Changes

#### Before (Original Implementation)
```c
if (!is_builtin) {
    /* Find command in PATH */
    char *path = find_command_in_path(token);
    if (path) {
        printf("%s\n", path);
        free(path);
    } else {
        printf("%s: not found\n", token);
        found_all = 0;
    }
}
```

#### After (Enhanced Implementation)
```c
if (!is_builtin) {
    /* Check if it's an alias */
    char *alias_value = get_alias_value(token);
    if (alias_value) {
        printf("%s: aliased to `%s'\n", token, alias_value);
    } else {
        /* Find command in PATH */
        char *path = find_command_in_path(token);
        if (path) {
            printf("%s\n", path);
            free(path);
        } else {
            printf("%s: not found\n", token);
            found_all = 0;
        }
    }
}
```

## Functionality

### Search Order
The enhanced `which` command now searches in this order:

1. **Built-in commands** (cd, pwd, exit, etc.)
2. **Aliases** (user-defined command shortcuts)
3. **Binaries in PATH** (executable files in system directories)
4. **Not found** (if none of the above match)

### Output Format

- **Built-in**: `cd: shell builtin`
- **Alias**: `ll: aliased to 'ls -la'`
- **Binary**: `/bin/ls`
- **Not found**: `nonexistent: not found`

### Multiple Commands
The command supports multiple arguments:
```bash
which cd ll ls nonexistent
```

## Testing

### Automated Tests
- `test_which_standalone.c`: Basic functionality verification
- Enhanced test in `tests/test_shell_enhancements.c`

### Manual Testing
1. Run `./bin/sudosh`
2. Create aliases: `alias ll='ls -la'`
3. Test various command types:
   - `which cd` (builtin)
   - `which ll` (alias)
   - `which ls` (binary)
   - `which nonexistent` (not found)

### Demo Scripts
- `demo_enhanced_which.sh`: Comprehensive demonstration
- `simple_which_test.sh`: Basic testing setup

## Integration with Existing System

### Alias System Integration
The enhancement leverages the existing alias system:
- Uses `get_alias_value()` function from `shell_enhancements.c`
- Integrates with alias loading from `~/.sudosh_aliases`
- Maintains compatibility with existing alias commands

### Compatibility
- Maintains backward compatibility with standard `which` behavior
- Preserves all existing functionality
- Adds new alias detection without breaking existing features

## Benefits

1. **Comprehensive Command Lookup**: Users can identify all types of commands in one place
2. **Alias Visibility**: Makes it easy to see what aliases are defined
3. **Debugging Aid**: Helps users understand command resolution order
4. **Shell Consistency**: Provides behavior similar to modern shells like bash and zsh

## Usage Examples

```bash
# Check a builtin command
which cd
# Output: cd: shell builtin

# Check an alias
alias ll='ls -la'
which ll
# Output: ll: aliased to `ls -la'

# Check a binary
which ls
# Output: /bin/ls

# Check multiple commands
which cd ll ls nonexistent
# Output:
# cd: shell builtin
# ll: aliased to `ls -la'
# /bin/ls
# nonexistent: not found
```

## Future Enhancements

Potential future improvements could include:
- Function detection (if shell functions are added)
- Keyword detection (if, while, etc.)
- More detailed path information
- Color-coded output
- Verbose mode with additional details

## Conclusion

The enhanced `which` command provides a more complete and useful command lookup experience, making sudosh more user-friendly and consistent with modern shell expectations while maintaining full backward compatibility.
