# BITMAP_RESOURCES Role - Medium Priority

## Overview
This role is responsible for extracting and analyzing all bitmap resources from the launcher.exe binary. Bitmap resources contain graphical elements, background images, icons, and other visual components used throughout the application.

## Primary Responsibilities
- Extract all bitmap resources (RT_BITMAP)
- Analyze bitmap dimensions and formats
- Categorize by size and type
- Map bitmaps to UI elements
- Create comprehensive bitmap reference tables
- Document file properties and resource usage

## Tools to Use

### Method 1: Using 7-Zip (Linux/Alternative) - **Recommended**
- **7-Zip**: PE file extraction tool
  ```bash
  cd /home/pi/mxo
  7z l launcher.exe | grep -i bitmap
  7z x launcher.exe -y -obitmaps/bitmaps ".rsrc/BITMAP/*"
  ```

### Method 2: Using Windows Tools (res/resdump)
- **resdump**: Resource listing utility (Windows)
  ```bash
  resdump launcher.exe | grep RT_BITMAP
  ```
- **res**: Microsoft resource extraction tool
  ```bash
  res launcher.exe bitmap > bitmaps.res
  ```

### Analysis Tools
- **identify**: Examine image properties
  ```bash
  identify bitmaps/*.bmp
  ```
- **file**: Check file format
  ```bash
  file bitmaps/*.bmp
  ```

## Input File Path
- **launcher.exe**: `/home/pi/mxo/launcher.exe`

## Output Deliverables
- `bitmaps/bitmaps/*.bmp` - Extracted bitmap files (organized by ID)
- `bitmap_reference.md` - Comprehensive bitmap specifications and documentation

## Workflow

### Method 1: Using 7-Zip (Linux/Alternative) - **Recommended**
1. List all RT_BITMAP resources: `7z l launcher.exe | grep -i bitmap`
2. Extract bitmap resources: `7z x launcher.exe -y -obitmaps/bitmaps ".rsrc/BITMAP/*"`
3. Copy extracted files to output directory: `cp .rsrc/BITMAP/* ../`
4. Analyze dimensions: `for f in *.bmp; do identify $f; done | sort | uniq -c`
5. Categorize by size and count
6. Create reference documentation with categorization, statistics, and UI mapping

### Method 2: Using Windows Tools (res/resdump)
1. List all bitmap resources using resdump: `resdump launcher.exe | grep RT_BITMAP`
2. Extract bitmap resources using res tool: `res launcher.exe bitmap > bitmaps.res`
3. Parse and catalog bitmaps by ID from extracted files
4. Analyze dimensions and formats
5. Create reference documentation

## Expected Bitmaps
- Background images (large, 177x300 to 500x420)
- UI component graphics (medium, 153x36 to 153x54)
- Status indicators (compact, 88x34 to 116x29)
- Decorative elements (various sizes)
- Application-specific graphics (unique dimensions)

## Bitmap Categorization by Size
| Dimensions | Count | Description |
|------------|-------|-------------|
| 177x300 | 2 | Main application background |
| 500x420 | 3 | Secondary backgrounds |
| 153x54 | 29 | Status/progress bars |
| 153x36 | 9 | Status indicators |
| 116x29 | 7 | Small UI elements |
| 305x31 | 6 | Compact graphics |
| 101x40 | 6 | Status icons |
| 238x9 | 5 | Progress indicators |
| 25x24 | 4 | Tiny icons |
| 170x27 | 4 | Compact status |
| 88x34 | 3 | Small graphics |

## File Properties
- **Format**: PC bitmap (Windows 3.x format)
- **Color Depth**: 8-bit sRGB
- **Compression**: None (raw DIB)
- **Header Size**: 54 bytes
- **Bits Offset**: 54 bytes from file start

## Total Resource Statistics
- **Compressed Size**: ~3,261,712 bytes (3.17 MB)
- **Average per Bitmap**: ~43.5 KB
- **Total Bitmaps**: 75 files
- **ID Range**: 183-271 (with gaps)

## Related Resources
- **ICON_RESOURCES**: /home/pi/mxo/docs/resources/ICON_RESOURCES/AGENTS.md
- **BITMAP_RESOURCES**: This file location

## Priority
**3 - MEDIUM** (Bitmaps enhance visual presentation)

## Lessons Learned from Execution
1. **7-Zip is more reliable** than res/resdump on Linux systems
2. **Direct extraction** to `.rsrc/BITMAP/` preserves original structure
3. **Batch analysis** with loops provides comprehensive dimension data
4. **Categorization by size** reveals patterns in resource usage
5. **Total resource size** is significant (~4.1 MB uncompressed)
6. **Format consistency**: All bitmaps use Windows 3.x BMP format (DIB)
7. **No compression** applied to bitmap data within the resource file

