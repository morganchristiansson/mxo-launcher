#!/usr/bin/env python3
"""
Error Messages Extractor for Matrix Online Launcher
Extracts and catalogs all error messages from launcher.exe
"""

import os
import pefile

def extract_strings_from_pe(filename):
    """Extract all string resources from a PE file."""
    try:
        pe = pefile.PE(filename)
        strings = pe.get_resources_strings()
        return strings
    except Exception as e:
        print(f"Error extracting strings: {e}")
        return []

def is_error_message(s):
    """Check if a string is an error message."""
    error_indicators = [
        'error', 'failed', 'unable', 'cannot', 'invalid', 'incompatible',
        'timeout', 'unavailable', 'access denied', 'locked', 'missing',
        'corrupt', 'crash', 'shutting down', 'suspended', 'banned',
        'incorrect', 'wrong', 'not sent', 'not found', 'not open'
    ]
    
    s_lower = s.lower()
    for indicator in error_indicators:
        if indicator in s_lower:
            return True
    return False

def is_ui_message(s):
    """Check if a string is a UI message (button, label, menu item)."""
    ui_indicators = [
        'login', 'password', 'character', 'server', 'account', 'accept',
        'decline', 'create', 'delete', 'select', 'view', 'open', 'status',
        'support', 'community', 'end user license', 'world instance',
        'cancel', 'ok', 'yes', 'no', 'apply', 'browse', 'choose'
    ]
    
    s_lower = s.lower()
    for indicator in ui_indicators:
        if indicator in s_lower:
            return True
    return False

def is_status_message(s):
    """Check if a string is a status message."""
    status_indicators = [
        'open', 'closed', 'full', 'down', 'admin', 'banned',
        'in transit', 'incomplete', 'unknown', 'success', 'failure'
    ]
    
    s_lower = s.lower()
    for indicator in status_indicators:
        if indicator in s_lower:
            return True
    return False

def create_error_catalog(strings, output_file):
    """Create error message catalog."""
    catalog = """# Error Messages Catalog - Matrix Online Launcher

## Overview
This document catalogs all error messages found in the Matrix Online launcher.

## Error Message Reference

| ID | Error Message | Severity |
|----|---------------|----------|
"""
    
    for i, s in enumerate(strings):
        if is_error_message(s):
            severity = 'critical' if 'critical' in s.lower() or 'shutting down' in s.lower() else 'warning'
            preview = s[:70] if len(s) > 70 else s
            catalog += f"| {i+1:04d} | **{preview}** | {severity} |\n"
    
    catalog += "\n## Critical Errors\n\n"
    
    for i, s in enumerate(strings):
        if is_error_message(s) and ('critical' in s.lower() or 'shutting down' in s.lower()):
            catalog += f"- **{i+1:04d}**: {s}\n"
    
    catalog += "\n## Authentication Errors\n\n"
    
    for i, s in enumerate(strings):
        if 'auth' in s.lower() or 'password' in s.lower() or 'login' in s.lower():
            if is_error_message(s) or 'incorrect' in s.lower() or 'timeout' in s.lower():
                catalog += f"- **{i+1:04d}**: {s[:60]}{'...' if len(s) > 60 else ''}\n"
    
    catalog += "\n## Server Connection Errors\n\n"
    
    for i, s in enumerate(strings):
        if 'server' in s.lower() or 'connection' in s.lower():
            if is_error_message(s) or 'unavailable' in s.lower() or 'inaccessible' in s.lower():
                catalog += f"- **{i+1:04d}**: {s[:60]}{'...' if len(s) > 60 else ''}\n"
    
    catalog += "\n## File System Errors\n\n"
    
    for i, s in enumerate(strings):
        if 'file' in s.lower() or 'disk' in s.lower():
            if is_error_message(s) or 'not found' in s.lower() or 'locked' in s.lower():
                catalog += f"- **{i+1:04d}**: {s[:60]}{'...' if len(s) > 60 else ''}\n"
    
    catalog += "\n## Patching/Update Errors\n\n"
    
    for i, s in enumerate(strings):
        if 'patch' in s.lower() or 'update' in s.lower():
            if is_error_message(s) or 'failed' in s.lower() or 'error' in s.lower():
                catalog += f"- **{i+1:04d}**: {s[:60]}{'...' if len(s) > 60 else ''}\n"
    
    with open(output_file, 'w') as f:
        f.write(catalog)

def create_ui_catalog(strings, output_file):
    """Create UI message catalog."""
    catalog = """# UI Messages Catalog - Matrix Online Launcher

## Overview
This document catalogs all user interface messages found in the Matrix Online launcher.

## UI Message Reference

| ID | UI Message | Type |
|----|------------|------|
"""
    
    for i, s in enumerate(strings):
        if is_ui_message(s):
            msg_type = 'button' if len(s) <= 30 else 'label'
            preview = s[:60] if len(s) > 60 else s
            catalog += f"| {i+1:04d} | {preview} | {msg_type} |\n"
    
    catalog += "\n## Login Dialog Messages\n\n"
    
    for i, s in enumerate(strings):
        if 'login' in s.lower() or 'password' in s.lower():
            catalog += f"- **{i+1:04d}**: {s}\n"
    
    catalog += "\n## Server Selection Messages\n\n"
    
    for i, s in enumerate(strings):
        if 'server' in s.lower() and is_status_message(s):
            catalog += f"- **{i+1:04d}**: {s}\n"
    
    catalog += "\n## Character Management Messages\n\n"
    
    for i, s in enumerate(strings):
        if 'character' in s.lower():
            catalog += f"- **{i+1:04d}**: {s[:60]}{'...' if len(s) > 60 else ''}\n"
    
    catalog += "\n## Account Messages\n\n"
    
    for i, s in enumerate(strings):
        if 'account' in s.lower():
            catalog += f"- **{i+1:04d}**: {s[:60]}{'...' if len(s) > 60 else ''}\n"
    
    with open(output_file, 'w') as f:
        f.write(catalog)

def main():
    """Main function to extract error messages and UI strings."""
    launcher_path = '/home/pi/mxo/launcher.exe'
    
    print(f"Extracting strings from {launcher_path}...")
    strings = extract_strings_from_pe(launcher_path)
    
    print(f"Found {len(strings)} strings")
    
    error_catalog = '/home/pi/mxo/docs/resources/error_messages.md'
    ui_catalog = '/home/pi/mxo/docs/resources/ui_strings.md'
    
    print(f"Creating error messages catalog at {error_catalog}...")
    create_error_catalog(strings, error_catalog)
    
    print(f"Creating UI messages catalog at {ui_catalog}...")
    create_ui_catalog(strings, ui_catalog)
    
    print("Done!")

if __name__ == '__main__':
    main()