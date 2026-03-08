#!/usr/bin/env python3
"""
String Resource Extractor for launcher.exe
Extracts and catalogs all string resources from RT_STRING section
"""

import pefile
import os

def extract_strings(exe_path):
    """Extract all string resources from PE file"""
    
    pe = pefile.PE(exe_path)
    
    print(f"Analyzing {exe_path}")
    print("=" * 60)
    
    # Find string resources (ID 5)
    for i, entry in enumerate(pe.DIRECTORY_ENTRY_RESOURCE.entries):
        if hasattr(entry, 'id') and entry.id == 5:
            print(f'\nFound string resources at Entry {i} (ID=5)')
            
            # Get directory
            directory = entry.directory
            
            # Try to get children
            if hasattr(directory, 'entries'):
                entries = directory.entries
                
                # Print entries
                for j, child in enumerate(entries):
                    print(f'\n  Entry {j} (ID: {child.id}):')
                    
                    # Try to get id and name
                    if hasattr(child, 'id'):
                        print(f'    ID: {child.id}')
                    
                    # Try to get children (sub-entries)
                    if hasattr(child, 'directory'):
                        sub_dir = child.directory
                        
                        if hasattr(sub_dir, 'entries'):
                            sub_entries = sub_dir.entries
                            
                            # Print sub-entries
                            for k, sub_child in enumerate(sub_entries):
                                print(f'      Sub-entry {k} (ID: {sub_child.id}):')
                                
                                # Try to get data
                                if hasattr(sub_child, 'directory'):
                                    sub_data = sub_child.directory
                                    
                                    if hasattr(sub_data, 'entries'):
                                        sub_data_entries = sub_data.entries
                                        
                                        # Print sub-data entries
                                        for m, sub_data_child in enumerate(sub_data_entries):
                                            print(f'        Sub-data entry {m}: {sub_data_child}')
                                            if hasattr(sub_data_child, 'id'):
                                                print(f'          ID: {sub_data_child.id}')
                                            
                                            # Try to get data field
                                            if hasattr(sub_data_child, 'data'):
                                                data = sub_data_child.data
                                                print(f'          Data: {data}')
                                                if isinstance(data, bytes):
                                                    print(f'          Data length: {len(data)}')
                                                    print(f'          Data type: {type(data)}')
                                            
                                            # Try to get entries from directory
                                            if hasattr(sub_data_child, 'directory'):
                                                final_dir = sub_data_child.directory
                                                
                                                if hasattr(final_dir, 'entries'):
                                                    final_entries = final_dir.entries
                                                    print(f'          Final entries count: {len(final_entries) if isinstance(final_entries, list) else "N/A"}')
                                                    
                                                    # Print final entries
                                                    for n, final_child in enumerate(final_entries):
                                                        print(f'            Final entry {n}: {final_child}')
                                                        if hasattr(final_child, 'id'):
                                                            print(f'              ID: {final_child.id}')
                                                        if hasattr(final_child, 'name'):
                                                            print(f'              Name: {final_child.name}')
                                                        
                                                        # Try to get data from final entry
                                                        if hasattr(final_child, 'data'):
                                                            data = final_child.data
                                                            if isinstance(data, bytes):
                                                                try:
                                                                    decoded = data.decode("utf-16-le").strip()
                                                                    print(f'              Decoded (utf-16-le): "{decoded}"')
                                                                except Exception as e:
                                                                    print(f'              Decode error (utf-16-le): {e}')
                                                                    try:
                                                                        decoded = data.decode("latin-1").strip()
                                                                        print(f'              Decoded (latin-1): "{decoded}"')
                                                                    except Exception as e2:
                                                                        print(f'              Decode error (latin-1): {e2}')
                                                                        print(f'              Raw bytes: {data.hex()}')
    
    return pe

if __name__ == "__main__":
    extract_strings("/home/pi/mxo/launcher.exe")