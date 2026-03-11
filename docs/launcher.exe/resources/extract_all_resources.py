#!/usr/bin/env python3
"""
Comprehensive Resource Extraction Script
Extracts and documents all resources from launcher.exe
"""

import os
import subprocess
import re
from datetime import datetime

# Configuration
LAUNCHER_PATH = "/home/pi/mxo/launcher.exe"
OUTPUT_DIR = "/home/pi/mxo/docs/resources"
STRINGS_DIR = os.path.join(OUTPUT_DIR, "strings")
RESOURCES_DIR = os.path.join(OUTPUT_DIR, "resources")
DOCUMENTATION_DIR = os.path.join(OUTPUT_DIR, "documentation")

def run_command(cmd, description=""):
    """Run a bash command and return output"""
    print(f"[{datetime.now().strftime('%H:%M:%S')}] Running: {description}")
    print(f"        Command: {cmd}")
    result = subprocess.run(cmd, shell=True, capture_output=True, text=True)
    if result.stdout:
        print(f"        Output: {result.stdout[:500]}")
    if result.stderr:
        print(f"        Errors: {result.stderr[:500]}")
    return result.returncode == 0, result.stdout, result.stderr

def list_all_resources():
    """List all resource types in launcher.exe"""
    print("\n" + "="*60)
    print("LISTING ALL RESOURCE TYPES")
    print("="*60)
    
    success, stdout, stderr = run_command(f"resdump {LAUNCHER_PATH}", "resdump launcher.exe")
    
    if success:
        # Parse and display resource types
        lines = stdout.split('\n')
        for line in lines[:50]:  # First 50 lines
            if line.strip():
                print(line)
        return stdout
    return ""

def extract_string_resources():
    """Extract all string resources"""
    print("\n" + "="*60)
    print("EXTRACTING STRING RESOURCES")
    print("="*60)
    
    # Extract string resources
    success, stdout, stderr = run_command(f"res {LAUNCHER_PATH} string > {STRINGS_DIR}/strings.res", "Extracting strings")
    
    if success:
        print(f"        Strings extracted to {STRINGS_DIR}/strings.res")
        return True
    return False

def extract_all_resources():
    """Extract all resource types"""
    print("\n" + "="*60)
    print("EXTRACTING ALL RESOURCES")
    print("="*60)
    
    # Create output directories
    os.makedirs(RESOURCES_DIR, exist_ok=True)
    os.makedirs(DOCUMENTATION_DIR, exist_ok=True)
    
    # Extract all resources
    success, stdout, stderr = run_command(f"res {LAUNCHER_PATH} > {RESOURCES_DIR}/resources.bin", "Extracting all resources")
    
    if success:
        print(f"        All resources extracted to {RESOURCES_DIR}/resources.bin")
        return True
    return False

def extract_icon_resources():
    """Extract icon resources"""
    print("\n" + "="*60)
    print("EXTRACTING ICON RESOURCES")
    print("="*60)
    
    os.makedirs(os.path.join(RESOURCES_DIR, "icons"), exist_ok=True)
    
    # Extract icon resources
    success, stdout, stderr = run_command(f"res {LAUNCHER_PATH} icon > {RESOURCES_DIR}/icons/icons.res", "Extracting icons")
    
    if success:
        print(f"        Icons extracted to {RESOURCES_DIR}/icons/icons.res")
        return True
    return False

def extract_dialog_resources():
    """Extract dialog resources"""
    print("\n" + "="*60)
    print("EXTRACTING DIALOG RESOURCES")
    print("="*60)
    
    os.makedirs(os.path.join(RESOURCES_DIR, "dialogs"), exist_ok=True)
    
    # Extract dialog resources
    success, stdout, stderr = run_command(f"res {LAUNCHER_PATH} dialog > {RESOURCES_DIR}/dialogs/dialogs.res", "Extracting dialogs")
    
    if success:
        print(f"        Dialogs extracted to {RESOURCES_DIR}/dialogs/dialogs.res")
        return True
    return False

def extract_bitmap_resources():
    """Extract bitmap resources"""
    print("\n" + "="*60)
    print("EXTRACTING BITMAP RESOURCES")
    print("="*60)
    
    os.makedirs(os.path.join(RESOURCES_DIR, "bitmaps"), exist_ok=True)
    
    # Extract bitmap resources
    success, stdout, stderr = run_command(f"res {LAUNCHER_PATH} bitmap > {RESOURCES_DIR}/bitmaps/bitmaps.res", "Extracting bitmaps")
    
    if success:
        print(f"        Bitmaps extracted to {RESOURCES_DIR}/bitmaps/bitmaps.res")
        return True
    return False

