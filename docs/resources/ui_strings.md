# UI Messages Catalog - Matrix Online Launcher

## Overview
This document catalogs all user interface messages found in the Matrix Online launcher.

## UI Message Reference

| ID | UI Message | Type |
|----|------------|------|
| 0002 | ZionOS ccc9 (release 2.4.20-13.7smp #1 SMP) | label |
| 0004 | To enter The Matrix Online, please run the game from the sho | label |
| 0005 | Failed to create a copy of the launcher or a critical suppor | label |
| 0006 | Could not open logfile: %s.  The file is read-only or locked | label |
| 0007 | Cannot create unique event.  GetLastError() returned %d.  Th | label |
| 0008 | Deleted characters cannot be recovered.  Are you certain you | label |
| 0009 | Your client has successfully patched however the server is n | label |
| 0013 | Error: Could not initialize internet connection.  You must b | label |
| 0014 | Your username or password is incorrect. Please try again. If | label |
| 0015 | Your account is already in use. If this is in error, click o | label |
| 0016 | The auth servers are unavailable. Please try again later. | label |
| 0017 | Incompatible client version. Please quit, restart, and retry | label |
| 0023 | unknown | button |
| 0024 | The auth servers are unavailable. Please try again later. | label |
| 0026 | An error has occurred while connecting to the patch download | label |
| 0027 | Your account has been suspended by a Game Master until %s. A | label |
| 0028 | Your account has been disabled. For additional account infor | label |
| 0029 | Unable to delete %s from disk.  To completely erase your cha | label |
| 0035 | Open | button |
| 0044 | Char Unknown | button |
| 0047 | End User License Agreement | button |
| 0048 | Server Name | button |
| 0051 | Login: | button |
| 0052 | Password: | button |
| 0055 | select a world instance: | button |
| 0059 | an error ocurred while connecting to the patch server | label |
| 0065 | Matrix server is version %s | button |
| 0066 | Error: Could not find version or version is invalid. Check y | label |
| 0072 | The file %s could not be found, or an error occured while lo | label |
| 0073 | The file %s could not be initialized properly.
The applicati | label |
| 0074 | The file %s could not be setup properly.

%s

The applicatio | label |
| 0075 | An error occured while running %s.

%s

The application is n | label |
| 0076 | Warning! %s did not terminate properly. | label |
| 0079 | Launcher clone could not be started.  GetLastError() returne | label |
| 0080 | Login request not sent - bad parameters? | label |
| 0081 | The character could not be deleted due to server error %s. | label |
| 0082 | Auth server inaccessible. Check your Internet connection and | label |
| 0083 | Character | button |
| 0085 | Status | button |
| 0096 | Accept End User License Agreement | label |
| 0097 | Decline End User License Agreement | label |
| 0098 | Create a new character on this server. | label |
| 0099 | Delete your character on this server. | label |
| 0100 | Select this character and server
for this play session. | label |
| 0101 | View account details in your web browser. | label |
| 0102 | Open support site in your web browser. | label |
| 0103 | Open community forums in your web browser. | label |
| 0105 | This column indicates the server names.
A PVP icon to the le | label |
| 0106 | Percentage of patch download.
Note: Total download size may  | label |
| 0108 | This column indicates your character's
name on this server. | label |
| 0109 | This column indicates the current status of the server:
  Op | label |
| 0110 | This column Indicates the current load on the server.
You ma | label |
| 0111 | Please run the game by using %s. Running %s directly or usin | label |
| 0113 | You are attempting to use an Admin account from an invalid l | label |
| 0114 | Your SecurID tokencode was incorrect, already used, or your  | label |
| 0115 | Your SecurID tokencode was missing or invalid. Enter as <pas | label |
| 0117 | SecurID authentication has timed out. Wait and try again wit | label |
| 0118 | SecurID support disabled, probable auth server error. Contac | label |
| 0119 | The client has already authenticated. Click on the Support b | label |
| 0120 | Auth server's public key is invalid. Please quit, restart, a | label |
| 0121 | Client/auth server incompatibility. Please quit, restart, an | label |
| 0123 | The character could not be deleted.  This account is already | label |
| 0124 | The character has been deleted but could not be removed from | label |
| 0125 | Your username or password is incorrect. *CAPS LOCK* is on, r | label |
| 0126 | Your Station account doesn’t include a subscription to the M | label |
| 0127 | The character could not be deleted.  The login servers were  | label |
| 0128 | 
The LaunchPad server has requested that you log in again us | label |
| 0129 | 
The LaunchPad server wants you to input a new secure PIN.
P | label |
| 0130 | 
The LaunchPad server has accepted your new PIN.
Log on agai | label |
| 0131 | 
The LaunchPad server has rejected your new PIN. (Invalid fo | label |
| 0132 | The character could not be deleted.  A deletion of this char | label |
| 0133 | The character could not be deleted.  This character is in us | label |
| 0134 | Your account has been suspended by a Game Master.  Additiona | label |

## Login Dialog Messages

- **0014**: Your username or password is incorrect. Please try again. If the error persists, please click the “Support” button below.
- **0051**: Login:
- **0052**: Password:
- **0080**: Login request not sent - bad parameters?
- **0115**: Your SecurID tokencode was missing or invalid. Enter as <password>/<SecurID tokencode> (Ex: p@ssw0rd/123456). If this persists, contact Technical Operations.
- **0125**: Your username or password is incorrect. *CAPS LOCK* is on, remember that passwords are case-sensitive.  Please try again. If the error persists, please click the “Support” button below.
- **0127**: The character could not be deleted.  The login servers were unable to validate your session.
- **0129**: 
The LaunchPad server wants you to input a new secure PIN.
Please log in again entering ONLY a new PIN in the password field.
(Do not include a passcode.)
- **0131**: 
The LaunchPad server has rejected your new PIN. (Invalid format?)
Please log in again entering ONLY a new PIN in the password field.
(Do not include a passcode.)

## Server Selection Messages

- **0009**: Your client has successfully patched however the server is not open for play or new character creation.
- **0026**: An error has occurred while connecting to the patch download server.

%s

Keep retrying?
- **0109**: This column indicates the current status of the server:
  Open : Server is currently open for play.
  Closed: Server is running, but is currently closed.
  Full: Server has reached max capacity, try back later.
  Down: Server is currently down, try back later.
  Admins Only: Only administrators are allowed in at this time.
  Banned: Your character is banned from the server.
  Char In Transit: Your character is being transfered to another server
     and is currently locked.
  Char Incomplete: Your character is in an incomplete state, you may
     delete and re-create.
- **0120**: Auth server's public key is invalid. Please quit, restart, and retry with "Full System Check." If problem persists, click on the Support button.
- **0121**: Client/auth server incompatibility. Please quit, restart, and retry with "Full System Check." DevTeam, you may be connecting to the wrong internal authentication server.

## Character Management Messages

- **0008**: Deleted characters cannot be recovered.  Are you certain you...
- **0009**: Your client has successfully patched however the server is n...
- **0029**: Unable to delete %s from disk.  To completely erase your cha...
- **0081**: The character could not be deleted due to server error %s.
- **0083**: Character
- **0098**: Create a new character on this server.
- **0099**: Delete your character on this server.
- **0100**: Select this character and server
for this play session.
- **0108**: This column indicates your character's
name on this server.
- **0109**: This column indicates the current status of the server:
  Op...
- **0123**: The character could not be deleted.  This account is already...
- **0124**: The character has been deleted but could not be removed from...
- **0127**: The character could not be deleted.  The login servers were ...
- **0132**: The character could not be deleted.  A deletion of this char...
- **0133**: The character could not be deleted.  This character is in us...

## Account Messages

- **0015**: Your account is already in use. If this is in error, click o...
- **0027**: Your account has been suspended by a Game Master until %s. A...
- **0028**: Your account has been disabled. For additional account infor...
- **0101**: View account details in your web browser.
- **0113**: You are attempting to use an Admin account from an invalid l...
- **0114**: Your SecurID tokencode was incorrect, already used, or your ...
- **0123**: The character could not be deleted.  This account is already...
- **0126**: Your Station account doesn’t include a subscription to the M...
- **0134**: Your account has been suspended by a Game Master.  Additiona...
