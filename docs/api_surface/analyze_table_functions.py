#!/usr/bin/env python3
"""
Analyze the largest function pointer table to identify patterns.
"""

import subprocess
import re

def get_function_info(r2_path, address):
    """Get basic info about a function using radare2."""
    try:
        # Get first 3 instructions
        cmd = f'r2 -q -c "pd 3 @ {address}" {r2_path} 2>/dev/null'
        result = subprocess.run(cmd, shell=True, capture_output=True, text=True, timeout=5)
        
        output = result.stdout.strip()
        if not output:
            return None
            
        # Extract the instructions
        lines = output.split('\n')
        instructions = []
        for line in lines:
            # Remove ANSI codes
            clean = re.sub(r'\x1b\[[0-9;]*m', '', line)
            instructions.append(clean.strip())
        
        return instructions
    except Exception as e:
        return None

def analyze_largest_table():
    """Analyze the largest function pointer table."""
    
    # Read the function pointer tables
    with open('function_pointer_tables.md', 'r') as f:
        content = f.read()
    
    # Find Table 83 (the largest one at 0x4b62b8 with 250 entries)
    # Extract function addresses from the markdown
    import re
    
    # Pattern to match table entries
    pattern = r'\|\s+(\d+)\s+\|\s+0x([0-9a-f]+)\s+\|\s+0x([0-9a-f]+)\s+\|'
    
    # Find the section for Table 83
    table_section = re.search(r'### Table 83.*?(?=### Table|\Z)', content, re.DOTALL)
    
    if not table_section:
        print("[-] Could not find Table 83")
        return
    
    matches = re.findall(pattern, table_section.group(0))
    
    if not matches:
        print("[-] No function pointers found in Table 83")
        return
    
    print(f"[*] Analyzing largest table (250 entries)")
    print("="*80)
    
    # Analyze first 20 functions
    print("\n[*] First 20 functions in the table:")
    print("-"*80)
    
    for idx, ptr_addr, func_addr in matches[:20]:
        func_addr_int = int(func_addr, 16)
        
        # Get function info
        instructions = get_function_info('../../launcher.exe', hex(func_addr_int))
        
        print(f"\n[{idx}] Function at 0x{func_addr}")
        if instructions:
            for instr in instructions[:3]:  # First 3 instructions only
                print(f"    {instr}")
        else:
            print("    [Could not disassemble]")
    
    # Analyze common patterns
    print("\n" + "="*80)
    print("[*] Pattern Analysis:")
    
    # Count nullsubs and thunks
    nullsub_count = 0
    thunk_count = 0
    
    for idx, ptr_addr, func_addr in matches:
        func_addr_int = int(func_addr, 16)
        
        # Common nullsub addresses
        if func_addr_int == 0x441790:
            nullsub_count += 1
        
        # Check for common patterns
        if func_addr_int >= 0x48b000 and func_addr_int < 0x48c000:
            thunk_count += 1
    
    print(f"  - Nullsub placeholders (0x441790): {nullsub_count}")
    print(f"  - Potential thunks (0x48b000-0x48c000): {thunk_count}")
    print(f"  - Actual implementations: {len(matches) - nullsub_count - thunk_count}")

if __name__ == '__main__':
    analyze_largest_table()