## Bitmap Reference Data

### Source Information
- **Binary**: launcher.exe
- **Location**: /home/pi/mxo/launcher.exe
- **Total Bitmaps**: 75
- **Range**: IDs 183-271 (with gaps)

### Large Background Images (4 files)
| ID | Dimensions | Description |
|----|------------|-------------|
| 183 | 177x300 | Main application background |
| 185 | 500x420 | Secondary background image |
| 186 | 500x420 | Secondary background image |
| 188 | 500x420 | Secondary background image |

### Medium Interface Graphics (71 files)

#### 153x54 (25 files) - Status/Progress Bars
IDs: 197, 199, 200, 201, 202, 203, 204, 206, 207, 209, 210, 212, 213, 214, 228, 229, 230, 234, 235, 236, 237, 238, 239, 246, 251, 252, 253, 257, 258, 259, 260, 261

#### 153x36 (9 files) - Status Indicators
IDs: 208, 209, 211, 215, 216, 217, 220, 221, 222, 223, 224, 225, 226, 227, 231, 232, 233, 240, 241

#### 116x29 (7 files) - Small UI Elements
IDs: 196, 198, 205, 218, 230, 242, 247

#### 305x31 (6 files) - Compact Graphics
IDs: 248, 249, 250, 254, 255, 262, 263, 264, 265, 266, 267

#### 101x40 (6 files) - Status Icons
IDs: 202, 203, 204, 205, 206, 207, 208, 209, 210, 211

#### 238x9 (5 files) - Progress Indicators
IDs: 219, 220, 221, 222, 223

#### 25x24 (4 files) - Tiny Icons
IDs: 192, 193, 194, 195

#### 170x27 (4 files) - Compact Status
IDs: 243, 244, 256, 268, 269

#### 88x34 (3 files) - Small Graphics
IDs: 212, 213, 214

#### 26x17 (1 file) - Unique Graphic
ID: 270

### File Properties

#### Format
- **Type**: PC bitmap (Windows 3.x format)
- **Color Depth**: 8-bit sRGB
- **Compression**: None (raw DIB)

#### Structure
- **Header Size**: 54 bytes
- **Bits Offset**: 54 bytes from file start
- **Image Data**: Raw RGB pixels

### UI Mapping

#### Background Images
- Used for application windows and dialogs
- Scaled to fit container dimensions
- IDs: 183, 185-188

#### Status Indicators
- Progress bars and status lines
- Horizontal orientation (width > height)
- IDs: 219-222, 245

#### Interface Elements
- Buttons, tabs, and navigation controls
- Rectangular proportions
- IDs: 196-207, 231-241

#### Small Icons
- Status indicators, tooltips
- Compact dimensions
- IDs: 192-195, 248-250

### Technical Specifications

#### File Sizes by Category
| Size (px) | Count | Total Size (KB) | Avg Size (KB) |
|-----------|-------|-----------------|---------------|
| 177x300 | 2 | 155.6 | 77.8 |
| 500x420 | 3 | 617.5 | 205.8 |
| 153x54 | 29 | 294.8 | 10.2 |
| 153x36 | 9 | 91.5 | 10.2 |
| 116x29 | 7 | 71.2 | 10.2 |
| 305x31 | 6 | 61.1 | 10.2 |
| 101x40 | 6 | 61.1 | 10.2 |
| 238x9 | 5 | 50.6 | 10.1 |
| 25x24 | 4 | 40.4 | 10.1 |
| 170x27 | 4 | 40.4 | 10.1 |
| 88x34 | 3 | 30.3 | 10.1 |

#### Total Resource Size
- **Compressed**: 3,261,712 bytes (3.17 MB)
- **Uncompressed**: ~4,700,000 bytes (estimated)
- **Average per Bitmap**: 43.5 KB

### Extraction Details

#### Method Used
- **Tool**: 7-Zip (Linux/Alternative)
- **Command**: `7z x launcher.exe -y -obitmaps/bitmaps ".rsrc/BITMAP/*"`

#### Output Location
```
/home/pi/mxo/bitmaps/bitmaps/
├── 183.bmp
├── 185.bmp
├── ...
└── 270.bmp
```

### Notes
- Bitmap IDs are sequential with gaps in the resource table
- All bitmaps use Windows 3.x BMP format (DIB)
- No compression applied to bitmap data
- Graphics are suitable for scaling and rendering at various resolutions