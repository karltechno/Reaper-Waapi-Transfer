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

#include "AK/WwiseAuthoringAPI/AkAutobahn/Client.h"

#include <iostream>
#include <string>
#include <sstream>

#include "JSONHelpers.h"
#include "AK/WwiseAuthoringAPI/AkAutobahn/Logger.h"

namespace AK
{
	namespace WwiseAuthoringAPI
	{
		Client::Client()
		{
			m_ws = new session();
		}

		Client::~Client()
		{
			delete m_ws;

			// IMPORTANT: In theory, deleting the websocket will hang the thread until the receiver thread has exited,
			// thus giving the garantee that the map "m_subscribers" can then be safely disposed of when the Client
			// object is fully destructed.
		}

		void Client::Log(const char* log)
		{
			Logger::Get()->LogMessage("AkAutobahn", log);
		}

		// This assumes "message" is present, which should be a valid assumption
		// for either error json constructed client-side or returned from WAAPI server.
		void Client::LogErrorMessageFromJson(const AkJson& in_json)
		{
			Log(in_json["message"].GetVariant().GetString().c_str());
		}
		
		bool Client::Connect(const char* in_uri, unsigned int in_port, disconnectHandler_t disconnectHandler /* = nullptr */, int in_timeoutMs /* = -1 */)
		{
			if (!m_ws->start(in_uri, in_port, disconnectHandler))
			{
				return false;
			}

			auto future = m_ws->join("realm1");
			uint64_t result;
			return GetFuture<uint64_t>(future, in_timeoutMs, result);
		}
		
        bool Client::IsConnected() const
        {
			if (m_ws != nullptr)
			{
				return m_ws->isConnected();
			}

            return false;
        }

		void Client::Disconnect()
		{
			m_ws->stop("connection closed by destruction of session");
		}

		void CreateErrorMessageFailedFuture(AkJson& out_jsonError)
		{
			session::createErrorMessageJson("Failed to receive WAAPI message in time.", out_jsonError);
		}
		
		bool Client::Call(const char* in_uri, const char* in_args, const char* in_options, std::string& out_result, int in_timeoutMs)
		{
			AkJson jsonArgs;
			rapidjson::Document docArgs;

			if (docArgs.Parse(in_args).HasParseError() || !JSONHelpers::FromRapidJson(docArgs, jsonArgs))
			{
				AkJson jsonError;
				session::createErrorMessageJson("in_args should contain a valid JSON object string (empty object is allowed).", jsonError);
				out_result = JSONHelpers::GetAkJsonString(jsonError);
				LogErrorMessageFromJson(jsonError);
				return false;
			}

			AkJson jsonOptions;
			rapidjson::Document docOptions;

			if (docOptions.Parse(in_options).HasParseError() || !JSONHelpers::FromRapidJson(docOptions, jsonOptions))
			{
				AkJson jsonError;
				session::createErrorMessageJson("in_options should contain a valid JSON object string (empty object is allowed).", jsonError);
				out_result = JSONHelpers::GetAkJsonString(jsonError);
				LogErrorMessageFromJson(jsonError);
				return false;
			}
			
			result_t result;
			std::future<result_t> future;
			AkJson jsonError;

			if (!m_ws->call_options(in_uri, std::vector<AkVariant>{}, jsonArgs, jsonOptions, future, jsonError))
			{
				out_result = JSONHelpers::GetAkJsonString(jsonError);
				LogErrorMessageFromJson(jsonError);
				return false;
			}
			
			if (!GetFuture<result_t>(future, in_timeoutMs, result))
			{
				AkJson jsonError;
				CreateErrorMessageFailedFuture(jsonError);
				out_result = JSONHelpers::GetAkJsonString(jsonError);
				LogErrorMessageFromJson(jsonError);
				return false;
			}

			out_result = JSONHelpers::GetAkJsonString(result.data);

			if (!result.success)
			{
				LogErrorMessageFromJson(result.data);
			}

			return result.success;
		}
		
