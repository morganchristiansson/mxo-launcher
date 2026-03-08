# OnInput

## Overview

**Category**: UI  
**Direction**: Launcher → Client  
**Purpose**: User input event notification callback

---

## Function Signature

```c
void OnInput(InputEvent* inputEvent, uint32_t inputId, uint32_t flags);
```

### Parameters

| Type | Name | Purpose |
|------|------|---------|
| `InputEvent*` | inputEvent | Input event details and state |
| `uint32_t` | inputId | Unique input identifier |
| `uint32_t` | flags | Input flags and modifiers |

### Return Value

| Type | Value | Meaning |
|------|-------|---------|
| `void` | N/A | No return value |

---

## Input Event Types

The input system handles various event types:

| Event Type | Description |
|------------|-------------|
| `INPUT_KEYDOWN` | Key pressed (down event) |
| `INPUT_KEYUP` | Key released (up event) |
| `INPUT_MOUSEMOVE` | Mouse movement |
| `INPUT_MOUSEBUTTONDOWN` | Mouse button pressed |
| `INPUT_MOUSEBUTTONUP` | Mouse button released |
| `INPUT_MOUSEDOWN` | Mouse scroll down |
| `INPUT_MOUSEUP` | Mouse scroll up |

---

## Input Data Structures

### InputEvent Structure

```c
struct InputEvent {
    uint32_t eventType;        // Event type (INPUT_KEYDOWN, etc.)
    uint32_t inputId;          // Input identifier (key code, mouse button)
    int32_t deltaX;            // X axis delta (mouse/scroll)
    int32_t deltaY;            // Y axis delta (mouse/scroll)
    int32_t amount;            // Amount/value (repeat count)
    uint32_t timestamp;        // Event timestamp
    uint32_t windowId;         // Target window identifier
    uint32_t reserved;         // Reserved for future use
};
```

### InputObject Structure

```c
struct InputObject {
    void* pVTable;              // 0x00: Virtual function table
    uint32_t inputState;        // 0x04: Current input state
    
    // Keyboard state
    uint32_t keyState[256];     // 0x08: Key states (array of 256 keys)
    
    // Mouse state
    int32_t mouseX;             // 0x10: Mouse X position
    int32_t mouseY;             // 0x14: Mouse Y position
    int32_t mouseWheel;         // 0x18: Mouse wheel delta
    
    // Window management
    uint32_t activeWindowId;    // 0x1C: Active window
    uint32_t focusedWindowId;   // 0x20: Focused window
    
    // Input modifiers
    uint32_t shiftPressed;      // 0x24: Shift key state
    uint32_t ctrlPressed;       // 0x28: Ctrl key state
    uint32_t altPressed;        // 0x2C: Alt key state
    uint32_t winPressed;        // 0x30: Windows key state
    
    // Callback registration
    void* inputCallback;        // 0x34: Registered input callback
    void* callbackUserData;     // 0x38: User data for callback
};
```

---

## Usage

### Registration

Register input callback during initialization:

```c
// Register input callback
CallbackRegistration reg;
reg.eventType = EVENT_INPUT;
reg.callbackFunc = OnInput;
reg.userData = NULL;
reg.priority = 50;
reg.flags = 0;

APIObject* obj = g_MasterDatabase->pPrimaryObject;
int callbackId = obj->RegisterCallback2(&reg);
```

### Assembly Pattern

```assembly
; Launcher invokes OnInput callback
mov eax, [input_callback]     ; Load callback pointer
test eax, eax                 ; Check if NULL
je skip_callback              ; Skip if no callback

; Prepare parameters
push flags                    ; Input flags
push inputId                  ; Input identifier
push inputEvent              ; Input event structure
call eax                      ; Invoke callback
add esp, 12                   ; Cleanup stack

skip_callback:
```

---

## Implementation

### Launcher Side

