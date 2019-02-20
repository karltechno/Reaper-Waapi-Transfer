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

#ifndef AUTOBAHN_H
#define AUTOBAHN_H

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <future>
#include <istream>
#include <map>
#include <mutex>
#include <ostream>
#include <queue>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "AK/WwiseAuthoringAPI/AkAutobahn/AkVariant.h"
#include "AK/WwiseAuthoringAPI/AkAutobahn/AkJson.h"
#include "AK/WwiseAuthoringAPI/AkAutobahn/JsonProvider.h"

#include "AK/WwiseAuthoringAPI/AkAutobahn/IWebSocketClientHandler.h"
#include "AK/WwiseAuthoringAPI/AkAutobahn/WebSocketClient.h"

// thank you microsoft
#ifdef ERROR
#undef ERROR
#endif

namespace AK
{
	namespace WwiseAuthoringAPI
	{
		typedef AkVariant any;

		/// A vector holding any values.
		typedef std::vector<AkVariant> anyvec;

		/// Handler type for use with session::subscribe(const std::string&, handler_t)
		typedef std::function<void(uint64_t, const JsonProvider&)> handler_t;

		/// Disconnect handler.
		typedef std::function<void()> disconnectHandler_t;

		struct result_t
		{
			result_t() : success(false) {}
			result_t(bool in_success) : success(in_success) {}
			result_t(bool in_success, const AkJson& in_data) : success(in_success), data(in_data) {}

			bool success;
			AkJson data;
		};


		/// Represents a topic subscription.
		struct subscription
		{
			subscription() : id(0), success(false) {}
			subscription(const AkJson& in_errorJson) : id(0), success(false), errorJson(in_errorJson) {}
			subscription(uint64_t in_id) : id(in_id), success(true) {};

			uint64_t id;
			bool success;
			AkJson errorJson;
		};

		/// Represents the authentication information sent on welcome
		struct authinfo
		{
			std::string authmethod;
			std::string authprovider;
			std::string authid;
			std::string authrole;
		};

		/*!
		* A WAMP session.
		*/
		class session : public IWebSocketClientHandler
		{
		public:

			static void createErrorMessageJson(const std::string& in_message, AkJson& out_jsonError);
			
		private:

			static void createNoSessionErrorJson(AkJson& out_jsonError);

		public:

			session();
			~session();

			/*!
			* Start listening on the IStream provided to the constructor
			* of this session.
			*/
			bool start(const char* in_uri, unsigned int in_port, disconnectHandler_t disconnectHandler = nullptr);

			/*!
			* Closes the IStream and the OStream provided to the constructor
			* of this session.
			*/
			void stop(const std::string& errorMessage);

			bool isConnected() const;

			uint64_t getSessionId() const { return m_session_id; }

			/*!
			* Join a realm with this session.
			*
			* \param realm The realm to join on the WAMP router connected to.
			* \param method The method used for login. Empty string will cause no login.
			* \param authid The authid to login with.
			* \param signature The signature to use when logging in. For method "ticket" the ticket, for method "wampcra" the
			* passphrase.
			* \return A future that resolves with the session ID when the realm was joined.
			*/
			std::future<uint64_t> join(const std::string& realm, const std::string& method = "", const std::string& authid = "",
				const std::string& signature = "");

			authinfo getAuthInfo() const;

			bool subscribe(const std::string& topic, handler_t handler, const AkJson& options, std::future<subscription>& out_future, AkJson& out_jsonError);

			bool unsubscribe(uint64_t subscription_id, std::future<result_t>& out_future, AkJson& out_jsonError);

			bool call_options(
				const std::string& procedure,
				const anyvec& args,
				const AkJson& kwargs,
				const AkJson& options,
				std::future<result_t>& out_future,
				AkJson& out_jsonError);

		private:
			
			//////////////////////////////////////////////////////////////////////////////////////
			/// Caller

			/// An outstanding WAMP call.
			struct call_t
			{
				call_t() {}
				call_t(call_t&& c) : m_res(std::move(c.m_res)) {}
				std::promise<result_t> m_res;
			};

			/// Map of outstanding WAMP calls (request ID -> call).
			typedef std::map<uint64_t, call_t> calls_t;

			/// Map of WAMP call ID -> call
			calls_t m_calls;

			std::mutex m_callsMutex;


			//////////////////////////////////////////////////////////////////////////////////////
			/// Subscriber

			/// An outstanding WAMP subscribe request.
			struct subscribe_request_t
			{
				subscribe_request_t(){};
				subscribe_request_t(subscribe_request_t&& s) : m_handler(std::move(s.m_handler)), m_res(std::move(s.m_res)) {}
				subscribe_request_t(handler_t handler) : m_handler(handler){};
				handler_t m_handler;
				std::promise<subscription> m_res;
			};

