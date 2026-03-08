#!/usr/bin/env python3
"""
Analyze callback registration patterns in client.dll
Identifies where function pointers are stored and called back
"""

import subprocess
import re

def run_r2_command(cmd):
    """Run radare2 command on client.dll"""
    full_cmd = f"r2 -q -c '{cmd}' ../../client.dll 2>/dev/null"
    result = subprocess.run(full_cmd, shell=True, capture_output=True, text=True)
    return result.stdout

def find_callback_strings():
    """Find all strings containing 'callback' keyword"""
    print("=" * 80)
    print("CALLBACK STRINGS IN client.dll")
    print("=" * 80)
    
    output = run_r2_command("iz~callback")
    print(output)
    
    # Parse callback string addresses
    callback_strings = []
    for line in output.strip().split('\n'):
        if line:
            parts = line.split()
            if len(parts) >= 4:
                addr = parts[2]
                string = ' '.join(parts[4:])
                callback_strings.append((addr, string))
    
    return callback_strings

def analyze_vtable_calls():
    """Analyze vtable-based callback invocation patterns"""
    print("\n" + "=" * 80)
    print("VTABLE CALLBACK PATTERNS")
    print("=" * 80)
    
    # Search for patterns like: call dword [edx + offset]
    # This is how vtable callbacks are invoked
    output = run_r2_command('pd 1000 @ 0x620012a0')
    
    vtable_patterns = []
    for line in output.split('\n'):
        if 'call dword' in line and ('[edx +' in line or '[eax +' in line or '[ecx +' in line):
            vtable_patterns.append(line.strip())
    
    print("\nFound {} vtable callback invocation patterns:".format(len(vtable_patterns)))
    for i, pattern in enumerate(vtable_patterns[:20], 1):
        print(f"  {i}. {pattern}")
    
    return vtable_patterns

def analyze_callback_structures():
    """Analyze structures that hold callback function pointers"""
    print("\n" + "=" * 80)
    print("CALLBACK STRUCTURE ANALYSIS")
    print("=" * 80)
    
    # The master database structure from previous analysis
    print("\nMaster Database Structure (0x629f14a0):")
    print("  Offset 0x00: Identifier/VTable pointer")
    print("  Offset 0x04: Field 4")
    print("  Offset 0x08: Field 8")
    print("  Offset 0x0C: Object pointer 1 (callbacks at +0x20, +0x24, +0x28)")
    print("  Offset 0x10: Field 16")
    print("  Offset 0x14: Field 20")
    print("  Offset 0x18: Object pointer 2")
    print("  Offset 0x1C: Field 28")
    print("  Offset 0x20: Field 32")
    
    print("\nCallback Object Structure (from 0x620011e0 analysis):")
    print("  Offset 0x20: Callback function pointer 1")
    print("  Offset 0x24: Callback function pointer 2")
    print("  Offset 0x28: Callback function pointer 3")
    print("  Offset 0x34: Callback data storage")

def analyze_init_function():
    """Analyze InitClientDLL for callback registration"""
    print("\n" + "=" * 80)
    print("INITIALIZATION CALLBACK REGISTRATION")
    print("=" * 80)
    
    output = run_r2_command('pd 200 @ 0x620012a0')
    
    # Look for callback registration patterns
    # Pattern: push callback_addr, then call registration function
    callback_regs = []
    lines = output.split('\n')
    
    for i, line in enumerate(lines):
        # Look for pushes of immediate addresses (potential callbacks)
        if 'push 0x62' in line:
            # Check if next few lines contain a call
            for j in range(i+1, min(i+5, len(lines))):
                if 'call' in lines[j]:
                    callback_regs.append({
                        'push': line.strip(),
                        'call': lines[j].strip()
                    })
                    break
    
    print("\nFound {} potential callback registrations:".format(len(callback_regs)))
    for i, reg in enumerate(callback_regs[:10], 1):
        print(f"  {i}. {reg['push']}")
        print(f"     {reg['call']}")

def analyze_event_handlers():
    """Analyze event handler registration patterns"""
    print("\n" + "=" * 80)
    print("EVENT HANDLER PATTERNS")
    print("=" * 80)
    
    # Search for common event handler patterns
    patterns = [
        "OnEvent",
        "EventHandler",
        "RegisterCallback",
        "SetCallback",
        "AddCallback"
    ]
    
    for pattern in patterns:
        output = run_r2_command(f'ait~{pattern}')
        if output.strip():
            print(f"\nPattern '{pattern}' found:")
            print(output[:500])

def main():
    print("=" * 80)
    print("CLIENT.DLL CALLBACK ANALYSIS")
    print("Phase 3.2: Runtime Callbacks")
    print("=" * 80)
    
    # Find callback strings
    callback_strings = find_callback_strings()
    
    # Analyze vtable calls
    vtable_patterns = analyze_vtable_calls()
    
    # Analyze callback structures
    analyze_callback_structures()
    
    # Analyze initialization
    analyze_init_function()
    
    # Analyze event handlers
    analyze_event_handlers()
    
    print("\n" + "=" * 80)
    print("ANALYSIS COMPLETE")
    print("=" * 80)

if __name__ == '__main__':
    main()