```c
// Trigger input event
void OnInputReceived(InputEvent* input) {
    // Find registered callback
    CallbackEntry* entry = FindCallbackByType(EVENT_INPUT);
    if (entry && entry->callback) {
        OnInputCallback callback = (OnInputCallback)entry->callback;
        uint32_t inputId = input->inputId;
        uint32_t flags = 0;
        
        callback(input, inputId, flags);
    }
}

// Process keyboard input
void HandleKeyDown(uint32_t key, uint32_t windowId) {
    InputEvent event;
    event.eventType = INPUT_KEYDOWN;
    event.inputId = key;
    event.deltaX = 0;
    event.deltaY = 0;
    event.amount = 1;
    event.timestamp = GetTickCount();
    event.windowId = windowId;
    
    // Mark key as pressed
    SetKeyState(key, true);
    
    // Trigger callback
    OnInputReceived(&event);
}

void HandleKeyUp(uint32_t key, uint32_t windowId) {
    InputEvent event;
    event.eventType = INPUT_KEYUP;
    event.inputId = key;
    event.deltaX = 0;
    event.deltaY = 0;
    event.amount = 1;
    event.timestamp = GetTickCount();
    event.windowId = windowId;
    
    // Mark key as released
    SetKeyState(key, false);
    
    // Trigger callback
    OnInputReceived(&event);
}

// Process mouse input
void HandleMouseMove(int32_t x, int32_t y, uint32_t windowId) {
    InputEvent event;
    event.eventType = INPUT_MOUSEMOVE;
    event.inputId = 0;  // No specific input ID for movement
    event.deltaX = x;
    event.deltaY = y;
    event.amount = 0;
    event.timestamp = GetTickCount();
    event.windowId = windowId;
    
    // Trigger callback
    OnInputReceived(&event);
}

void HandleMouseDown(uint32_t button, uint32_t windowId) {
    InputEvent event;
    event.eventType = INPUT_MOUSEBUTTONDOWN;
    event.inputId = button;
    event.deltaX = 0;
    event.deltaY = 0;
    event.amount = 1;
    event.timestamp = GetTickCount();
    event.windowId = windowId;
    
    // Trigger callback
    OnInputReceived(&event);
}

void HandleMouseUp(uint32_t button, uint32_t windowId) {
    InputEvent event;
    event.eventType = INPUT_MOUSEBUTTONUP;
    event.inputId = button;
    event.deltaX = 0;
    event.deltaY = 0;
    event.amount = 1;
    event.timestamp = GetTickCount();
    event.windowId = windowId;
    
    // Trigger callback
    OnInputReceived(&event);
}

void HandleMouseWheel(int32_t delta, uint32_t windowId) {
    InputEvent event;
    event.eventType = INPUT_MOUSEDOWN;
    event.inputId = 0;
    event.deltaX = 0;
    event.deltaY = delta;
    event.amount = 1;
    event.timestamp = GetTickCount();
    event.windowId = windowId;
    
    // Trigger callback
    OnInputReceived(&event);
}
```

### Client Side

