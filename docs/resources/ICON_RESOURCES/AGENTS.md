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
- **res**: Microsoft resource extraction tool
  ```bash
  res launcher.exe icon > icons.res
  res launcher.exe groupicon > groupicons.res
  ```
- **resdump**: Resource listing utility
  ```bash
  resdump launcher.exe | grep -E "RT_GROUP_ICON|RT_ICON"
  ```
- **iconv**: Convert icon formats if needed
- **xwininfo**: Inspect icon sizes in GUI

## Input File Path
- **launcher.exe**: `/home/pi/mxo/resources/launcher.exe`

## Output Deliverables
- `icons/main.ico` - Main launcher icon
- `icons/window.ico` - Window icon
- `icons/cursor.ico` - Cursor icons
- `icon_reference.md` - Icon specifications

## Workflow
1. List all icon resources using resdump
2. Extract icon resources using res tool
3. Parse and catalog icons by ID
4. Categorize icons (main, window, cursor)
5. Create reference documentation

## Expected Icons
- Main icon: Matrix logo or game icon
- Window icon: Small version for taskbar
- Cursor icons: Standard Windows cursors
- Application-specific cursors

## Priority
**2 - HIGH** (Icons are essential for application identification)