#!/usr/bin/env python3
"""
Analyze .rdata section for internal function pointer tables.
"""

import struct
import sys

# Memory map from TODO.md
TEXT_START = 0x00401000
TEXT_END = 0x004a8000
RDATA_START = 0x004a9000
RDATA_END = 0x004c6000
RDATA_FILE_OFFSET = 0xa9000

def read_dword(data, offset):
    """Read a 4-byte DWORD from data at given offset."""
    if offset + 4 > len(data):
        return None
    return struct.unpack('<I', data[offset:offset+4])[0]

def is_text_address(addr):
    """Check if address is in .text section."""
    return TEXT_START <= addr < TEXT_END

def is_rdata_address(addr):
    """Check if address is in .rdata section."""
    return RDATA_START <= addr < RDATA_END

def analyze_function_pointer_tables(exe_path):
    """Analyze .rdata section for function pointer tables."""
    
    print(f"[*] Loading {exe_path}")
    with open(exe_path, 'rb') as f:
        data = f.read()
    
    print(f"[*] File size: {len(data)} bytes")
    
    # Extract .rdata section data
    rdata_size = RDATA_END - RDATA_START
    rdata_data = data[RDATA_FILE_OFFSET:RDATA_FILE_OFFSET + rdata_size]
    
    print(f"[*] .rdata section: 0x{RDATA_START:x} - 0x{RDATA_END:x} ({rdata_size} bytes)")
    
    # Find function pointer tables
    tables = []
    current_table = None
    table_start = None
    
    for offset in range(0, len(rdata_data) - 4, 4):
        addr = read_dword(rdata_data, offset)
        
        if addr is None:
            continue
            
        # Check if this is an internal function pointer
        if is_text_address(addr):
            if current_table is None:
                # Start new table
                table_start = RDATA_START + offset
                current_table = []
            current_table.append((RDATA_START + offset, addr))
        else:
            # Not a function pointer
            if current_table is not None and len(current_table) >= 3:
                # End current table (only save if it has at least 3 entries)
                tables.append((table_start, current_table))
            current_table = None
            table_start = None
    
    # Check last table
    if current_table is not None and len(current_table) >= 3:
        tables.append((table_start, current_table))
    
    print(f"\n[*] Found {len(tables)} function pointer tables in .rdata")
    print("="*80)
    
    # Analyze each table
    for idx, (table_addr, entries) in enumerate(tables, 1):
        print(f"\n[TABLE {idx}] Address: 0x{table_addr:x}")
        print(f"  Entries: {len(entries)}")
        print(f"  Size: {len(entries) * 4} bytes")
        print(f"  Range: 0x{entries[0][0]:x} - 0x{entries[-1][0]:x}")
        print(f"\n  First 10 function pointers:")
        
        for i, (ptr_addr, func_addr) in enumerate(entries[:10]):
            print(f"    [{i:3d}] 0x{ptr_addr:x} -> 0x{func_addr:x}")
        
        if len(entries) > 10:
            print(f"    ...")
            print(f"    [{len(entries)-1:3d}] 0x{entries[-1][0]:x} -> 0x{entries[-1][1]:x}")
    
    # Generate summary statistics
    print("\n" + "="*80)
    print("\n[*] SUMMARY STATISTICS")
    print(f"Total tables found: {len(tables)}")
    
    total_pointers = sum(len(entries) for _, entries in tables)
    print(f"Total function pointers: {total_pointers}")
    
    # Group by table size
    size_distribution = {}
    for _, entries in tables:
        size = len(entries)
        size_distribution[size] = size_distribution.get(size, 0) + 1
    
    print("\n[*] Table size distribution:")
    for size in sorted(size_distribution.keys(), reverse=True):
        count = size_distribution[size]
        print(f"  {size:4d} entries: {count:3d} table(s)")
    
    # Find largest tables
    largest = sorted(tables, key=lambda x: len(x[1]), reverse=True)[:5]
    print("\n[*] Top 5 largest tables:")
    for i, (addr, entries) in enumerate(largest, 1):
        print(f"  {i}. 0x{addr:x} - {len(entries)} entries")
    
    return tables

if __name__ == '__main__':
    exe_path = '../../launcher.exe'
    tables = analyze_function_pointer_tables(exe_path)
    
    # Save results to file
    with open('function_pointer_tables.md', 'w') as f:
        f.write("# Function Pointer Tables Analysis\n\n")
        f.write("## Overview\n\n")
        f.write(f"- **Section**: .rdata (0x{RDATA_START:x} - 0x{RDATA_END:x})\n")
        f.write(f"- **Total tables found**: {len(tables)}\n")
        total_pointers = sum(len(entries) for _, entries in tables)
        f.write(f"- **Total function pointers**: {total_pointers}\n\n")
        
        f.write("## Tables Detail\n\n")
        for idx, (table_addr, entries) in enumerate(tables, 1):
            f.write(f"### Table {idx}\n\n")
            f.write(f"- **Address**: 0x{table_addr:x}\n")
            f.write(f"- **Entries**: {len(entries)}\n")
            f.write(f"- **Size**: {len(entries) * 4} bytes\n\n")
            f.write(f"| Index | Pointer Address | Function Address |\n")
            f.write(f"|-------|----------------|------------------|\n")
            
            for i, (ptr_addr, func_addr) in enumerate(entries):
                f.write(f"| {i:3d} | 0x{ptr_addr:08x} | 0x{func_addr:08x} |\n")
            f.write("\n")
    
    print("\n[*] Results saved to function_pointer_tables.md")