def extract_version_info():
    """Extract version information"""
    print("\n" + "="*60)
    print("EXTRACTING VERSION INFORMATION")
    print("="*60)
    
    os.makedirs(os.path.join(RESOURCES_DIR, "version"), exist_ok=True)
    
    # Extract version info
    success, stdout, stderr = run_command(f"res {LAUNCHER_PATH} version > {RESOURCES_DIR}/version/version.res", "Extracting version info")
    
    if success:
        print(f"        Version info extracted to {RESOURCES_DIR}/version/version.res")
        return True
    return False

def count_string_files():
    """Count string files in the strings directory"""
    print("\n" + "="*60)
    print("COUNTING STRING FILES")
    print("="*60)
    
    if os.path.exists(STRINGS_DIR):
        files = [f for f in os.listdir(STRINGS_DIR) if f.endswith('.txt')]
        print(f"        Total string files: {len(files)}")
        return len(files)
    return 0

def parse_string_files():
    """Parse all string files and generate catalog"""
    print("\n" + "="*60)
    print("PARSING STRING FILES")
    print("="*60)
    
    if not os.path.exists(STRINGS_DIR):
        print("        Strings directory not found")
        return []
    
    strings = []
    for filename in sorted(os.listdir(STRINGS_DIR)):
        if filename.endswith('.txt'):
            filepath = os.path.join(STRINGS_DIR, filename)
            try:
                with open(filepath, 'r') as f:
                    content = f.read()
                    
                # Parse ID
                id_match = re.search(r'# String ID: (\d+)', content)
                if id_match:
                    string_id = id_match.group(1)
                    
                    # Parse content
                    content_match = re.search(r'# Content:\s*(.+)', content, re.DOTALL)
                    if content_match:
                        content = content_match.group(1).strip()
                        
                        strings.append({
                            'id': string_id,
                            'content': content
                        })
            except Exception as e:
                print(f"        Error reading {filename}: {e}")
    
    print(f"        Parsed {len(strings)} strings")
    return strings

def generate_resource_table(strings):
    """Generate comprehensive resource table"""
    print("\n" + "="*60)
    print("GENERATING RESOURCE TABLE")
    print("="*60)
    
    table_path = os.path.join(DOCUMENTATION_DIR, "resource_table.md")
    
    with open(table_path, 'w') as f:
        f.write("# Resource Table\n\n")
        f.write(f"**Generated:** {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n\n")
        f.write("## Overview\n\n")
        f.write(f"This document contains a comprehensive table of all resources extracted from launcher.exe.\n\n")
        f.write(f"## Summary\n\n")
        f.write(f"- **Total String Resources:** {len(strings)}\n")
        f.write(f"- **String Directory:** {STRINGS_DIR}\n")
        f.write(f"- **Resources Directory:** {RESOURCES_DIR}\n\n")
        
        # Write string catalog
        f.write("## String Catalog\n\n")
        f.write("| ID | Content |\n")
        f.write("|----|---------|\n")
        
        for s in strings:
            # Truncate long content
            content = s['content']
            if len(content) > 100:
                content = content[:100] + "..."
            f.write(f"| {s['id']} | {content} |\n")
        
        f.write("\n## String Statistics\n\n")
        
        # Group strings by length
        short_strings = [s for s in strings if len(s['content']) < 50]
        medium_strings = [s for s in strings if 50 <= len(s['content']) < 200]
        long_strings = [s for s in strings if len(s['content']) >= 200]
        
        f.write(f"- **Short strings (< 50 chars):** {len(short_strings)}\n")
        f.write(f"- **Medium strings (50-200 chars):** {len(medium_strings)}\n")
        f.write(f"- **Long strings (> 200 chars):** {len(long_strings)}\n\n")
        
        # Write UI-related strings (strings with common UI terms)
        ui_terms = ['Login', 'Password', 'Connect', 'Disconnect', 'Server', 'About', 'Help', 'Settings', 'Options', 'File', 'Edit', 'View', 'Window', 'Menu']
        ui_strings = [s for s in strings if any(term in s['content'] for term in ui_terms)]
        
        f.write(f"## UI-Related Strings\n\n")
        f.write("| ID | Content |\n")
        f.write("|----|---------|\n")
        
        for s in ui_strings:
            content = s['content']
            if len(content) > 100:
                content = content[:100] + "..."
            f.write(f"| {s['id']} | {content} |\n")
    
    print(f"        Resource table written to {table_path}")

