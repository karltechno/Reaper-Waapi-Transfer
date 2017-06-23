# Reaper-Waapi-Transfer

Reaper extension for quickly importing rendered audio files into Wwise.

## [Download the extension here](https://github.com/karltechno/Reaper-Waapi-Transfer/releases)

## [Read the user guide](https://github.com/karltechno/Reaper-Waapi-Transfer/wiki/User-Guide)

![Reaper Waapi Transfer](http://i.imgur.com/5fqg5tO.png)

# Building: 
1. Install the latest Wwise SDK with WAAPI.
2. Copy AkAutobahn from the WAAPI cpp sample into include/AkAutobahn.
3. Run CMake and point AKSDK_INCLUDE_DIR to the Wwise SDK include path.
4. (Optional) Replace reaper_plugin_functions.h with a version for your reaper install by running **[developer] Write C++ API functions header** action from your Reaper installation.
