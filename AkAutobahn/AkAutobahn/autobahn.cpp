///////////////////////////////////////////////////////////////////////////////
//
//  Copyright (C) 2014 Tavendo GmbH
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// This file was modified by Audiokinetic inc.
///////////////////////////////////////////////////////////////////////////////

#include "AK/WwiseAuthoringAPI/AkAutobahn/autobahn.h"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

#include "JSONHelpers.h"
#include "AK/WwiseAuthoringAPI/AkAutobahn/Logger.h"

#ifdef VALIDATE_WAMP
#define WAMP_ASSERT(x, y) WampAssert((x), (y))
#else
#define WAMP_ASSERT(x, y)
#endif

namespace AK
{
	namespace WwiseAuthoringAPI
	{
#ifdef VALIDATE_WAMP
		void session::WampAssert(bool value, const char* message)
		{
			if (!value)
			{
				logMessage(message);
				AKASSERT(false);
			}
		}
#endif

		void session::createErrorMessageJson(const std::string& in_message, AkJson& out_jsonError)
		{
			out_jsonError = AkJson(AkJson::Map{
				{ "message", AkVariant(in_message) }
			});
		}
		
		void session::createNoSessionErrorJson(AkJson& out_jsonError)
		{
			createErrorMessageJson("session not joined", out_jsonError);
		}

		session::session()
		{
			m_running = false;
			m_wasDisconnected = false;
			m_disconnectHandler = nullptr;
		}

		session::~session()
		{
			stop("connection closed by destruction of session");
		}

		bool session::start(const char* in_uri, unsigned int in_port, disconnectHandler_t disconnectHandler /* = nullptr */)
		{
			std::lock_guard<std::recursive_mutex> websocketLock(m_websocketMutex);

			m_disconnectHandler = disconnectHandler;

			stop("connection closed by session reconnect");
			m_wasDisconnected = false;
			bool connectResult = false;

			m_websocket = std::make_shared<WebSocketClient>(this);
			connectResult = m_websocket->Connect(in_uri, in_port);

			if (connectResult)
			{
				m_running = true;
				m_sendThread = std::thread([this] { sendThread(); });
			}

			return connectResult;
		}

		void session::stop(const std::string& errorMessage)
		{
			std::lock_guard<std::recursive_mutex> websocketLock(m_websocketMutex);

			bool expected = true;
			if (m_running.compare_exchange_strong(expected, false))
			{
				{
					std::lock_guard<std::mutex> lock(m_sendQueueMutex);
					m_sendQueue = decltype(m_sendQueue)();
					// Wake the send thread up, so it terminates.
					m_sendEvent.notify_one();
				}

				// Stop the threads.
				if (m_sendThread.get_id() == std::this_thread::get_id())
					m_sendThread.detach();
				else if (m_sendThread.joinable())
					m_sendThread.join();

				if (m_websocket != nullptr)
				{
					m_websocket->Close();
					m_websocket = nullptr;
				}

				std::lock_guard<std::mutex> lockCalls(m_callsMutex);
				std::lock_guard<std::mutex> lockSubs(m_subreqMutex);
				std::lock_guard<std::mutex> lockUnsubs(m_unsubreqMutex);

				AkJson jsonError;
				createErrorMessageJson(errorMessage, jsonError);

				// Abort all pending requests.
				for (auto& call : m_calls)
				{
					call.second.m_res.set_value(result_t(false, jsonError));
				}

				for (auto& sub_req : m_subscribe_requests)
				{
					sub_req.second.m_res.set_value(subscription(jsonError));
				}

				for (auto& unsub_req : m_unsubscribe_requests)
				{
					unsub_req.second.m_res.set_value(result_t(false, jsonError));
				}

				m_calls.clear();
				m_subscribe_requests.clear();
				m_unsubscribe_requests.clear();

				m_session_id = 0;
			}
		}

		bool session::isConnected() const
		{
			return m_running && !m_wasDisconnected;
		}

