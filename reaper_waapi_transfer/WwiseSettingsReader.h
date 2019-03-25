#pragma once

//reads Wwise user preferences file, 
//function returns whether or not it could open the file
//waapienabled and waapiport will be -1 if they couldn't be found in settings

enum class WaapiSetting
{
	Unknown,
	Disabled,
	Enabled
};

bool GetWaapiSettings(WaapiSetting &waapiEnabled, int &waapiPort);