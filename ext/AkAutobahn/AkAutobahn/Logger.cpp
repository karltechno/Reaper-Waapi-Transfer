/*******************************************************************************
The content of this file includes portions of the AUDIOKINETIC Wwise Technology
released in source code form as part of the SDK installer package.

Commercial License Usage

Licensees holding valid commercial licenses to the AUDIOKINETIC Wwise Technology
may use this file in accordance with the end user license agreement provided
with the software or, alternatively, in accordance with the terms contained in a
written agreement between you and Audiokinetic Inc.

Copyright (c) 2020 Audiokinetic Inc.
*******************************************************************************/

#include "AK/WwiseAuthoringAPI/AkAutobahn/Logger.h"

#include <string>
#include <stdarg.h>
#include <stdio.h>

namespace AK
{
	namespace WwiseAuthoringAPI
	{
		Logger Logger::m_instance;

		void LogPrint(const char* message, ...)
		{
			va_list args;
			va_start(args, message);
			const int MAX_BUFFER = 512;
			char buffer[MAX_BUFFER];
			vsnprintf(buffer, MAX_BUFFER - 1, message, args);
			va_end(args);

			WwiseAuthoringAPI::Logger::Get()->LogMessage("AkAutobahn", buffer);
		}
	}
}
