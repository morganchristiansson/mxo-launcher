# MENU_RESOURCES Role - Medium Priority

## Overview
This role is responsible for extracting and analyzing all menu resources from the launcher.exe binary. Menu resources contain application menus including file operations, configuration options, help items, and other navigational elements.

## Primary Responsibilities
- Extract all menu resources (RT_MENU)
- Document menu structure and items
- Map menu IDs to actions
- Analyze menu layouts
- Create comprehensive menu reference tables

## Tools to Use
- **res**: Microsoft resource extraction tool
  ```bash
  res launcher.exe menu > menus.res
  ```
- **resdump**: Resource listing utility
  ```bash
  resdump launcher.exe | grep RT_MENU
  ```
- **grep**: Parse menu definitions
- **awk**: Extract menu items and IDs

## Input File Path
- **launcher.exe**: `/home/pi/mxo/resources/launcher.exe`

## Output Deliverables
- `menus/0030.res` - Menu files
- `menu_reference.md` - Menu specifications

## Workflow
1. List all RT_MENU resources using resdump
2. Extract menu resources using res tool
3. Parse and catalog menus by ID
4. Map menu items to actions
5. Create reference documentation

## Expected Menus
- File menu: Open, save, exit
- Config menu: Server settings
- Help menu: About, documentation
- Application-specific menus

## Priority
**3 - MEDIUM** (Menus enhance user navigation)