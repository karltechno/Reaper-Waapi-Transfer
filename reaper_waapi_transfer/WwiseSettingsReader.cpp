#include <string>
#include <memory>

#include <ShlObj.h>

#include "rapidxml/rapidxml.hpp"
#include "rapidxml/rapidxml_utils.hpp"

#include "WwiseSettingsReader.h"
#include "config.h"
#include "WAAPIHelpers.h"

bool GetWaapiSettings(WaapiSetting &waapiEnabled, int &waapiPort)
{
    fs::path wwiseSettingsPath;

	// Note: with new versions of Wwise WAAPI is enabled by default - so we won't error anymore.
	waapiEnabled = WaapiSetting::Unknown;
	waapiPort = -1;

#ifdef _WIN32
    PWSTR appDataPath;
    if (SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_DEFAULT, NULL, &appDataPath) != S_OK)
    {
        return false;
    }
    wwiseSettingsPath = appDataPath;
    CoTaskMemFree(appDataPath);
    wwiseSettingsPath.append("Audiokinetic/Wwise/Wwise.wsettings");

    //check if wwise settings file is valid
    if (!fs::is_regular_file(wwiseSettingsPath))
    {
        return false;
    }

#else
    static_assert(false, "OSX not implemented for parsing wwise settings file.");
#endif

    //rapidxml can throw
    try
    {
        rapidxml::xml_document<char> wwiseSettingsXML;

        rapidxml::file<char> xmlFile(wwiseSettingsPath.generic_string().c_str());
        wwiseSettingsXML.parse<rapidxml::parse_default>(xmlFile.data());

        bool foundPropertyList = false;
        auto *propertyListNode = wwiseSettingsXML.first_node();

        //get property list
        while (propertyListNode)
        {
            if (!strcmp(propertyListNode->name(), "PropertyList"))
            {
                foundPropertyList = true;
                break;
            }
            propertyListNode = propertyListNode->first_node();
        }

        if (!foundPropertyList)
        {
            return false;
        }

        //find properties
        rapidxml::xml_node<char> *waapiEnabledNode  = nullptr;
        rapidxml::xml_node<char> *waapiPortNode     = nullptr;

        auto propertiesIter = propertyListNode->first_node();

        while (propertiesIter)
        {
            auto nameValue = propertiesIter->first_attribute("Name")->value();
            if (!strcmp(nameValue, "Waapi\\EnableWaapi"))
            {
                waapiEnabledNode = propertiesIter;
            }
            else if (!strcmp(nameValue, "Waapi\\WampPort"))
            {
                waapiPortNode = propertiesIter;
            }

            //found both
            if (waapiEnabledNode && waapiPortNode)
            {
                break;
            }

            propertiesIter = propertiesIter->next_sibling();
        }

        //set output variables
        if (waapiPortNode)
        {
            auto attrib = waapiPortNode->last_attribute("Value");
            if (attrib)
            {
                waapiPort = std::stoi(attrib->value());
            }
        }

        if (waapiEnabledNode)
        {
            auto attrib = waapiEnabledNode->last_attribute("Value");
            if (attrib)
            {
                char *val = attrib->value();
                waapiEnabled = strcmp(val, "True") == 0 ? WaapiSetting::Enabled : WaapiSetting::Disabled;
            }

        }
    }
    catch (...)
    {
        return false;
    }

    return true;
}