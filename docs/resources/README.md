# Resource Table Analysis

## Purpose
Document all embedded resources including icons, dialogs, menus, strings, and bitmaps.

## Resource Types Expected

### 1. Icons (RT_GROUP_ICON)
- Launcher icon
- Window icons
- Small icons

### 2. Dialogs (RT_DIALOG)
- Login dialog
- Configuration dialog
- Error dialogs
- Progress dialogs

### 3. Menus (RT_MENU)
- Application menu
- Context menus
- Toolbars

### 4. Strings (RT_STRING)
- UI text
- Error messages
- Help text

### 5. Bitmaps (RT_BITMAP)
- Splash screen
- Background images
- Status icons

### 6. Version Info (RT_VERSION)
- File version
- Product version
- Company name
- Description

## Resource Directory Structure

```
Resource Table
├── Characteristics
├── Time Stamp
├── Version
├── Number of Types
├── Number of Names
├── RVA to First Directory
└── Size of Directory

Directory Entries:
├── Type (e.g., RT_GROUP_ICON)
├── RVA to Sub-directory
└── Size of Sub-directory
```

## Resource Extraction Commands

```bash
# View resources
resdump launcher.exe

# Extract all resources
res launcher.exe > resources.bin

# Extract specific resource type
res launcher.exe icon > icon.res
```

## Expected Resources for Matrix Online Launcher

### Icons
- **Main Icon**: Matrix logo or game icon
- **Window Icon**: Small version for taskbar
- **Cursor Icons**: Standard Windows cursors

### Dialogs
- **Login Dialog**: Username/password input
- **Config Dialog**: Server settings
- **Error Dialogs**: Connection errors

### Strings
- "Matrix Online"
- "Login"
- "Password"
- "Connect"
- "Disconnect"
- Error messages
- Help text

### Version Info
- Product: Matrix Online
- Company: Matrix Entertainment
- Version: ?
- FileDescription: Matrix Online Launcher

## Resource Analysis Template

```markdown
## Resources

### Icons (RT_GROUP_ICON)
| ID | RVA | Size | Description |
|----|-----|------|-------------|
| 1 | 0x00402000 | 0x00000100 | Main icon |

### Dialogs (RT_DIALOG)
| ID | RVA | Size | Controls |
|----|-----|------|----------|
| 101 | 0x00403000 | 0x00000200 | Login dialog |

### Version Info (RT_VERSION)
| Field | Value |
|-------|-------|
| FileVersion | |
| ProductVersion | |
| CompanyName | |
| Description | |
```

## Notes
- Resources are packed/compressed in some cases
- String resources may be encrypted
- Dialog controls are defined with IDs and types

## Status
- **Resource Types Found**: 0
- **Total Resources**: 0
- **Analysis Complete**: No