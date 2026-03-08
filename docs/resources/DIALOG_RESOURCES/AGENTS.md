# DIALOG_RESOURCES Role - High Priority

## Overview
This role is responsible for extracting and analyzing all dialog resources from the launcher.exe binary. Dialog resources contain UI dialogs including login forms, configuration dialogs, error dialogs, progress indicators, and other interactive windows.

## Primary Responsibilities
- Extract all dialog resources (RT_DIALOG)
- Map dialog IDs to control types
- Document control relationships
- Analyze dialog layouts
- Create dialog reference tables
- Identify login, config, and error dialogs

## Tools to Use
- **res**: Microsoft resource extraction tool
  ```bash
  res launcher.exe dialog > dialogs.res
  ```
- **resdump**: Resource listing utility
  ```bash
  resdump launcher.exe | grep RT_DIALOG
  ```
- **dialog**: Analyze extracted dialog files
- **grep**: Parse control definitions

## Input File Path
- **launcher.exe**: `/home/pi/mxo/resources/launcher.exe`

## Output Deliverables
- `dialogs/0010.res` - Dialog files
- `dialog_reference.md` - Complete dialog documentation
- `login_dialog.md` - Login dialog analysis
- `config_dialog.md` - Config dialog analysis

## Workflow
1. List all RT_DIALOG resources using resdump
2. Extract dialog resources using res tool
3. Parse and catalog dialogs by ID
4. Map controls (buttons, edit fields, checkboxes)
5. Create reference documentation

## Expected Dialogs
- Login dialog: Username/password input
- Config dialog: Server settings
- Error dialogs: Connection errors
- Progress dialogs: Loading indicators
- Help dialogs: Usage instructions

## Priority
**2 - HIGH** (Dialogs are critical for application functionality)