		std::future<uint64_t> session::join(const std::string& realm, const std::string& method, const std::string& authid,
			const std::string& signature)
		{
			// [HELLO, Realm|uri, Details|dict]

			AkJson akJson(AkJson::Array
			{
				AkVariant(static_cast<int>(msg_code::HELLO)),
				AkVariant(realm)
			});

			AkJson akRoles(AkJson::Map
			{
				{ "caller", AkJson(AkJson::Type::Map) },
				{ "callee", AkJson(AkJson::Type::Map) },
				{ "publisher", AkJson(AkJson::Type::Map) },
				{ "subscriber", AkJson(AkJson::Type::Map) },
			});

			AkJson akDetails(AkJson::Map
			{
				{ "roles", akRoles }
			});

			if (method != "")
			{
				akDetails.GetMap()["authmethods"] = AkJson(AkJson::Array
				{
					AkVariant(method)
				});

				akDetails.GetMap()["authid"] = AkVariant(authid);

				m_signature = signature;
			}

			akJson.GetArray().push_back(akDetails);
			send(akJson);

			std::lock_guard<std::mutex> lock(m_joinMutex);

			m_session_join = decltype(m_session_join)();
			return m_session_join.get_future();
		}
		
		authinfo session::getAuthInfo() const
		{
			return m_authinfo;
		}
		
		bool session::subscribe(const std::string& topic, handler_t handler, const AkJson& options, std::future<subscription>& out_future, AkJson& out_jsonError)
		{
			// [SUBSCRIBE, Request|id, Options|dict, Topic|uri]

			if (!m_session_id)
			{
				createNoSessionErrorJson(out_jsonError);
				return false;
			}

			std::lock_guard<std::mutex> lock(m_subreqMutex);

			m_request_id += 1;
			m_subscribe_requests.insert(std::make_pair(m_request_id, subscribe_request_t(handler)));

			AkJson jsonPayload(AkJson::Array
			{
				AkJson(AkVariant(static_cast<int>(msg_code::SUBSCRIBE))),
				AkJson(AkVariant(m_request_id)),
				AkJson(options),
				AkJson(AkVariant(topic))
			});

			send(jsonPayload);

			out_future = m_subscribe_requests[m_request_id].m_res.get_future();
			return true;
		}
		
		bool session::unsubscribe(uint64_t subscription_id, std::future<result_t>& out_future, AkJson& out_jsonError)
		{
			// [UNSUBSCRIBE, Request|id, SUBSCRIBED.Subscription|id]

			if (!m_session_id)
			{
				createNoSessionErrorJson(out_jsonError);
				return false;
			}

			std::lock_guard<std::mutex> lock(m_unsubreqMutex);

			++m_request_id;
			m_unsubscribe_requests.insert(std::make_pair(m_request_id, unsubscribe_request_t()));

			AkJson jsonPayload(AkJson::Array
			{
				AkVariant(static_cast<int>(msg_code::UNSUBSCRIBE)),
				AkVariant(m_request_id),
				AkVariant(subscription_id)
			});

			send(jsonPayload);

			out_future = m_unsubscribe_requests[m_request_id].m_res.get_future();
			return true;
		}

		bool session::call_options(
			const std::string& procedure,
			const anyvec& args,
			const AkJson& kwargs,
			const AkJson& options,
			std::future<result_t>& out_future,
			AkJson& out_jsonError)
		{
			if (!m_session_id)
			{
				createNoSessionErrorJson(out_jsonError);
				return false;
			}

			WAMP_ASSERT((
				kwargs.IsEmpty() ||
				kwargs.IsMap()),
				"Provided arguments are not valid.");
			
			if (!kwargs.IsMap())
			{
				createErrorMessageJson("Expected an object in kwargs", out_jsonError);
				return false;
			}

			std::lock_guard<std::mutex> lock(m_callsMutex);

			m_request_id += 1;
			m_calls.insert(std::make_pair(m_request_id, call_t()));

			// [CALL, Request|id, Options|dict, Procedure|uri, Arguments|list, ArgumentsKw|dict]

			AkJson jsonArgs(AkJson::Type::Array);

			for (auto value : args)
			{
				jsonArgs.GetArray().push_back(AkVariant(value));
			}

			AkJson jsonPayload(AkJson::Array
			{
				AkVariant(static_cast<int>(msg_code::CALL)),
				AkVariant(m_request_id),
				options,
				AkVariant(procedure),
				jsonArgs,
				kwargs
			});

			send(jsonPayload);

			out_future = m_calls[m_request_id].m_res.get_future();
			return true;
		}

