#!/usr/bin/env python3
"""
String Extractor for Matrix Online Launcher
Extracts and catalogs all string resources from launcher.exe
"""

import os
import pefile

def extract_strings_from_pe(filename):
    """Extract all string resources from a PE file using pefile."""
    try:
        pe = pefile.PE(filename)
        
        # Get resource strings
        strings = pe.get_resources_strings()
        
        return strings
    except Exception as e:
        print(f"Error extracting strings: {e}")
        return []

def categorize_string(s):
    """Categorize a string into UI, status, or system."""
    is_ui = False
    is_status = False
    is_system = False
    
    # UI-related strings (buttons, labels, menu items)
    keywords_ui = ['login', 'password', 'character', 'server', 'account', 'accept', 'decline', 'create', 'delete', 'select', 'view', 'open', 'support', 'community', 'end user license', 'world instance']
    for kw in keywords_ui:
        if kw in s.lower():
            is_ui = True
            break
    
    # Status strings (Open, Closed, Full, etc.)
    status_keywords = ['Open', 'Closed', 'Full', 'Down', 'Admins Only', 'Banned', 'Char In Transit', 'Char Incomplete', 'Char Unknown', 'Success', 'Failure']
    for kw in status_keywords:
        if kw in s:
            is_status = True
            break
    
    # System messages (patching, updating, scanning)
    keywords_system = ['patch', 'update', 'scan', 'download', 'server version', 'client version', 'launcher', 'system check', 'dll']
    for kw in keywords_system:
        if kw.lower() in s.lower():
            is_system = True
            break
    
    if is_ui:
        return 'ui'
    elif is_status:
        return 'status'
    elif is_system:
        return 'system'
    else:
        return 'other'

def create_string_files(strings, output_dir):
    """Create individual string files."""
    os.makedirs(output_dir, exist_ok=True)
    
    for i, s in enumerate(strings):
        filename = f'{output_dir}/00{i+1:04d}.txt'
        with open(filename, 'w') as f:
            f.write(f"# String ID: {i+1:04d}\n")
            f.write(f"# Content:\n")
            f.write(f"{s}\n")

def create_string_catalog(strings, output_file):
    """Create the string catalog markdown file."""
    catalog = """# String Catalog - Matrix Online Launcher

## Overview
This document catalogs all string resources extracted from the Matrix Online launcher executable.

## String Reference Table

| ID | Content | Category |
|----|---------|----------|
"""
    
    for i, s in enumerate(strings):
        category = categorize_string(s)
        preview = s[:80] if len(s) > 80 else s
        catalog += f"| {i+1:04d} | {preview} | {category} |\n"
    
    catalog += "\n## String Details\n\n### UI Strings (buttons, labels, menu items)\n\n"
    
    for i, s in enumerate(strings):
        if categorize_string(s) == 'ui':
            catalog += f"- **{i+1:04d}**: {s}\n"
    
    catalog += "\n### Status Strings (server status, character status)\n\n"
    
    for i, s in enumerate(strings):
        if categorize_string(s) == 'status':
            catalog += f"- **{i+1:04d}**: {s}\n"
    
    catalog += "\n### System/Technical Strings\n\n"
    
    for i, s in enumerate(strings):
        if categorize_string(s) == 'system':
            preview = s[:60] if len(s) > 60 else s
            catalog += f"- **{i+1:04d}**: {preview}\n"
    
    with open(output_file, 'w') as f:
        f.write(catalog)

def main():
    """Main function to extract and catalog strings."""
    launcher_path = '/home/pi/mxo/launcher.exe'
    output_dir = '/home/pi/mxo/docs/resources/strings'
    catalog_file = '/home/pi/mxo/docs/resources/string_catalog.md'
    
    print(f"Extracting strings from {launcher_path}...")
    strings = extract_strings_from_pe(launcher_path)
    
    print(f"Found {len(strings)} strings")
    
    print(f"Creating string files in {output_dir}...")
    create_string_files(strings, output_dir)
    
    print(f"Creating catalog at {catalog_file}...")
    create_string_catalog(strings, catalog_file)
    
    print("Done!")

if __name__ == '__main__':
    main()