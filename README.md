*_Please note, this is an old project which is not actively maintained. Forks/pull requests welcome._* 

# Reaper-Waapi-Transfer

Reaper extension for quickly importing rendered audio files into Wwise.


## [Download the extension here](https://github.com/karltechno/Reaper-Waapi-Transfer/releases)

## [Read the user guide](https://github.com/karltechno/Reaper-Waapi-Transfer/wiki/User-Guide)

![Reaper Waapi Transfer](http://i.imgur.com/5fqg5tO.png)

# Building: 
The project can be built with CMake. It currently relies on AkAutobahn from the Wwise SDK. For simplicity there is a python script to set everything up for you.
1. Install the latest Wwise SDK, CMake and Python.
2. Run 'configure_project.py' By default this will create a Visual Studio 2017 x64 solution under Build/. Run 'configure_project.py -h' to see available options. Adding the reaper path is useful (eg: 'configure_project.py -reaper64_dir "C:\Program Files\REAPER (x64)"') as this will automatically copy the DLL to your plugins folder as well as configure Reaper to open in the debugger. To reconfigure simply delete the build directory and re-run the python script.
3. (Optional) Replace reaper_plugin_functions.h with a version for your reaper install by running **[developer] Write C++ API functions header** action from your Reaper installation.