```c
// Input callback implementation
void MyOnInputCallback(InputEvent* inputEvent, uint32_t inputId, uint32_t flags) {
    switch (inputEvent->eventType) {
        case INPUT_KEYDOWN:
            HandleKeyDown(inputEvent->inputId, inputEvent->windowId);
            break;
            
        case INPUT_KEYUP:
            HandleKeyUp(inputEvent->inputId, inputEvent->windowId);
            break;
            
        case INPUT_MOUSEMOVE:
            SetCursorPosition(inputEvent->deltaX, inputEvent->deltaY);
            break;
            
        case INPUT_MOUSEBUTTONDOWN:
            HandleMouseDown(inputEvent->inputId, inputEvent->windowId);
            break;
            
        case INPUT_MOUSEBUTTONUP:
            HandleMouseUp(inputEvent->inputId, inputEvent->windowId);
            break;
            
        case INPUT_MOUSEDOWN:
            HandleMouseWheel(inputEvent->deltaY, inputEvent->windowId);
            break;
            
        case INPUT_MOUSEUP:
            HandleMouseScrollUp(inputEvent->deltaY, inputEvent->windowId);
            break;
    }
}

// Key handler example
void HandleKeyDown(uint32_t key) {
    switch (key) {
        case VK_ESCAPE:
            RequestCloseCurrentWindow();
            break;
            
        case 'W':
            MoveCharacter(West);
            break;
            
        case 'A':
            MoveCharacter(North);
            break;
            
        case 'S':
            MoveCharacter(South);
            break;
            
        case 'D':
            MoveCharacter(East);
            break;
    }
}

// Mouse movement handler
void SetCursorPosition(int32_t x, int32_t y) {
    // Update cursor position in window
    UpdateWindowCursor(g_ActiveWindow, x, y);
}

// Mouse button handler
void HandleMouseDown(uint32_t button) {
    switch (button) {
        case 0:  // Left click
            PerformLeftClick();
            break;
            
        case 1:  // Right click
            PerformRightClick();
            break;
            
        case 2:  // Middle click
            PerformMiddleClick();
            break;
    }
}

// Register input callback
void RegisterInputCallback() {
    CallbackRegistration reg;
    reg.eventType = EVENT_INPUT;
    reg.callbackFunc = MyOnInputCallback;
    reg.userData = NULL;
    reg.priority = 50;
    reg.flags = 0;
    
    APIObject* obj = g_MasterDatabase->pPrimaryObject;
    VTable* vtable = obj->pVTable;
    
    int (*regFunc)(APIObject*, CallbackRegistration*);
    regFunc = (int (*)(APIObject*, CallbackRegistration*))vtable->functions[24];
    
    int callbackId = regFunc(obj, &reg);
    printf("Registered input callback, ID=%d\n", callbackId);
}

// Unregister input callback
void UnregisterInputCallback() {
    CallbackRegistration reg;
    reg.eventType = EVENT_INPUT;
    reg.callbackFunc = NULL;
    
    APIObject* obj = g_MasterDatabase->pPrimaryObject;
    VTable* vtable = obj->pVTable;
    
    int (*unregFunc)(APIObject*, uint32_t);
    unregFunc = (int (*)(APIObject*, uint32_t))vtable->functions[5];
    
    int result = unregFunc(obj, 0);  // Use callback ID from registration
    printf("Unregistered input callback, result=%d\n", result);
}
```

---

## Input Key Codes

| Code | Description | Windows Key |
|------|-------------|-------------|
| VK_ESCAPE (0x1B) | Escape | ESC |
| VK_TAB (0x09) | Tab | TAB |
| VK_RETURN (0x0D) | Enter | ENTER |
| VK_BACK (0x08) | Backspace | BACKSPACE |
| VK_SPACE (0x20) | Space | SPACE |
| VK_UP (0x26) | Up Arrow | UP ARROW |
| VK_DOWN (0x28) | Down Arrow | DOWN ARROW |
| VK_LEFT (0x25) | Left Arrow | LEFT ARROW |
| VK_RIGHT (0x27) | Right Arrow | RIGHT ARROW |
| VK_DELETE (0x2E) | Delete | DELETE |
| VK_INSERT (0x2D) | Insert | INSERT |
| VK_HOME (0x21) | Home | HOME |
| VK_END (0x23) | End | END |
| VK_PRIOR (0x22) | Page Up | PAGE UP |
| VK_NEXT (0x24) | Page Down | PAGE DOWN |

---

## Input Modifiers

| Flag | Description |
|------|-------------|
| 1 | Shift key pressed |
| 2 | Ctrl key pressed |
| 4 | Alt key pressed |
| 8 | Windows key pressed |
| 16 | Caps lock active |
| 32 | Num lock active |
| 64 | Scroll lock active |

---

## Input State Management

### Key State Array

