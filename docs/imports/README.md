# Import Table Analysis

## Purpose
Document all DLL imports and API calls used by launcher.exe to understand its dependencies and functionality.

## Common Windows APIs Used by Launchers

### User Interface
- `CreateWindowEx` - Create windows
- `ShowWindow` - Display windows
- `SendMessage` - Window messaging
- `PostMessage` - Async window messaging
- `GetDlgItem` - Get dialog controls
- `SendDlgItemMessage` - Send messages to dialog controls
- `MessageBox` - Display message boxes
- `DialogBox` - Create dialog boxes
- `GetWindowText` - Get window text
- `SetWindowText` - Set window text

### Graphics
- `GetDC` / `ReleaseDC` - Device context
- `DrawText` - Text rendering
- `LoadIcon` / `LoadCursor` - Load icons
- `CreateBitmap` - Create bitmaps
- `BitBlt` - Bit block transfer
- `GDIPlusStartup` - GDI+ initialization

### System
- `GetSystemDirectory` - Get system path
- `GetCurrentProcess` - Get process handle
- `GetModuleHandle` - Get module handle
- `FindFirstFile` - File enumeration
- `CreateFile` - File operations
- `ReadFile` / `WriteFile` - File I/O
- `CloseHandle` - Close handles

### Networking (if applicable)
- `WSAStartup` - Winsock initialization
- `socket` - Create socket
- `connect` - Connect to server
- `send` / `recv` - Network I/O

### Memory
- `VirtualAlloc` - Allocate memory
- `VirtualFree` - Free memory
- `HeapAlloc` - Heap allocation
- `FreeLibrary` - Free DLL

## Import Analysis Template

```markdown
## Import Table

| DLL Name | Function | Ordinal | RVA | Characteristics |
|----------|----------|---------|-----|-----------------|
| kernel32.dll | CreateFile | 34 | 0x00401234 | IMAGE_ORDINAL_FLAG |
```

## Known Imports for Matrix Online

### Likely Imports (to verify)
- `user32.dll` - Windows UI functions
- `gdi32.dll` - Graphics functions
- `kernel32.dll` - System functions
- `ws2_32.dll` - Winsock (networking)
- `comctl32.dll` - Common controls
- `wininet.dll` - Internet functions

## Analysis Method

1. Parse PE Optional Header Data Directory for Import Table
2. Extract Import Address Table (IAT)
3. Extract Import Name Table
4. Identify ordinal vs name imports
5. Categorize by DLL and function type

## Notes
- Some functions may use ordinals instead of names
- IAT entries are patched at runtime
- Delay-loaded imports may not appear in static analysis

## Status
- **Imports Identified**: 0
- **DLLs Found**: 0
- **Analysis Complete**: No