def generate_string_catalog(strings):
    """Generate detailed string catalog"""
    print("\n" + "="*60)
    print("GENERATING STRING CATALOG")
    print("="*60)
    
    catalog_path = os.path.join(DOCUMENTATION_DIR, "string_catalog.md")
    
    with open(catalog_path, 'w') as f:
        f.write("# String Catalog\n\n")
        f.write(f"**Generated:** {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n\n")
        f.write("## Complete String Reference\n\n")
        f.write("This document contains all string resources extracted from launcher.exe.\n\n")
        
        for s in strings:
            f.write(f"### String ID: {s['id']}\n\n")
            f.write(f"**Content:**\n```\n{s['content']}\n```\n\n")
    
    print(f"        String catalog written to {catalog_path}")

def generate_ui_strings(strings):
    """Generate UI-related strings catalog"""
    print("\n" + "="*60)
    print("GENERATING UI STRINGS CATALOG")
    print("="*60)
    
    ui_path = os.path.join(DOCUMENTATION_DIR, "ui_strings.md")
    
    # UI terms to search for
    ui_terms = ['Login', 'Password', 'Connect', 'Disconnect', 'Server', 'About', 'Help', 
                'Settings', 'Options', 'File', 'Edit', 'View', 'Window', 'Menu',
                'Username', 'User', 'Admin', 'System', 'Network']
    
    ui_strings = [s for s in strings if any(term in s['content'] for term in ui_terms)]
    
    with open(ui_path, 'w') as f:
        f.write("# UI-Related Strings\n\n")
        f.write(f"**Generated:** {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n\n")
        f.write("## User Interface Strings\n\n")
        f.write("This document contains strings related to the user interface.\n\n")
        
        if ui_strings:
            f.write("| ID | Content |\n")
            f.write("|----|---------|\n")
            
            for s in ui_strings:
                content = s['content']
                if len(content) > 100:
                    content = content[:100] + "..."
                f.write(f"| {s['id']} | {content} |\n")
        else:
            f.write("No UI-related strings found.\n")
    
    print(f"        UI strings catalog written to {ui_path}")

def generate_error_messages(strings):
    """Generate error message catalog"""
    print("\n" + "="*60)
    print("GENERATING ERROR MESSAGES CATALOG")
    print("="*60)
    
    error_path = os.path.join(DOCUMENTATION_DIR, "error_messages.md")
    
    # Error terms to search for
    error_terms = ['Error', 'Failed', 'Warning', 'Critical', 'Exception',
                   'Invalid', 'Not found', 'Unable', 'Cannot', 'Missing',
                   'Timeout', 'Connection']
    
    error_strings = [s for s in strings if any(term in s['content'] for term in error_terms)]
    
    with open(error_path, 'w') as f:
        f.write("# Error Messages\n\n")
        f.write(f"**Generated:** {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n\n")
        f.write("## Error Message Reference\n\n")
        f.write("This document contains all error-related strings.\n\n")
        
        if error_strings:
            f.write("| ID | Content |\n")
            f.write("|----|---------|\n")
            
            for s in error_strings:
                content = s['content']
                if len(content) > 100:
                    content = content[:100] + "..."
                f.write(f"| {s['id']} | {content} |\n")
        else:
            f.write("No error-related strings found.\n")
    
    print(f"        Error messages catalog written to {error_path}")

