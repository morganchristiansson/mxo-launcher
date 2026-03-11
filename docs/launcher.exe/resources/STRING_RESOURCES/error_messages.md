# Error Messages Catalog - Matrix Online Launcher

## Overview
This document catalogs all error messages found in the Matrix Online launcher.

## Error Message Reference

| ID | Error Message | Severity |
|----|---------------|----------|
| 0005 | **Failed to create a copy of the launcher or a critical support library ** | critical |
| 0006 | **Could not open logfile: %s.  The file is read-only or locked by anothe** | warning |
| 0007 | **Cannot create unique event.  GetLastError() returned %d.  The applicat** | warning |
| 0008 | **Deleted characters cannot be recovered.  Are you certain you wish to d** | warning |
| 0009 | **Your client has successfully patched however the server is not open fo** | warning |
| 0010 | **Error: Unable to load patching system interface DLL** | warning |
| 0011 | **Error: Unable to load patch function from patching system interface DL** | warning |
| 0013 | **Error: Could not initialize internet connection.  You must be connecte** | warning |
| 0014 | **Your username or password is incorrect. Please try again. If the error** | warning |
| 0015 | **Your account is already in use. If this is in error, click on the Supp** | warning |
| 0016 | **The auth servers are unavailable. Please try again later.** | warning |
| 0017 | **Incompatible client version. Please quit, restart, and retry with "Ful** | warning |
| 0021 | **Patch system error (result code %i):

%s** | warning |
| 0024 | **The auth servers are unavailable. Please try again later.** | warning |
| 0026 | **An error has occurred while connecting to the patch download server.
** | warning |
| 0027 | **Your account has been suspended by a Game Master until %s. Additional ** | warning |
| 0029 | **Unable to delete %s from disk.  To completely erase your character, pl** | warning |
| 0039 | **Error** | warning |
| 0042 | **Banned** | warning |
| 0045 | **The Matrix Online client crashed at some point in the past and the cra** | warning |
| 0059 | **an error ocurred while connecting to the patch server** | warning |
| 0066 | **Error: Could not find version or version is invalid. Check your starti** | warning |
| 0072 | **The file %s could not be found, or an error occured while loading it!
** | critical |
| 0073 | **The file %s could not be initialized properly.
The application is now ** | critical |
| 0074 | **The file %s could not be setup properly.

%s

The application is now s** | critical |
| 0075 | **An error occured while running %s.

%s

The application is now shuttin** | critical |
| 0079 | **Launcher clone could not be started.  GetLastError() returned error co** | warning |
| 0080 | **Login request not sent - bad parameters?** | warning |
| 0081 | **The character could not be deleted due to server error %s.** | warning |
| 0082 | **Auth server inaccessible. Check your Internet connection and try again** | warning |
| 0109 | **This column indicates the current status of the server:
  Open : Serve** | warning |
| 0113 | **You are attempting to use an Admin account from an invalid location.** | warning |
| 0114 | **Your SecurID tokencode was incorrect, already used, or your account ha** | warning |
| 0115 | **Your SecurID tokencode was missing or invalid. Enter as <password>/<Se** | warning |
| 0116 | **SecurID authentication failed due to networking error. Contact Technic** | warning |
| 0118 | **SecurID support disabled, probable auth server error. Contact Technica** | warning |
| 0120 | **Auth server's public key is invalid. Please quit, restart, and retry w** | warning |
| 0121 | **Client/auth server incompatibility. Please quit, restart, and retry wi** | warning |
| 0125 | **Your username or password is incorrect. *CAPS LOCK* is on, remember th** | warning |
| 0127 | **The character could not be deleted.  The login servers were unable to ** | warning |
| 0131 | **
The LaunchPad server has rejected your new PIN. (Invalid format?)
Ple** | warning |
| 0134 | **Your account has been suspended by a Game Master.  Additional informat** | warning |

## Critical Errors

- **0005**: Failed to create a copy of the launcher or a critical support library for patching.  If you have a file called Matrix.exe in your game directory, please make sure it is writable.
- **0072**: The file %s could not be found, or an error occured while loading it!
The application is now shutting down.
- **0073**: The file %s could not be initialized properly.
The application is now shutting down.
- **0074**: The file %s could not be setup properly.

%s

The application is now shutting down.
- **0075**: An error occured while running %s.

%s

The application is now shutting down.

## Authentication Errors

- **0014**: Your username or password is incorrect. Please try again. If...
- **0016**: The auth servers are unavailable. Please try again later.
- **0024**: The auth servers are unavailable. Please try again later.
- **0080**: Login request not sent - bad parameters?
- **0082**: Auth server inaccessible. Check your Internet connection and...
- **0115**: Your SecurID tokencode was missing or invalid. Enter as <pas...
- **0116**: SecurID authentication failed due to networking error. Conta...
- **0118**: SecurID support disabled, probable auth server error. Contac...
- **0120**: Auth server's public key is invalid. Please quit, restart, a...
- **0121**: Client/auth server incompatibility. Please quit, restart, an...
- **0125**: Your username or password is incorrect. *CAPS LOCK* is on, r...
- **0127**: The character could not be deleted.  The login servers were ...
- **0131**: 
The LaunchPad server has rejected your new PIN. (Invalid fo...

## Server Connection Errors

- **0009**: Your client has successfully patched however the server is n...
- **0013**: Error: Could not initialize internet connection.  You must b...
- **0016**: The auth servers are unavailable. Please try again later.
- **0024**: The auth servers are unavailable. Please try again later.
- **0026**: An error has occurred while connecting to the patch download...
- **0059**: an error ocurred while connecting to the patch server
- **0081**: The character could not be deleted due to server error %s.
- **0082**: Auth server inaccessible. Check your Internet connection and...
- **0109**: This column indicates the current status of the server:
  Op...
- **0118**: SecurID support disabled, probable auth server error. Contac...
- **0120**: Auth server's public key is invalid. Please quit, restart, a...
- **0121**: Client/auth server incompatibility. Please quit, restart, an...
- **0127**: The character could not be deleted.  The login servers were ...
- **0131**: 
The LaunchPad server has rejected your new PIN. (Invalid fo...

## File System Errors

- **0005**: Failed to create a copy of the launcher or a critical suppor...
- **0006**: Could not open logfile: %s.  The file is read-only or locked...
- **0029**: Unable to delete %s from disk.  To completely erase your cha...
- **0072**: The file %s could not be found, or an error occured while lo...
- **0073**: The file %s could not be initialized properly.
The applicati...
- **0074**: The file %s could not be setup properly.

%s

The applicatio...

## Patching/Update Errors

- **0005**: Failed to create a copy of the launcher or a critical suppor...
- **0009**: Your client has successfully patched however the server is n...
- **0010**: Error: Unable to load patching system interface DLL
- **0011**: Error: Unable to load patch function from patching system in...
- **0021**: Patch system error (result code %i):

%s
- **0026**: An error has occurred while connecting to the patch download...
- **0059**: an error ocurred while connecting to the patch server