		void session::process_welcome(const wamp_msg_t& msg)
		{
			// [WELCOME, Session|id, Details|dict]

			WAMP_ASSERT((
				msg.IsArray() &&
				msg.GetArray().size() == 3 &&
				msg[1].IsVariant() &&
				msg[1].GetVariant().IsNumber() &&
				msg[2].IsMap()),
				"WELCOME message is not valid");

			m_session_id = msg[1].GetVariant();

			AkJson details = msg[2];

			if (details.HasKey("authmethod"))
			{
				WAMP_ASSERT(details["authmethod"].IsVariant(), "authmethod is invalid");
				m_authinfo.authmethod = details["authmethod"].GetVariant().GetString();
			}

			if (details.HasKey("authprovider"))
			{
				WAMP_ASSERT(details["authprovider"].IsVariant(), "authprovider is invalid");
				m_authinfo.authprovider = details["authprovider"].GetVariant().GetString();
			}

			if (details.HasKey("authid"))
			{
				WAMP_ASSERT(details["authid"].IsVariant(), "authid is invalid");
				m_authinfo.authid = details["authid"].GetVariant().GetString();
			}

			if (details.HasKey("authrole"))
			{
				WAMP_ASSERT(details["authrole"].IsVariant(), "authrole is invalid");
				m_authinfo.authrole = details["authrole"].GetVariant().GetString();
			}

			std::lock_guard<std::mutex> lock(m_joinMutex);

			m_session_join.set_value(m_session_id);
		}
		
		void session::process_challenge(const wamp_msg_t& msg)
		{
			WAMP_ASSERT(false, "unable to respond to auth method");
		}

		void session::process_error(const wamp_msg_t& msg)
		{
			// [ERROR, REQUEST.Type|int, REQUEST.Request|id, Details|dict, Error|uri]
			// [ERROR, REQUEST.Type|int, REQUEST.Request|id, Details|dict, Error|uri, Arguments|list]
			// [ERROR, REQUEST.Type|int, REQUEST.Request|id, Details|dict, Error|uri, Arguments|list, ArgumentsKw|dict]

			WAMP_ASSERT((
				msg.IsArray() &&
				msg.GetArray().size() >= 5 &&
				msg[1].IsVariant() &&
				msg[2].IsVariant() &&
				msg[4].IsVariant()
				), "ERROR wamp message is invalid");

			AkJson errorJson(AkJson::Type::Map);

			if (msg.GetArray().size() > 5)
			{
				errorJson = msg[6];
			}

			errorJson.GetMap()["uri"] = msg[4].GetVariant();

			switch (static_cast<msg_code>(static_cast<int64_t>(msg[1].GetVariant())))
			{
			case msg_code::SUBSCRIBE:
			{
				std::lock_guard<std::mutex> lock(m_subreqMutex);
				auto subscribe_request = m_subscribe_requests.find(msg[2].GetVariant());
				if (subscribe_request != m_subscribe_requests.end())
				{
					subscribe_request->second.m_res.set_value(subscription(errorJson));
					m_subscribe_requests.erase(subscribe_request);
				}
			}
			break;
			case msg_code::UNSUBSCRIBE:
			{
				std::lock_guard<std::mutex> lock(m_unsubreqMutex);
				auto unsubscribe_request = m_unsubscribe_requests.find(msg[2].GetVariant());
				if (unsubscribe_request != m_unsubscribe_requests.end())
				{
					unsubscribe_request->second.m_res.set_value(result_t(false, errorJson));
					m_unsubscribe_requests.erase(unsubscribe_request);
				}
			}
			break;
			case msg_code::CALL:
			{
				std::lock_guard<std::mutex> lock(m_callsMutex);
				auto call_req = m_calls.find(msg[2].GetVariant());
				if (call_req != m_calls.end())
				{
					call_req->second.m_res.set_value(result_t(false, errorJson));
					m_calls.erase(call_req);
				}
			}
			break;

			default:
				WAMP_ASSERT(false, "ERROR not handled");
			}
		}

