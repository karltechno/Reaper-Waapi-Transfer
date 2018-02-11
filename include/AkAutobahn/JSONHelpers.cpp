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

#include "JSONHelpers.h"

#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

#include <string>

namespace AK
{
	namespace WwiseAuthoringAPI
	{
		namespace JSONHelpers
		{
			class StringToValue
			{
			public:

				static rapidjson::Value Convert(const std::string& in_val, rapidjson::MemoryPoolAllocator<>& in_allocator)
				{
					return rapidjson::Value(in_val.c_str(), static_cast<rapidjson::SizeType>(in_val.size()), in_allocator);
				}
			};

			bool ToRapidJson(const AkJson& in_node, rapidjson::Value& out_rapidJson, rapidjson::MemoryPoolAllocator<>& in_allocator)
			{
				return AkJson::ToRapidJson<rapidjson::Value, rapidjson::MemoryPoolAllocator<>, rapidjson::SizeType, StringToValue>(in_node, out_rapidJson, in_allocator);
			}

			bool FromRapidJson(const rapidjson::Value& in_rapidJson, AkJson& out_node)
			{
				return AkJson::FromRapidJson<rapidjson::Value>(in_rapidJson, out_node);
			}

			std::string GetJsonText(rapidjson::Document& in_doc)
			{
				rapidjson::StringBuffer buffer;

				buffer.Clear();

				rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
				in_doc.Accept(writer);

				return buffer.GetString();
			}

			std::string GetAkJsonString(const AkJson& json)
			{
				rapidjson::Document doc;
				ToRapidJson(json, doc, doc.GetAllocator());
				return JSONHelpers::GetJsonText(doc);
			}
		}
	}
}