		bool Client::Call(const char* in_uri, const AkJson& in_args, const AkJson& in_options, AkJson& out_result, int in_timeoutMs)
		{
			std::future<result_t> future;

			if (!m_ws->call_options(in_uri, std::vector<AkVariant>{}, in_args, in_options, future, out_result))
			{
				LogErrorMessageFromJson(out_result);
				return false;
			}
			
			result_t result;
			if (!GetFuture<result_t>(future, in_timeoutMs, result))
			{
				CreateErrorMessageFailedFuture(out_result);
				LogErrorMessageFromJson(out_result);
				return false;
			}

			out_result = result.data;

			if (!result.success)
			{
				LogErrorMessageFromJson(out_result);
			}

			return result.success;
		}

		bool Client::SubscribeImpl(const char* in_uri, const AkJson& in_options, handler_t in_callback, int in_timeoutMs, uint64_t& out_subscriptionId, AkJson& out_result)
		{
			std::future<subscription> future;

			if (!m_ws->subscribe(in_uri, in_callback, in_options, future, out_result))
			{
				return false;
			}

			subscription resultObject;
			if (!GetFuture<subscription>(future, in_timeoutMs, resultObject))
			{
				CreateErrorMessageFailedFuture(out_result);
				return false;
			}

			out_subscriptionId = resultObject.id;
			out_result = resultObject.errorJson;

			return resultObject.success;
		}
		
		bool Client::Subscribe(const char* in_uri, const char* in_options, WampEventCallback in_callback, uint64_t& out_subscriptionId, std::string& out_result, int in_timeoutMs)
		{
			rapidjson::Document doc;
			
			if (doc.Parse(in_options).HasParseError())
			{
				AkJson jsonError;
				session::createErrorMessageJson("in_options should contain a valid JSON object string (empty object is allowed).", jsonError);
				LogErrorMessageFromJson(jsonError);
				return false;
			}
			
			AkJson jsonOptions;
			JSONHelpers::FromRapidJson(doc, jsonOptions);

			AkJson jsonError;

			bool result = SubscribeImpl(in_uri, jsonOptions, in_callback, in_timeoutMs, out_subscriptionId, jsonError);

			if (!result)
			{
				out_result = JSONHelpers::GetAkJsonString(jsonError);
				LogErrorMessageFromJson(jsonError);
			}

			return result;
		}

		bool Client::Subscribe(const char* in_uri, const AkJson& in_options, WampEventCallback in_callback, uint64_t& out_subscriptionId, AkJson& out_result, int in_timeoutMs)
		{
			bool result = SubscribeImpl(in_uri, in_options, in_callback, in_timeoutMs, out_subscriptionId, out_result);

			if (!result)
			{
				LogErrorMessageFromJson(out_result);
			}

			return result;
		}
		
		bool Client::UnsubscribeImpl(const uint64_t& in_subscriptionId, int in_timeoutMs, AkJson& out_result)
		{
			std::future<result_t> future;

			if (!m_ws->unsubscribe(in_subscriptionId, future, out_result))
			{
				return false;
			}

			result_t result;
			if (!GetFuture<result_t>(future, in_timeoutMs, result))
			{
				CreateErrorMessageFailedFuture(out_result);
				return false;
			}

			out_result = result.data;
			return result.success;
		}
		
		bool Client::Unsubscribe(const uint64_t& in_subscriptionId, std::string& out_result, int in_timeoutMs)
		{
			AkJson jsonError;
			bool result = UnsubscribeImpl(in_subscriptionId, in_timeoutMs, jsonError);

			if (!result)
			{
				out_result = JSONHelpers::GetAkJsonString(jsonError);
				CreateErrorMessageFailedFuture(jsonError);
			}

			return result;
		}

		bool Client::Unsubscribe(const uint64_t& in_subscriptionId, AkJson& out_result, int in_timeoutMs)
		{
			bool result = UnsubscribeImpl(in_subscriptionId, in_timeoutMs, out_result);

			if (!result)
			{
				LogErrorMessageFromJson(out_result);
			}

			return result;
		}
	}
}
