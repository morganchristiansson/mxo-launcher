/**
 * DLL Deployer Module
 * 
 * Handles deployment of DLLs from p_dlls/ to the game root directory.
 * Compares timestamps and sizes to determine if copy is needed.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include "dll_deployer.h"

/**
 * Check if a file exists using _stat()
 * 
 * @param filename Path to the file
 * @return 1 if file exists, 0 otherwise
 */
int file_exists(const char* filename)
{
    struct _stat st;
    
    if (_stat(filename, &st) == 0)
        return 1;
    
    return 0;
}

/**
 * Get file timestamp (modification time)
 * 
 * @param filename Path to the file
 * @return Time_t value of modification time, or -1 if error
 */
time_t get_file_timestamp(const char* filename)
{
    struct _stat st;
    
    if (_stat(filename, &st) != 0)
        return -1;
    
    return st.st_mtime;
}

/**
 * Get file size in bytes
 * 
 * @param filename Path to the file
 * @return File size in bytes, or -1 if error
 */
long long get_file_size(const char* filename)
{
    struct _stat st;
    
    if (_stat(filename, &st) != 0)
        return -1;
    
    return (long long)st.st_size;
}

/**
 * Deploy DLL from source to destination
 * 
 * Checks if files differ by comparing timestamps and sizes.
 * If they differ, copies the file using CopyFileA().
 * 
 * @param source_path Path to source DLL
 * @param dest_path Path to destination DLL
 * @return TRUE if deployment successful, FALSE otherwise
 */
BOOL deploy_dll(const char* source_path, const char* dest_path)
{
    // Step 1: Check if source file exists
    if (!file_exists(source_path)) {
        fprintf(stderr, "Error: Source file '%s' does not exist\n", source_path);
        return FALSE;
    }
    
    // Step 2: Check if destination file exists
    if (file_exists(dest_path)) {
        time_t src_time = get_file_timestamp(source_path);
        time_t dest_time = get_file_timestamp(dest_path);
        
        long long src_size = get_file_size(source_path);
        long long dest_size = get_file_size(dest_path);
        
        // Step 3: Compare file timestamps and sizes
        if (src_time == dest_time && src_size == dest_size) {
            // Files are identical, no need to copy
            return TRUE;
        }
        
        // Files differ, proceed to copy
    }
    
    // Step 4: Copy source to destination using CopyFileA()
    if (!CopyFileA(source_path, dest_path, FALSE)) {
        fprintf(stderr, "Error: Failed to copy '%s' to '%s'", source_path, dest_path);
        return FALSE;
    }
    
    // Step 5: Return TRUE on success
    return TRUE;
}

/**
 * Deploy multiple DLLs from p_dlls/ to game root
 * 
 * @param count Number of DLLs to deploy
 * @param sources Array of source paths
 * @param destinations Array of destination paths
 * @return Number of successful deployments
 */
int deploy_multiple_dlls(int count, const char* sources[], const char* destinations[])
{
    int successful = 0;
    
    for (int i = 0; i < count; i++) {
        if (deploy_dll(sources[i], destinations[i])) {
            printf("Successfully deployed: %s -> %s\n", sources[i], destinations[i]);
            successful++;
        } else {
            printf("Failed to deploy: %s -> %s\n", sources[i], destinations[i]);
        }
    }
    
    return successful;
}

/**
 * Deploy all standard DLLs from p_dlls/ to game root
 * 
 * Deploys: dllWebBrowser.dll, patchctl.dll, xpatcher.dll
 * 
 * @return Number of successful deployments
 */
int deploy_standard_dlls(void)
{
    const char* source_paths[] = {
        "p_dlls/dllWebBrowser.dll",
        "p_dlls/patchctl.dll",
        "p_dlls/xpatcher.dll"
    };
    
    const char* dest_paths[] = {
        "dllWebBrowser.dll",
        "patchctl.dll",
        "xpatcher.dll"
    };
    
    int count = 3;
    return deploy_multiple_dlls(count, source_paths, dest_paths);
}