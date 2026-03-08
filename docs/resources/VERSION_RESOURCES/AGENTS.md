# VERSION_RESOURCES Role - High Priority

## Overview
This role is responsible for extracting and analyzing all version information resources from the launcher.exe binary. Version resources contain product metadata including name, company, version numbers, description, copyright, and legal information.

## Primary Responsibilities
- Extract all version info (RT_VERSION)
- Parse product name, company, version fields
- Extract description and legal info
- Create comprehensive version reference tables

## Tools to Use
- **res**: Microsoft resource extraction tool
  ```bash
  res launcher.exe version > version.res
  ```
- **resdump**: Resource listing utility
  ```bash
  resdump launcher.exe | grep RT_VERSION
  ```
- **verifype**: Verify PE version info
- **grep**: Parse version fields

## Input File Path
- **launcher.exe**: `/home/pi/mxo/resources/launcher.exe`

## Output Deliverables
- `version_info/0040.res` - Version files
- `version_info.md` - Version details

## Workflow
1. List all RT_VERSION resources using resdump
2. Extract version resources using res tool
3. Parse and catalog version fields
4. Extract product metadata
5. Create reference documentation

## Expected Version Info
- Product name: Application identifier
- Company: Developer name
- Version: Major.Minor.Build.Revision
- Description: Product description
- Copyright: Legal information
- Trademarks: Brand information

## Priority
**2 - HIGH** (Version info is essential for application identification)