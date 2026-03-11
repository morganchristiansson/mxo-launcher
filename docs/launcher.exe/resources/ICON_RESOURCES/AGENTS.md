# ICON_RESOURCES Role - High Priority

## Overview
This role is responsible for extracting and analyzing all icon resources from the launcher.exe binary. Icon resources include the main application icon, window icons, cursor icons, and other graphical elements used for visual representation.

## Primary Responsibilities
- Extract all icon resources (RT_GROUP_ICON, RT_ICON)
- Document icon dimensions and characteristics
- Categorize by size (main, small, cursor)
- Analyze icon formats (ICO, CUR)
- Map icons to UI elements
- Create comprehensive icon reference tables

## Tools to Use
- **resdump**: Resource listing utility (Windows)
  ```bash
  resdump launcher.exe | grep -E "GROUP_ICON|ICON"
  ```
- **7-Zip**: PE file extraction tool (Linux/Alternative)
  ```bash
  7z l launcher.exe | grep -i icon
  7z x launcher.exe -y -oicons ".rsrc/ICON/*" ".rsrc/GROUP_ICON/*"
  ```
- **res**: Microsoft resource extraction tool (Windows, if available)
  ```bash
  res launcher.exe icon > icons.res
  res launcher.exe groupicon > groupicons.res
  ```
- **iconv**: Convert icon formats if needed
- **xwininfo**: Inspect icon sizes in GUI

## Input File Path
- **launcher.exe**: `/home/pi/mxo/launcher.exe` (or `~/mxo/launcher.exe`)

## Output Deliverables
- `icons/1.ico`, `icons/3.ico` - Main launcher icons (256x256)
- `icons/2.ico`, `icons/4.ico` - Window/taskbar icons (128x128)
- `icons/5-12.ico` - Cursor/small icons (48x48 to 8x8)
- `icons/128`, `icons/131` - Group icon entries
- `icons/icon_reference.md` - Icon specifications and documentation

## Workflow

### Method 1: Using 7-Zip (Linux/Alternative)
1. List all icon resources: `7z l launcher.exe | grep -i icon`
2. Extract icon resources: `7z x launcher.exe -y -oicons ".rsrc/ICON/*" ".rsrc/GROUP_ICON/*"`
3. Parse and catalog icons by ID from file listing
4. Categorize icons by size (main, window, cursor)
5. Create reference documentation

### Method 2: Using Windows Tools (res/resdump)
1. List all icon resources using resdump: `resdump launcher.exe | grep -E "GROUP_ICON|ICON"`
2. Extract icon resources using res tool: `res launcher.exe icon > icons.res`
3. Parse and catalog icons by ID from extracted files
4. Categorize icons (main, window, cursor)
5. Create reference documentation

## Expected Icons

### ICON Resources (12 files)
| ID | Size | Description |
|----|------|-------------|
| 1, 3 | 256x256 | Main launcher icons |
| 2, 4 | 128x128 | Window/taskbar icons |
| 5, 9 | 48x48 | Cursor/large small icons |
| 6, 10 | 32x32 | Small icons |
| 7, 11 | 16x16 | Very small icons |
| 8, 12 | 8x8 | Tiny icons |

### GROUP_ICON Resources (2 entries)
- 128, 131: Group icon entries

### Additional Resources Found
- BITMAP resources (25+ entries): Additional bitmap icons
- WAVE resources: Sound files
- DIALOG resources: UI dialogs

## Priority
**2 - HIGH** (Icons are essential for application identification)