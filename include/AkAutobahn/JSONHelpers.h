/*******************************************************************************
The content of this file includes portions of the AUDIOKINETIC Wwise Technology
released in source code form as part of the SDK installer package.

Commercial License Usage

Licensees holding valid commercial licenses to the AUDIOKINETIC Wwise Technology
may use this file in accordance with the end user license agreement provided
with the software or, alternatively, in accordance with the terms contained in a
written agreement between you and Audiokinetic Inc.

Version: v2017.1.0  Build: 6302
Copyright (c) 2006-2017 Audiokinetic Inc.
*******************************************************************************/

#pragma once

#include "AkVariant.h"
#include "AkJson.h"

#include "rapidjson/document.h"

namespace AK
{
	namespace WwiseAuthoringAPI
	{
		namespace JSONHelpers
		{
			bool ToRapidJson(const AkJson& in_node, rapidjson::Value& out_rapidJson, rapidjson::MemoryPoolAllocator<>& in_allocator);
			bool FromRapidJson(const rapidjson::Value& in_rapidJson, AkJson& out_node);

			std::string GetJsonText(rapidjson::Document& doc);
			std::string GetAkJsonString(const AkJson& json);
		}
	}
}