			/// Map of outstanding WAMP subscribe requests (request ID -> subscribe request).
			typedef std::map<uint64_t, subscribe_request_t> subscribe_requests_t;

			/// Map of WAMP subscribe request ID -> subscribe request
			subscribe_requests_t m_subscribe_requests;

			std::mutex m_subreqMutex;

			/// Map of subscribed handlers (subscription ID -> handler)
			typedef std::map<uint64_t, handler_t> handlers_t;

			/// Map of WAMP subscription ID -> handler
			std::mutex m_handlersMutex;
			handlers_t m_handlers;

			/// Disconnect handler.
			disconnectHandler_t m_disconnectHandler;

			// No mutex required.

			//////////////////////////////////////////////////////////////////////////////////////
			/// Unsubscriber

			/// An outstanding WAMP unsubscribe request.
			struct unsubscribe_request_t
			{
				unsubscribe_request_t(){};
				unsubscribe_request_t(unsubscribe_request_t&& s) : m_res(std::move(s.m_res)) {}
				std::promise<result_t> m_res;
			};

			/// Map of outstanding WAMP subscribe requests (request ID -> subscribe request).
			typedef std::map<uint64_t, unsubscribe_request_t> unsubscribe_requests_t;

			/// Map of WAMP subscribe request ID -> subscribe request
			unsubscribe_requests_t m_unsubscribe_requests;

			std::mutex m_unsubreqMutex;


			//////////////////////////////////////////////////////////////////////////////////////
			/// Callee


			/// An unserialized, raw WAMP message.
			typedef AkJson wamp_msg_t;


			/// Process a WAMP WELCOME message.
			void process_welcome(const wamp_msg_t& msg);

			/// Process a WAMP CHALLENGE message.
			void process_challenge(const wamp_msg_t& msg);

			/// Process a WAMP ERROR message.
			void process_error(const wamp_msg_t& msg);

			/// Process a WAMP RESULT message.
			void process_call_result(const wamp_msg_t& msg);

			/// Process a WAMP SUBSCRIBED message.
			void process_subscribed(const wamp_msg_t& msg);

			/// Process a WAMP UNSUBSCRIBED message.
			void process_unsubscribed(const wamp_msg_t& msg);

			/// Process a WAMP EVENT message.
			void process_event(const wamp_msg_t& msg);

			/// Process a WAMP GOODBYE message.
			void process_goodbye(const wamp_msg_t& msg);


			/// Send wamp message. Asynchronous.
			void send(const AkJson& jsonPayload);
			void send(std::string ssout);

			/// Process incoming message.
			void got_msg(const std::string& jsonPayload);

			// IWebSocketClientHandler
			void OnMessage(std::string&& message);
			void OnConnectionLost();

			void sendThread();

			void logMessage(const char* logContent);

#ifdef VALIDATE_WAMP
			void WampAssert(bool value, const char* message);
#endif

			std::atomic<bool> m_running;
			std::atomic<bool> m_wasDisconnected;

			std::shared_ptr<WebSocketClient> m_websocket;
			std::recursive_mutex m_websocketMutex;

			std::thread m_sendThread;

			std::mutex m_sendQueueMutex;
			std::condition_variable m_sendEvent;
			std::queue<std::shared_ptr<std::vector<char>>> m_sendQueue;

			//Poco::JSON::Parser m_parser;

			/// WAMP session ID (if the session is joined to a realm).
			uint64_t m_session_id = 0;

			/// Future to be fired when session was joined.
			std::promise<uint64_t> m_session_join;

			std::mutex m_joinMutex;

			/// Last request ID of outgoing WAMP requests.
			uint64_t m_request_id = 0;

			/// Signature to be used to authenticate
			std::string m_signature;

			/// Authentication information sent on welcome
			authinfo m_authinfo;


			bool m_goodbye_sent = false;

			/// WAMP message type codes.
			enum class msg_code : int
			{
				HELLO = 1,
				WELCOME = 2,
				ABORT = 3,
				CHALLENGE = 4,
				AUTHENTICATE = 5,
				GOODBYE = 6,
				HEARTBEAT = 7,
				ERROR = 8,
				PUBLISH = 16,
				PUBLISHED = 17,
				SUBSCRIBE = 32,
				SUBSCRIBED = 33,
				UNSUBSCRIBE = 34,
				UNSUBSCRIBED = 35,
				EVENT = 36,
				CALL = 48,
				CANCEL = 49,
				RESULT = 50,
				REGISTER = 64,
				// Renamed from original source to avoid clash with macro from Wwise SDK.
				// Keeping the others as-is to remain in sync with the original source as much as possible.
				WAMP_REGISTERED = 65,
				UNREGISTER = 66,
				UNREGISTERED = 67,
				INVOCATION = 68,
				INTERRUPT = 69,
				YIELD = 70
			};
		};
	}
}

#endif // AUTOBAHN_H
