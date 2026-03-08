# Resource Table Analysis - Multi-Agent Setup

## Overview
This document defines the multi-agent architecture for analyzing embedded resources in launcher.exe, including icons, dialogs, menus, strings, and bitmaps.

## Agent Roles and Responsibilities

### 1. RESOURCE_EXTRACTOR
**Priority**: 2 (HIGH)
**Status**: Pending

**Primary Responsibility**: Extract all resource types from launcher.exe

**Tasks**:
- [ ] List all resource types using resdump
- [ ] Extract resource directory structure
- [ ] Extract icon resources (RT_GROUP_ICON)
- [ ] Extract dialog resources (RT_DIALOG)
- [ ] Extract menu resources (RT_MENU)
- [ ] Extract string resources (RT_STRING)
- [ ] Extract bitmap resources (RT_BITMAP)
- [ ] Extract version info (RT_VERSION)

**Deliverables**:
- `resources.bin` - All extracted resources
- `icons/` - Icon files
- `dialogs/` - Dialog files
- `strings/` - String files
- `bitmaps/` - Bitmap files
- `version_info/` - Version info files

**Commands**:
```bash
# View all resources
resdump launcher.exe

# Extract all resources
res launcher.exe > resources.bin

# Extract specific types
res launcher.exe icon > icons.res
res launcher.exe dialog > dialogs.res
res launcher.exe string > strings.res
res launcher.exe bitmap > bitmaps.res
res launcher.exe version > version.res
```

---

### 2. RESOURCE_ANALYST
**Priority**: 2 (HIGH)
**Status**: Pending

**Primary Responsibility**: Analyze extracted resources and document structure

**Tasks**:
- [ ] Document resource directory structure
- [ ] Analyze icon characteristics and dimensions
- [ ] Map dialog ID to control relationships
- [ ] Extract and categorize string content
- [ ] Analyze bitmap dimensions and formats
- [ ] Parse version info fields
- [ ] Create resource reference tables

**Deliverables**:
- `resource_table.md` - Complete resource documentation
- `icon_reference.md` - Icon specifications
- `dialog_reference.md` - Dialog control mappings
- `string_catalog.md` - String content catalog
- `bitmap_reference.md` - Bitmap specifications
- `version_info.md` - Version details

**Expected Resources**:
- **Icons**: Main icon, window icons, cursor icons
- **Dialogs**: Login dialog, config dialog, error dialogs
- **Strings**: "Matrix Online", "Login", "Password", "Connect", "Disconnect"
- **Version**: Product name, company, version, description

---

### 3. STRING_EXTRACTOR
**Priority**: 1 (CRITICAL)
**Status**: Pending

**Primary Responsibility**: Extract and catalog all string resources

**Tasks**:
- [ ] Extract all string resources from RT_STRING section
- [ ] Catalog strings by ID and content
- [ ] Identify UI-related strings
- [ ] Extract error messages
- [ ] Find help text
- [ ] Map strings to expected UI elements

**Deliverables**:
- `strings/0001.txt` - Individual string files
- `string_catalog.md` - Complete string reference
- `ui_strings.md` - UI-related string catalog
- `error_messages.md` - Error message catalog

**Expected Strings**:
- "Matrix Online"
- "Login"
- "Password"
- "Connect"
- "Disconnect"
- Server connection messages
- Error messages
- Help text

---

### 4. ICON_ANALYST
**Priority**: 3 (MEDIUM)
**Status**: Pending

**Primary Responsibility**: Analyze and document all icon resources

**Tasks**:
- [ ] Extract all icon resources
- [ ] Document icon dimensions and characteristics
- [ ] Categorize by size (main, small, cursor)
- [ ] Analyze icon formats (ICO, CUR)
- [ ] Map icons to UI elements

**Deliverables**:
- `icons/main.ico` - Main launcher icon
- `icons/window.ico` - Window icon
- `icons/cursor.ico` - Cursor icons
- `icon_reference.md` - Icon specifications

**Expected Icons**:
- Main icon: Matrix logo or game icon
- Window icon: Small version for taskbar
- Cursor icons: Standard Windows cursors

---

### 5. DIALOG_ANALYST
**Priority**: 3 (MEDIUM)
**Status**: Pending

**Primary Responsibility**: Analyze and document dialog resources

**Tasks**:
- [ ] Extract all dialog resources
- [ ] Map dialog IDs to control types
- [ ] Document control relationships
- [ ] Analyze dialog layouts
- [ ] Create dialog reference tables

**Deliverables**:
- `dialogs/0010.res` - Dialog files
- `dialog_reference.md` - Complete dialog documentation
- `login_dialog.md` - Login dialog analysis
- `config_dialog.md` - Config dialog analysis

**Expected Dialogs**:
- Login dialog: Username/password input
- Config dialog: Server settings
- Error dialogs: Connection errors
- Progress dialogs: Loading indicators

---

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

## Resource Extraction Workflow

```
1. RESOURCE_EXTRACTOR
   ├── Execute resdump launcher.exe
   ├── Extract resources.bin
   └── Organize by type

2. STRING_EXTRACTOR (Parallel)
   ├── Extract RT_STRING resources
   ├── Catalog strings
   └── Create string reference

3. ICON_ANALYST (Parallel)
   ├── Extract RT_GROUP_ICON
   ├── Analyze dimensions
   └── Document specifications

4. DIALOG_ANALYST (Parallel)
   ├── Extract RT_DIALOG
   ├── Map controls
   └── Document layouts

5. RESOURCE_ANALYST
   ├── Consolidate all findings
   ├── Create resource_table.md
   └── Final documentation
```

## Expected Deliverables

### Primary Documentation
- `resource_table.md` - Complete resource reference
- `string_catalog.md` - String content catalog
- `icon_reference.md` - Icon specifications
- `dialog_reference.md` - Dialog control mappings

### Extracted Files
- `resources.bin` - All extracted resources
- `icons/` - Icon files
- `dialogs/` - Dialog files
- `strings/` - String files
- `bitmaps/` - Bitmap files
- `version_info/` - Version details

## Status Tracking

| Agent | Priority | Status | Progress | Next Task |
|-------|----------|--------|----------|-----------|
| RESOURCE_EXTRACTOR | 2 | Pending | 0% | List resources |
| STRING_EXTRACTOR | 1 | Pending | 0% | Extract strings |
| ICON_ANALYST | 3 | Pending | 0% | Extract icons |
| DIALOG_ANALYST | 3 | Pending | 0% | Extract dialogs |
| RESOURCE_ANALYST | 2 | Pending | 0% | Wait for data |

## Notes
- Resources may be packed/compressed
- String resources may be encrypted
- Dialog controls defined with IDs and types
- Version info contains product metadata

## Tools Available
- **res**: Microsoft resource extraction tool
- **resdump**: Resource listing utility
- **readpe**: PE header analysis (for resource locations)