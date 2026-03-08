# Icon Reference - launcher.exe

## Extracted Icons

### ICON Resources (from .rsrc/ICON/)

| ID | File | Size (bytes) | Likely Dimensions | Description |
|----|------|--------------|-------------------|-------------|
| 1 | 1.ico | 67646 | 256x256 | Main launcher icon |
| 2 | 2.ico | 16958 | 128x128 | Window/taskbar icon |
| 3 | 3.ico | 67646 | 256x256 | Main launcher icon variant |
| 4 | 4.ico | 16958 | 128x128 | Window/taskbar icon variant |
| 5 | 5.ico | 9662 | 48x48 | Cursor/large small icon |
| 6 | 6.ico | 4286 | 32x32 | Small icon |
| 7 | 7.ico | 2462 | 16x16 | Very small icon |
| 8 | 8.ico | 1150 | 8x8 | Tiny icon |
| 9 | 9.ico | 9662 | 48x48 | Cursor/large small icon |
| 10 | 10.ico | 4286 | 32x32 | Small icon |
| 11 | 11.ico | 2462 | 16x16 | Very small icon |
| 12 | 12.ico | 1150 | 8x8 | Tiny icon |

### GROUP_ICON Resources

| ID | File | Size (bytes) | Description |
|----|------|--------------|-------------|
| 128 | 128 | 90 | Group icon entry 1 |
| 131 | 131 | 90 | Group icon entry 2 |

## Icon Categories

### Main Icons (256x256)
- 1.ico - Primary launcher icon
- 3.ico - Secondary launcher icon

### Window Icons (128x128)
- 2.ico - Taskbar/window icon
- 4.ico - Alternate window icon

### Cursor/Small Icons
- 5.ico, 9.ico (48x48) - Larger cursor icons
- 6.ico, 10.ico (32x32) - Small icons
- 7.ico, 11.ico (16x16) - Very small icons
- 8.ico, 12.ico (8x8) - Tiny icons

## File Locations

All extracted icon files are in: `~/mxo/icons/`

- Main icons: `icons/1.ico`, `icons/3.ico`
- Window icons: `icons/2.ico`, `icons/4.ico`
- Cursor/small icons: `icons/5-12.ico`
- Group icons: `icons/128`, `icons/131`

## Extraction Method

Icons were extracted using 7-Zip from the PE file resources:
```bash
7z x launcher.exe -y -oicons ".rsrc/ICON/*" ".rsrc/GROUP_ICON/*"
```

## Application Info

- **Product**: The Matrix Online
- **Company**: Monolith Productions, Inc.
- **Version**: 76.0.0.4
- **File Date**: September 13, 2008
