# BITMAP_RESOURCES Role - Medium Priority

## Overview
This role is responsible for extracting and analyzing all bitmap resources from the launcher.exe binary. Bitmap resources contain graphical elements, background images, icons, and other visual components used throughout the application.

## Primary Responsibilities
- Extract all bitmap resources (RT_BITMAP)
- Analyze bitmap dimensions and formats
- Categorize by size and type
- Map bitmaps to UI elements
- Create comprehensive bitmap reference tables

## Tools to Use
- **res**: Microsoft resource extraction tool
  ```bash
  res launcher.exe bitmap > bitmaps.res
  ```
- **resdump**: Resource listing utility
  ```bash
  resdump launcher.exe | grep RT_BITMAP
  ```
- **convert**: Image format conversion (if needed)
- **identify**: Examine image properties

## Input File Path
- **launcher.exe**: `/home/pi/mxo/resources/launcher.exe`

## Output Deliverables
- `bitmaps/0020.bmp` - Bitmap files
- `bitmap_reference.md` - Bitmap specifications

## Workflow
1. List all RT_BITMAP resources using resdump
2. Extract bitmap resources using res tool
3. Parse and catalog bitmaps by ID
4. Analyze dimensions and formats
5. Create reference documentation

## Expected Bitmaps
- Background images
- UI component graphics
- Status indicators
- Decorative elements
- Application-specific graphics

## Priority
**3 - MEDIUM** (Bitmaps enhance visual presentation)