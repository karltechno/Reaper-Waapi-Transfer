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

#include "autobahn.h"

#include "util/Continuation.h"
#include "util/make_unique.h"

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
#include "Logger.h"

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
				throw protocol_error(message);
			}
		}
#endif

		session::session()
		{
			m_running = false;
		}

		session::~session()
		{
			auto eptr = std::make_exception_ptr(connection_error("connection closed by destruction of session"));
			stop(eptr);
		}

		void session::start(const char* in_uri, unsigned int in_port)
		{
			auto eptr = std::make_exception_ptr(connection_error("connection closed by session reconnect"));
			stop(eptr);

			{
				std::lock_guard<std::mutex> websocketLock(m_websocketMutex);
				m_websocket = std::make_shared<WebSocketClient>(this);
				// Will throw in case the connection could not establish.
				m_websocket->Connect(in_uri, in_port);
			}

			// TODO
			//m_ws->setReceiveTimeout(Poco::Timespan(10, 0, 0, 0, 0));

			m_running = true;

			m_sendThread = std::thread([this] { sendThread(); });
		}

		void session::stop(std::exception_ptr abortExc)
		{
			bool expected = true;
			if (m_running.compare_exchange_strong(expected, false))
			{
				{
					std::lock_guard<std::mutex> websocketLock(m_websocketMutex);
					m_websocket->Close();
					m_websocket = nullptr;
				}

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

				std::lock_guard<std::mutex> lockCalls(m_callsMutex);
				std::lock_guard<std::mutex> lockSubs(m_subreqMutex);
				std::lock_guard<std::mutex> lockRegs(m_regreqMutex);
				std::lock_guard<std::mutex> lockUnsubs(m_unsubreqMutex);

				// Abort all pending requests.
				for (auto& call : m_calls)
					call.second.m_res.set_exception(abortExc);

				for (auto& sub_req : m_subscribe_requests)
					sub_req.second.m_res.set_exception(abortExc);

				for (auto& reg_req : m_register_requests)
					reg_req.second.m_res.set_exception(abortExc);

				for (auto& unsub_req : m_unsubscribe_requests)
					unsub_req.second.m_res.set_exception(abortExc);

				m_calls.clear();
				m_subscribe_requests.clear();
				m_register_requests.clear();
				m_unsubscribe_requests.clear();

				m_session_id = 0;
			}
		}


		bool session::isConnected() const
		{
			return m_running;
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


		std::future<subscription> session::subscribe(const std::string& topic, handler_t handler, const anymap& options)
		{
			// [SUBSCRIBE, Request|id, Options|dict, Topic|uri]

			if (!m_session_id)
			{
				throw no_session_error();
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

			return m_subscribe_requests[m_request_id].m_res.get_future();
		}

		std::future<anymap> session::unsubscribe(uint64_t subscription_id)
		{
			// [UNSUBSCRIBE, Request|id, SUBSCRIBED.Subscription|id]

			if (!m_session_id)
			{
				throw no_session_error();
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

			return m_unsubscribe_requests[m_request_id].m_res.get_future();
		}

		void session::publish(const std::string& topic, const anyvec& args, const anymap& kwargs, const publish_options& options)
		{
			// [PUBLISH, Request|id, Options|dict, Topic|uri, Arguments|list, ArgumentsKw|dict]

			if (!m_session_id)
				throw no_session_error();

			AkJson json(AkJson::Array
			{
				AkVariant(static_cast<int>(msg_code::PUBLISH)),
				AkVariant(m_request_id),
				options.toDict(),
				AkVariant(topic)
			});

			if (!args.empty())
			{
				AkJson jsonArgs(AkJson::Type::Array);

				for (auto value : args)
				{
					jsonArgs.GetArray().push_back(value);
				}

				json.GetArray().push_back(jsonArgs);
			}

			if (!kwargs.GetMap().empty())
			{
				json.GetArray().push_back(kwargs);
			}

			m_request_id += 1;

			send(json);
		}


		std::future<anymap> session::call(const std::string& procedure)
		{
			return call_options(procedure, AkJson(AkJson::Type::Map));
		}


		std::future<anymap> session::call_options(const std::string& procedure, const anymap& options)
		{
			if (!m_session_id)
			{
				throw no_session_error();
			}

			std::lock_guard<std::mutex> lock(m_callsMutex);

			m_request_id += 1;
			m_calls.insert(std::make_pair(m_request_id, call_t()));

			// [CALL, Request|id, Options|dict, Procedure|uri]

			AkJson json(AkJson::Array
			{
				AkVariant(static_cast<int>(msg_code::CALL)),
				AkVariant(m_request_id),
				options,
				AkVariant(procedure)
			});

			send(json);

			return m_calls[m_request_id].m_res.get_future();
		}


		std::future<anymap> session::call(const std::string& procedure, const anyvec& args)
		{
			return call_options(procedure, args, AkJson(AkJson::Type::Map));
		}


		std::future<anymap> session::call_options(const std::string& procedure, const anyvec& args, const anymap& options)
		{
			if (!m_session_id)
			{
				throw no_session_error();
			}

			if (args.size() > 0)
			{
				std::lock_guard<std::mutex> lock(m_callsMutex);

				m_request_id += 1;
				m_calls.insert(std::make_pair(m_request_id, call_t()));

				// [CALL, Request|id, Options|dict, Procedure|uri, Arguments|list]

				AkJson jsonArgs(AkJson::Type::Array);

				for (auto value : args)
				{
					jsonArgs.GetArray().push_back(AkVariant(value));
				}

				AkJson json(AkJson::Array
				{
					AkVariant(static_cast<int>(msg_code::CALL)),
					AkVariant(m_request_id),
					options,
					AkVariant(procedure),
					jsonArgs
				});

				return m_calls[m_request_id].m_res.get_future();
			}
			else
			{
				return call(procedure);
			}
		}


		std::future<anymap> session::call(const std::string& procedure, const anyvec& args, const anymap& kwargs)
		{
			return call_options(procedure, args, kwargs, AkJson(AkJson::Type::Map));
		}


		std::future<anymap> session::call_options(const std::string& procedure, const anyvec& args, const anymap& kwargs,
			const anymap& options)
		{
			if (!m_session_id)
			{
				throw no_session_error();
			}

			WAMP_ASSERT((
				kwargs.IsEmpty() ||
				kwargs.IsMap()),
				"Provided arguments are not valid.");
			
			if (kwargs.IsMap())
			{
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

				return m_calls[m_request_id].m_res.get_future();
			}
			else
			{
				return call(procedure, args);
			}
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


		void session::process_abort(const wamp_msg_t& msg)
		{
			// [ABORT, Details|dict, Reason|uri]

			WAMP_ASSERT((
				msg.IsArray() &&
				msg.GetArray().size() == 3 &&
				msg[2].IsVariant()
				), "ABORT wamp message is invalid");

			std::lock_guard<std::mutex> lock(m_joinMutex);

			auto eptr = std::make_exception_ptr(server_error(msg[2].GetVariant().GetString()));
			m_session_join.set_exception(eptr);
		}


		void session::process_challenge(const wamp_msg_t& msg)
		{
			throw protocol_error("unable to respond to auth method");
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

			auto eptr = std::make_exception_ptr(server_error(JSONHelpers::GetAkJsonString(errorJson)));

			switch (static_cast<msg_code>(static_cast<int64_t>(msg[1].GetVariant())))
			{
			case msg_code::REGISTER:
			{
				std::lock_guard<std::mutex> lock(m_regreqMutex);
				auto register_request = m_register_requests.find(msg[2].GetVariant());
				if (register_request != m_register_requests.end())
				{
					register_request->second.m_res.set_exception(eptr);
					m_register_requests.erase(register_request);
				}
			}
			break;
			case msg_code::SUBSCRIBE:
			{
				std::lock_guard<std::mutex> lock(m_subreqMutex);
				auto subscribe_request = m_subscribe_requests.find(msg[2].GetVariant());
				if (subscribe_request != m_subscribe_requests.end())
				{
					subscribe_request->second.m_res.set_exception(eptr);
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
					unsubscribe_request->second.m_res.set_exception(eptr);
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
					call_req->second.m_res.set_exception(eptr);
					m_calls.erase(call_req);
				}
			}
			break;

			// TODO: INVOCATION, UNREGISTER, PUBLISH
			logMessage("ERROR not handled");
			}
		}


		void session::process_goodbye(const wamp_msg_t& msg)
		{
			if (!m_session_id) {
				throw protocol_error("GOODBYE received an no session established");
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
			std::string reason = msg[2].GetVariant().GetString();
			m_session_leave.set_value(reason);
		}


		std::future<std::string> session::leave(const std::string& reason)
		{
			if (!m_session_id)
			{
				throw no_session_error();
			}

			m_goodbye_sent = true;
			m_session_id = 0;

			// [GOODBYE, Details|dict, Reason|uri]

			AkJson json(AkJson::Array
			{
				AkVariant(static_cast<int>(msg_code::GOODBYE)),
				AkJson(AkJson::Type::Map),
				AkVariant(reason)
			});

			send(json);

			m_session_leave = decltype(m_session_leave)();
			return m_session_leave.get_future();
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
					call->second.m_res.set_value(args);
				}
				else
				{
					// empty result
					call->second.m_res.set_value(AkJson(AkJson::Type::Map));
				}

				m_calls.erase(call);
			}
			else
			{
				throw protocol_error("bogus RESULT message for non-pending request ID");
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
				throw protocol_error("bogus SUBSCRIBED message for non-pending request ID");
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
				// TODO: Is an empty object the proper return value?
				unsubscribe_request->second.m_res.set_value(AkJson(AkJson::Type::Map));
				m_unsubscribe_requests.erase(unsubscribe_request);
			}
			else
			{
				throw protocol_error("bogus UNSUBSCRIBED message for non-pending request ID");
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
				anymap kwargs;

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

				try
				{
					(handler->second)(subscription_id, JsonProvider(kwargs));
				}
				catch (...)
				{
					logMessage("event handler fired exception");
				}
			}
			else
			{
				// silently swallow EVENT for non-existent subscription IDs.
				// We may have just unsubscribed, the this EVENT might be have
				// already been in-flight.
				logMessage("Skipping EVENT for non-existent subscription ID");
			}
		}


		void session::process_registered(const wamp_msg_t& msg)
		{
			// [REGISTERED, REGISTER.Request|id, Registration|id]

			WAMP_ASSERT((
				msg.IsArray() &&
				msg.GetArray().size() == 3 &&
				msg[1].IsVariant() &&
				msg[1].GetVariant().IsNumber()
				), "REGISTERED wamp message is invalid");

			uint64_t request_id = msg[1].GetVariant();

			std::lock_guard<std::mutex> lock(m_regreqMutex);

			register_requests_t::iterator register_request = m_register_requests.find(request_id);

			if (register_request != m_register_requests.end())
			{
				WAMP_ASSERT((msg[2].IsVariant() && msg[2].GetVariant().IsNumber()), "REGISTERED wamp message - Request id is not a number");
				uint64_t registration_id = msg[2].GetVariant();

				m_endpoints[registration_id] = register_request->second.m_endpoint;

				register_request->second.m_res.set_value(registration(registration_id));

				m_register_requests.erase(register_request);
			}
			else
			{
				throw protocol_error("bogus REGISTERED message for non-pending request ID");
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
				throw protocol_error("received HELLO message unexpected for WAMP client roles");

			case msg_code::WELCOME:
				process_welcome(msg);
				break;

			case msg_code::ABORT:
				process_abort(msg);
				break;

			case msg_code::CHALLENGE:
				process_challenge(msg);
				break;

			case msg_code::AUTHENTICATE:
				throw protocol_error("received AUTHENTICATE message unexpected for WAMP client roles");

			case msg_code::GOODBYE:
				process_goodbye(msg);
				break;

			case msg_code::HEARTBEAT:
				throw protocol_error("received HEARTBEAT message - not implemented");
				break;

			case msg_code::ERROR:
				process_error(msg);
				break;

			case msg_code::PUBLISH:
				throw protocol_error("received PUBLISH message unexpected for WAMP client roles");

			case msg_code::PUBLISHED:
				// FIXME
				break;

			case msg_code::SUBSCRIBE:
				throw protocol_error("received SUBSCRIBE message unexpected for WAMP client roles");

			case msg_code::SUBSCRIBED:
				process_subscribed(msg);
				break;

			case msg_code::UNSUBSCRIBE:
				throw protocol_error("received UNSUBSCRIBE message unexpected for WAMP client roles");

			case msg_code::UNSUBSCRIBED:
				process_unsubscribed(msg);
				break;

			case msg_code::EVENT:
				process_event(msg);
				break;

			case msg_code::CALL:
				throw protocol_error("received CALL message unexpected for WAMP client roles");

			case msg_code::CANCEL:
				throw protocol_error("received CANCEL message unexpected for WAMP client roles");

			case msg_code::RESULT:
				process_call_result(msg);
				break;

			case msg_code::REGISTER:
				throw protocol_error("received REGISTER message unexpected for WAMP client roles");

			case msg_code::REGISTERED:
				process_registered(msg);
				break;

			case msg_code::UNREGISTER:
				throw protocol_error("received UNREGISTER message unexpected for WAMP client roles");

			case msg_code::UNREGISTERED:
				// FIXME
				break;

				// Not yet supported
			case msg_code::INVOCATION:
				throw protocol_error("received INVOCATION message - not implemented");
				break;

			case msg_code::INTERRUPT:
				throw protocol_error("received INTERRUPT message - not implemented");

			case msg_code::YIELD:
				throw protocol_error("received YIELD message unexpected for WAMP client roles");
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
			try
			{
				got_msg(message);
			}
			catch (const std::exception& e)
			{
				logMessage((std::string("Uncaught exception processing websocket message: ") + e.what()).c_str());
			}
		}

		void session::OnConnectionLost()
		{
			logMessage("WebSocket connection was lost, the current WAMP session is now invalid and must be re-established.");
		}

		void session::logMessage(const char* logContent)
		{
			Logger::Get()->LogMessage("autobahn", logContent);
		}

		void session::sendThread()
		{
			while (m_running)
			{
				std::unique_lock<std::mutex> lock(m_sendQueueMutex);
				m_sendEvent.wait(lock);

				decltype(m_websocket) ws;

				{
					std::lock_guard<std::mutex> websocketLock(m_websocketMutex);

					ws = m_websocket;
					if (!ws)
					{
						break;
					}
				}

				try
				{
					while (m_sendQueue.size())
					{
						auto sendBuffer = m_sendQueue.front();
						lock.unlock();

						try
						{
							std::lock_guard<std::mutex> websocketLock(m_websocketMutex);
							ws->SendUTF8(std::string(sendBuffer->data()));
						}
						catch (...)
						{
							stop(std::current_exception());
						}

						lock.lock();
						// Queue could have been cleared by another thread.
						if (m_sendQueue.size())
						{
							m_sendQueue.pop();
						}
					}
				}
				catch (...)
				{
					// Release lock if held to prevent double lock in stop.
					lock = {};
					stop(std::current_exception());
				}
			}
		}
	}
}