		void session::process_goodbye(const wamp_msg_t& msg)
		{
			if (!m_session_id)
			{
				WAMP_ASSERT(false, "GOODBYE received an no session established");
				return;
			}
			
			WAMP_ASSERT((
				msg.IsArray() &&
				msg.GetArray().size() == 3 &&
				msg[2].IsVariant() &&
				msg[2].GetVariant().IsString()
				), "GOODBYE wamp message is invalid");

			m_session_id = 0;

			if (!m_goodbye_sent)
			{
				// if we did not initiate closing, reply ..

				// [GOODBYE, Details|dict, Reason|uri]

				AkJson json(AkJson::Array
				{
					AkVariant(static_cast<int>(msg_code::GOODBYE)),
					AkJson(AkJson::Type::Map),
					AkVariant("wamp.error.goodbye_and_out")
				});

				send(json);
			}
			else
			{
				// we previously initiated closing, so this
				// is the peer reply
			}
		}
		
		void session::process_call_result(const wamp_msg_t& msg)
		{
			// [RESULT, CALL.Request|id, Details|dict]
			// [RESULT, CALL.Request|id, Details|dict, YIELD.Arguments|list]
			// [RESULT, CALL.Request|id, Details|dict, YIELD.Arguments|list, YIELD.ArgumentsKw|dict]

			WAMP_ASSERT((
				msg.IsArray() &&
				msg.GetArray().size() >= 3 &&
				msg[1].IsVariant()
				), "RESULT wamp message is invalid");

			uint64_t request_id = msg[1].GetVariant();

			std::lock_guard<std::mutex> lock(m_callsMutex);

			calls_t::iterator call = m_calls.find(request_id);
			
			if (call != m_calls.end())
			{
				if (msg.GetArray().size() > 4)
				{
					auto args = msg[4];
					WAMP_ASSERT(args.IsMap(), "RESULT wamp message is invalid - ArgumentsKw");
					call->second.m_res.set_value(result_t(true, args));
				}
				else
				{
					// empty result
					call->second.m_res.set_value(result_t(true, AkJson(AkJson::Type::Map)));
				}

				m_calls.erase(call);
			}
			else
			{
				WAMP_ASSERT(false, "bogus RESULT message for non-pending request ID");
			}
		}

		void session::process_subscribed(const wamp_msg_t& msg)
		{
			// [SUBSCRIBED, SUBSCRIBE.Request|id, Subscription|id]

			WAMP_ASSERT((
				msg.IsArray() &&
				msg.GetArray().size() == 3 &&
				msg[1].IsVariant() &&
				msg[2].IsVariant()
				), "SUBSCRIBED wamp message is invalid");

			uint64_t request_id = msg[1].GetVariant();

			std::lock_guard<std::mutex> lock(m_subreqMutex);

			subscribe_requests_t::iterator subscribe_request = m_subscribe_requests.find(request_id);

			if (subscribe_request != m_subscribe_requests.end())
			{
				uint64_t subscription_id = msg[2].GetVariant();

				{
					std::lock_guard<std::mutex> lock(m_handlersMutex);
					m_handlers[subscription_id] = subscribe_request->second.m_handler;
				}

				subscribe_request->second.m_res.set_value(subscription(subscription_id));

				m_subscribe_requests.erase(subscribe_request);
			}
			else
			{
				WAMP_ASSERT(false, "bogus SUBSCRIBED message for non-pending request ID");
			}
		}

