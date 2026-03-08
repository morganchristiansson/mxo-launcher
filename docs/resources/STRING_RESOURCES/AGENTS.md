# STRING_RESOURCES Role - Critical Priority

## Overview
This role is responsible for extracting and cataloging all string resources from the launcher.exe binary. String resources contain UI text, error messages, help text, and other textual content used by the application.

## Primary Responsibilities
- Extract all string resources from RT_STRING section
- Catalog strings by ID and content
- Identify UI-related strings
- Extract error messages
- Find help text
- Map strings to expected UI elements
- Create comprehensive string reference tables

## Tools to Use
- **res**: Microsoft resource extraction tool
  ```bash
  res launcher.exe string > strings.res
  ```
- **resdump**: Resource listing utility
  ```bash
  resdump launcher.exe | grep RT_STRING
  ```
- **readpe**: PE header analysis (for resource locations)

## Input File Path
- **launcher.exe**: `/home/pi/mxo/resources/launcher.exe`

## Output Deliverables
- `strings/0001.txt` - Individual string files
- `string_catalog.md` - Complete string reference
- `ui_strings.md` - UI-related string catalog
- `error_messages.md` - Error message catalog

## Workflow
1. List all RT_STRING resources using resdump
2. Extract string resources using res tool
3. Parse and catalog strings by ID
4. Categorize strings (UI, error, help)
5. Create reference documentation

## Expected Strings
- "Matrix Online"
- "Login"
- "Password"
- "Connect"
- "Disconnect"
- Server connection messages
- Error messages
- Help text

## Priority
**1 - CRITICAL** (String extraction is foundational for UI analysis)