def generate_summary_report(strings):
    """Generate extraction summary report"""
    print("\n" + "="*60)
    print("GENERATING SUMMARY REPORT")
    print("="*60)
    
    report_path = os.path.join(DOCUMENTATION_DIR, "extraction_summary.md")
    
    with open(report_path, 'w') as f:
        f.write("# Resource Extraction Summary\n\n")
        f.write(f"**Generated:** {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n\n")
        f.write("## Overview\n\n")
        f.write("This report summarizes the extraction of all resources from launcher.exe.\n\n")
        
        f.write("## Statistics\n\n")
        f.write(f"- **Total String Resources:** {len(strings)}\n")
        f.write(f"- **String Directory:** {STRINGS_DIR}\n")
        f.write(f"- **Resources Directory:** {RESOURCES_DIR}\n")
        f.write(f"- **Documentation Directory:** {DOCUMENTATION_DIR}\n\n")
        
        # File sizes
        f.write("## File Sizes\n\n")
        
        for item in ['resources.bin', 'strings.res']:
            filepath = os.path.join(RESOURCES_DIR, item)
            if os.path.exists(filepath):
                size = os.path.getsize(filepath)
                f.write(f"- **{item}:** {size:,} bytes\n")
        
        # String statistics
        short_strings = [s for s in strings if len(s['content']) < 50]
        medium_strings = [s for s in strings if 50 <= len(s['content']) < 200]
        long_strings = [s for s in strings if len(s['content']) >= 200]
        
        f.write("\n## String Statistics\n\n")
        f.write(f"- **Short strings (< 50 chars):** {len(short_strings)}\n")
        f.write(f"- **Medium strings (50-200 chars):** {len(medium_strings)}\n")
        f.write(f"- **Long strings (> 200 chars):** {len(long_strings)}\n\n")
        
        # UI strings
        ui_terms = ['Login', 'Password', 'Connect', 'Disconnect', 'Server', 'About', 'Help']
        ui_strings = [s for s in strings if any(term in s['content'] for term in ui_terms)]
        
        f.write(f"- **UI-related strings:** {len(ui_strings)}\n")
        
        # Error strings
        error_terms = ['Error', 'Failed', 'Warning', 'Critical', 'Exception']
        error_strings = [s for s in strings if any(term in s['content'] for term in error_terms)]
        
        f.write(f"- **Error-related strings:** {len(error_strings)}\n\n")
        
        f.write("## Generated Documentation\n\n")
        f.write(f"- **Resource Table:** {table_path}\n")
        f.write(f"- **String Catalog:** {catalog_path}\n")
        f.write(f"- **UI Strings:** {ui_path}\n")
        f.write(f"- **Error Messages:** {error_path}\n")
        f.write(f"- **Extraction Summary:** {report_path}\n\n")
    
    print(f"        Summary report written to {report_path}")

def main():
    """Main execution function"""
    print("\n" + "="*60)
    print("RESOURCE EXTRACTION SCRIPT")
    print("="*60)
    print(f"Started at: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
    
    # Create output directories
    os.makedirs(STRINGS_DIR, exist_ok=True)
    os.makedirs(RESOURCES_DIR, exist_ok=True)
    os.makedirs(DOCUMENTATION_DIR, exist_ok=True)
    
    # Step 1: List all resources
    print("\n[Step 1] Listing all resource types...")
    list_all_resources()
    
    # Step 2: Count existing string files
    print("\n[Step 2] Counting existing string files...")
    string_count = count_string_files()
    print(f"        Found {string_count} string files")
    
    # Step 3: Extract all resources
    print("\n[Step 3] Extracting all resources...")
    extract_all_resources()
    
    # Step 4: Extract specific resource types
    print("\n[Step 4] Extracting specific resource types...")
    extract_icon_resources()
    extract_dialog_resources()
    extract_bitmap_resources()
    extract_version_info()
    
    # Step 5: Parse string files
    print("\n[Step 5] Parsing string files...")
    strings = parse_string_files()
    
    # Step 6: Generate documentation
    print("\n[Step 6] Generating documentation...")
    generate_resource_table(strings)
    generate_string_catalog(strings)
    generate_ui_strings(strings)
    generate_error_messages(strings)
    generate_summary_report(strings)
    
    # Final summary
    print("\n" + "="*60)
    print("EXTRACTION COMPLETE")
    print("="*60)
    print(f"Total string resources: {len(strings)}")
    print(f"Documentation written to: {DOCUMENTATION_DIR}")
    print(f"\nFiles created:")
    print(f"  - Resource Table: {os.path.join(DOCUMENTATION_DIR, 'resource_table.md')}")
    print(f"  - String Catalog: {os.path.join(DOCUMENTATION_DIR, 'string_catalog.md')}")
    print(f"  - UI Strings: {os.path.join(DOCUMENTATION_DIR, 'ui_strings.md')}")
    print(f"  - Error Messages: {os.path.join(DOCUMENTATION_DIR, 'error_messages.md')}")
    print(f"  - Extraction Summary: {os.path.join(DOCUMENTATION_DIR, 'extraction_summary.md')}")
    print(f"\nResources extracted to: {RESOURCES_DIR}")
    
    print(f"\nCompleted at: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")

if __name__ == "__main__":
    main()