		void session::process_unsubscribed(const wamp_msg_t& msg)
		{
			// [UNSUBSCRIBED, UNSUBSCRIBE.Request|id]

			WAMP_ASSERT((
				msg.IsArray() &&
				msg.GetArray().size() == 2 &&
				msg[1].IsVariant()
				), "UNSUBSCRIBED wamp message is invalid");

			uint64_t request_id = msg[1].GetVariant();

			std::lock_guard<std::mutex> lock(m_unsubreqMutex);

			auto unsubscribe_request = m_unsubscribe_requests.find(request_id);

			if (unsubscribe_request != m_unsubscribe_requests.end())
			{
				unsubscribe_request->second.m_res.set_value(result_t(true, AkJson(AkJson::Type::Map)));
				m_unsubscribe_requests.erase(unsubscribe_request);
			}
			else
			{
				WAMP_ASSERT(false, "bogus UNSUBSCRIBED message for non-pending request ID");
			}
		}
		
		void session::process_event(const wamp_msg_t& msg)
		{
			// [EVENT, SUBSCRIBED.Subscription|id, PUBLISHED.Publication|id, Details|dict]
			// [EVENT, SUBSCRIBED.Subscription|id, PUBLISHED.Publication|id, Details|dict, PUBLISH.Arguments|list]
			// [EVENT, SUBSCRIBED.Subscription|id, PUBLISHED.Publication|id, Details|dict, PUBLISH.Arguments|list,
			// PUBLISH.ArgumentsKw|dict]

			WAMP_ASSERT((
				msg.IsArray() &&
				msg.GetArray().size() >= 4 &&
				msg[1].IsVariant()
				), "EVENT wamp message is invalid");

			uint64_t subscription_id = msg[1].GetVariant();

			std::lock_guard<std::mutex> lock(m_handlersMutex);
			handlers_t::iterator handler = m_handlers.find(subscription_id);
			if (handler != m_handlers.end())
			{
				anyvec args;
				AkJson kwargs;

				if (msg.GetArray().size() > 4)
				{
					WAMP_ASSERT(msg[4].IsArray(), "EVENT wamp message - Arguments is not an array");

					for (auto value : msg[4].GetArray())
					{
						args.push_back(value.GetVariant());
					}

					if (msg.GetArray().size() > 5)
					{
						kwargs = msg[5];
					}
				}

				(handler->second)(subscription_id, JsonProvider(kwargs));
			}
			else
			{
				// silently swallow EVENT for non-existent subscription IDs.
				// We may have just unsubscribed, the this EVENT might be have
				// already been in-flight.
				logMessage("Skipping EVENT for non-existent subscription ID");
			}
		}

