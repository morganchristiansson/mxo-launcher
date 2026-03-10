# String Table Analysis

## Purpose
Document all embedded strings used by the launcher.

## String Categories

### 1. UI Strings
- Window titles
- Button labels
- Menu items
- Dialog text

### 2. Error Messages
- Connection errors
- Configuration errors
- System errors

### 3. Game Messages
- Server status
- User prompts
- Notifications

### 4. Resource Strings
- Version info strings
- Company/product names
- Description strings

## String Extraction

```bash
# Extract all strings
strings launcher.exe > string_table.txt

# Extract printable strings (min 4 chars)
strings -e l -n 4 launcher.exe > strings_ascii.txt

# Extract hex strings
strings -e h -n 4 launcher.exe > strings_hex.txt
```

## String Analysis Template

```markdown
## UI Strings

| String | Category | Location | Usage |
|--------|----------|----------|-------|
| "Matrix Online" | Window Title | 0x00402000 | Main window title |
| "Login" | Button | 0x00403000 | Login button label |
| "Password" | Label | 0x00404000 | Password field label |

## Error Messages

| String | Category | Location | Usage |
|--------|----------|----------|-------|
| "Connection failed" | Error | 0x00405000 | Connection error message |
| "Invalid credentials" | Error | 0x00406000 | Login error message |

## Version Info Strings

| String | Category | Location | Usage |
|--------|----------|----------|-------|
| "Matrix Entertainment" | Company | 0x00407000 | Company name |
| "1.0.0.0" | Version | 0x00408000 | File version |
```

## Known Matrix Online Strings (to verify)

### Window Titles
- "Matrix Online"
- "Matrix Online Launcher"
- "Login to Matrix Online"

### UI Elements
- "Username"
- "Password"
- "Connect"
- "Disconnect"
- "Options"
- "Help"
- "Exit"

### Messages
- "Connecting to server..."
- "Connected"
- "Disconnected"
- "Server not found"
- "Invalid password"
- "Connection timeout"

## Notes
- Some strings may be obfuscated
- Resource strings are in resource table
- Embedded strings are in code/data sections
- Unicode strings may be double-width

## Status
- **Total Strings**: 0
- **UI Strings**: 0
- **Error Messages**: 0
- **Analysis Complete**: No