```c
// Track key states for repeat detection and debouncing
uint32_t keyState[256] = {0};  // All keys initially released

void SetKeyState(uint32_t key, bool pressed) {
    if (key < 256) {
        keyState[key] = pressed ? 1 : 0;
    }
}

bool IsKeyPressed(uint32_t key) {
    return key < 256 && keyState[key] == 1;
}

bool WasKeyJustPressed(uint32_t key) {
    // Check if key was released in previous frame
    bool wasReleased = !IsKeyPressed(key);
    bool isPressed = IsKeyPressed(key);
    return wasReleased && isPressed;
}
```

---

## Input Flow

### Typical Input Sequence

```
1. User presses key/mouse button
   └─> Windows sends message to launcher

2. Launcher processes input
   └─> Updates internal state
   └─> Creates InputEvent structure

3. Launcher triggers callback
   └─> Calls registered OnInput function
   └─> Passes event data

4. Client processes event
   └─> Updates game/UI state
   └─> Handles specific input type

5. Event cleanup
   └─> Mark key as released (if applicable)
   └─> Handle repeat events
```

---

## Usage Examples

### Text Input Handling

```c
void HandleTextInput(InputEvent* input) {
    if (input->eventType == INPUT_KEYDOWN) {
        // Check for printable characters
        if (input->inputId >= 0x20 && input->inputId <= 0x7E) {
            char ch = (char)input->inputId;
            AppendToBuffer(ch);
        }
    }
}
```

### Modifier Key Detection

```c
void HandleModifierKeys(InputEvent* input, uint32_t flags) {
    if (input->eventType == INPUT_KEYDOWN) {
        switch (input->inputId) {
            case VK_SHIFT:
                SetShiftPressed(true);
                break;
                
            case VK_CONTROL:
                SetCtrlPressed(true);
                break;
                
            case VK_ALT:
                SetAltPressed(true);
                break;
        }
    } else if (input->eventType == INPUT_KEYUP) {
        switch (input->inputId) {
            case VK_SHIFT:
                SetShiftPressed(false);
                break;
                
            case VK_CONTROL:
                SetCtrlPressed(false);
                break;
                
            case VK_ALT:
                SetAltPressed(false);
                break;
        }
    }
}
```

### Mouse Position Tracking

```c
void TrackMousePosition(InputEvent* input) {
    if (input->eventType == INPUT_MOUSEMOVE) {
        int32_t x = input->deltaX;
        int32_t y = input->deltaY;
        
        // Clamp to window bounds
        int32_t width = GetWindowWidth(g_ActiveWindow);
        int32_t height = GetWindowHeight(g_ActiveWindow);
        
        x = CLAMP(x, 0, width - 1);
        y = CLAMP(y, 0, height - 1);
        
        SetCursorPosition(x, y);
    }
}
```

---

## Notes

- **Critical UI callback** for all user interaction handling
- Called for keyboard, mouse, and touch input events
- Provides detailed event information including modifiers
- Should be registered early in initialization
- Input state persists across multiple callbacks
- Handle repeat events appropriately

---

## Security Considerations

- Validate input sources before processing
- Prevent key logging vulnerabilities
- Sanitize user input data
- Implement input rate limiting
- Handle malformed input gracefully

---

## Related Callbacks

- [OnFocus](OnFocus.md) - Window focus change callback
- [OnWindowEvent](OnWindowEvent.md) - Window event callback
- [OnResize](OnResize.md) - Window resize callback
- [OnClose](OnClose.md) - Window close callback

---

## VTable Functions

Related VTable functions for input:

| Index | Byte Offset | Function | Purpose |
|-------|-------------|----------|---------|
| 2 | 0x08 | Reset | Reset input state |
| 4 | 0x10 | RegisterCallback | Register callback |
| 5 | 0x14 | UnregisterCallback | Unregister callback |
| 6 | 0x18 | ProcessEvent | Process input events |

---

## References

- **Source**: `client_dll_callback_analysis.md` Section 5.1
- **Source**: `data_structures_analysis.md` InputObject section
- **Category**: UI event handling
- **Direction**: Launcher to Client callback invocation

---

**Status**: ✅ Documented  
**Confidence**: Medium (inferred from UI callback patterns)  
**Last Updated**: 2025-06-17