		void session::got_msg(const std::string& jsonPayload)
		{
			wamp_msg_t msg;
			rapidjson::Document doc;

			// Disregard ParseResult other content and directly cast, we just want to know if it worked at all.
			bool hasErrors = doc.Parse(jsonPayload.c_str()).HasParseError();

			WAMP_ASSERT((
				!hasErrors
				), "WebSocket received payload is not a valid JSON");

			bool fromRapidJsonResult = JSONHelpers::FromRapidJson(doc, msg);

			WAMP_ASSERT((
				fromRapidJsonResult
				), "WebSocket received JSON payload contains invalid data");

			WAMP_ASSERT((
				msg.IsArray() &&
				msg[0].IsVariant() &&
				msg[0].GetVariant().IsNumber()
				), "JSON payload contains invalid WAMP message format");

			msg_code code = static_cast<msg_code>(static_cast<int64_t>(msg[0].GetVariant()));

			switch (code)
			{
			case msg_code::HELLO:
				WAMP_ASSERT(false, "received HELLO message unexpected for WAMP client roles");
				break;

			case msg_code::WELCOME:
				process_welcome(msg);
				break;

			case msg_code::ABORT:
				break;

			case msg_code::CHALLENGE:
				process_challenge(msg);
				break;

			case msg_code::AUTHENTICATE:
				WAMP_ASSERT(false, "received AUTHENTICATE message unexpected for WAMP client roles");
				break;

			case msg_code::GOODBYE:
				process_goodbye(msg);
				break;

			case msg_code::HEARTBEAT:
				break;

			case msg_code::ERROR:
				process_error(msg);
				break;

			case msg_code::PUBLISH:
				WAMP_ASSERT(false, "received PUBLISH message unexpected for WAMP client roles");
				break;

			case msg_code::PUBLISHED:
				break;

			case msg_code::SUBSCRIBE:
				WAMP_ASSERT(false, "received SUBSCRIBE message unexpected for WAMP client roles");
				break;

			case msg_code::SUBSCRIBED:
				process_subscribed(msg);
				break;

			case msg_code::UNSUBSCRIBE:
				WAMP_ASSERT(false, "received UNSUBSCRIBE message unexpected for WAMP client roles");
				break;

			case msg_code::UNSUBSCRIBED:
				process_unsubscribed(msg);
				break;

			case msg_code::EVENT:
				process_event(msg);
				break;

			case msg_code::CALL:
				WAMP_ASSERT(false, "received CALL message unexpected for WAMP client roles");
				break;

			case msg_code::CANCEL:
				WAMP_ASSERT(false, "received CANCEL message unexpected for WAMP client roles");
				break;

			case msg_code::RESULT:
				process_call_result(msg);
				break;

			case msg_code::REGISTER:
				WAMP_ASSERT(false, "received REGISTER message unexpected for WAMP client roles");
				break;

			case msg_code::WAMP_REGISTERED:
				break;
				
			case msg_code::UNREGISTER:
				WAMP_ASSERT(false, "received UNREGISTER message unexpected for WAMP client roles");
				break;

			case msg_code::UNREGISTERED:
				break;

			case msg_code::INVOCATION:
				break;

			case msg_code::INTERRUPT:
				break;

			case msg_code::YIELD:
				WAMP_ASSERT(false, "received YIELD message unexpected for WAMP client roles");
				break;
			}
		}

		void session::send(const AkJson& jsonPayload)
		{
			send(JSONHelpers::GetAkJsonString(jsonPayload).c_str());
		}

		void session::send(std::string s)
		{
			auto sendBuffer = std::make_shared<std::vector<char>>(s.c_str(), s.c_str() + s.length() + 1);

			std::lock_guard<std::mutex> lock(m_sendQueueMutex);
			m_sendQueue.push(sendBuffer);
			m_sendEvent.notify_one();
		}
		
		void session::OnMessage(std::string&& message)
		{
			got_msg(message);
		}

		void session::OnConnectionLost()
		{
			logMessage("WebSocket connection was lost, the current WAMP session is now invalid and must be re-established.");

			// Need to set a state here and defer the call to stop() until the next connect,
			// otherwise we run into a random dead-lock which has been investigated for way too long.
			// This is the safest way to fix the issue since we enforce the call to stop()
			// from the same thread as the call to start().
			m_wasDisconnected = true;
			
			if (m_disconnectHandler)
			{
				m_disconnectHandler();
			}
		}

		void session::logMessage(const char* logContent)
		{
			Logger::Get()->LogMessage("AkAutobahn", logContent);
		}

		void session::sendThread()
		{
			while (m_running && m_websocket)
			{
				std::unique_lock<std::mutex> lock(m_sendQueueMutex);
				m_sendEvent.wait(lock);

				while (m_running && m_websocket && m_sendQueue.size())
				{
					auto sendBuffer = m_sendQueue.front();
					lock.unlock();

					std::string errorMessage;
					bool result = false;
					{
						std::lock_guard<std::recursive_mutex> websocketLock(m_websocketMutex);
						
						if (!m_websocket)
						{
							return;
						}
						if (!m_websocket->SendUTF8(std::string(sendBuffer->data()), errorMessage))
						{
							stop(errorMessage);
							return;
						}
					}

					lock.lock();
					// Queue could have been cleared by another thread.
					if (m_sendQueue.size())
					{
						m_sendQueue.pop();
					}
				}
			}
		}
	}
}
