/**
 * Matrix Online Launcher Configuration
 * 
 * Central configuration header for the launcher.
 */

#ifndef CONFIG_H
#define CONFIG_H

// Application settings
#define APP_NAME "Matrix Online Launcher"
#define APP_VERSION_MAJOR 1
#define APP_VERSION_MINOR 0
#define APP_VERSION_PATCH 0

// Window settings
#define WINDOW_WIDTH  400
#define WINDOW_HEIGHT 300
#define WINDOW_X      CW_USEDEFAULT
#define WINDOW_Y      CW_USEDEFAULT

// Paths
#define ASSETS_DIR    "assets/"
#define LOGO_PATH     ASSETS_DIR "logo.png"
#define SOUND_PATH    ASSETS_DIR "startup.wav"

// Launch settings
#define AUTO_START    true
#define FULLSCREEN    false
#define MINIMIZED     false

// Matrix Online integration (placeholder)
#define MO_HOST       "mo.matrixonline.com"
#define MO_PORT       80
#define MO_TIMEOUT    30000

// Feature flags
#define FEATURE_SOUND true
#define FEATURE_LOGO  true
#define FEATURE_STATUS true

#endif // CONFIG_H