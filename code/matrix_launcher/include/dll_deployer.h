/**
 * DLL Deployer Header
 * 
 * Declarations for DLL deployment module.
 */

#ifndef DLL_DEPLOYER_H
#define DLL_DEPLOYER_H

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Deploy a single DLL from source to destination
 * 
 * @param source_path Path to source DLL
 * @param dest_path Path to destination DLL
 * @return TRUE if successful, FALSE otherwise
 */
BOOL deploy_dll(const char* source_path, const char* dest_path);

/**
 * Deploy multiple DLLs at once
 * 
 * @param count Number of DLLs to deploy
 * @param sources Array of source paths
 * @param destinations Array of destination paths
 * @return Number of successful deployments
 */
int deploy_multiple_dlls(int count, const char* sources[], const char* destinations[]);

/**
 * Deploy all standard DLLs from p_dlls/ to game root
 * 
 * Deploys: dllWebBrowser.dll, patchctl.dll, xpatcher.dll
 * 
 * @return Number of successful deployments
 */
int deploy_standard_dlls(void);

#ifdef __cplusplus
}
#endif

#endif /* DLL_DEPLOYER_H */