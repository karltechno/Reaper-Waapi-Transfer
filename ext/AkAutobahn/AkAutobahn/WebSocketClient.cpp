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

#include "AK/WwiseAuthoringAPI/AkAutobahn/WebSocketClient.h"

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#include <winsock2.h>
#endif

#include "AK/WwiseAuthoringAPI/AkAutobahn/civetweb.h"

#include "AK/WwiseAuthoringAPI/AkAutobahn/IWebSocketClientHandler.h"

namespace AK
{
	namespace WwiseAuthoringAPI
	{
		bool WebSocketClient::m_isNetworkInitialized = false;

		bool WebSocketClient::InitializeNetwork()
		{
#if defined(_WIN32) || defined(_WIN64)

			WSADATA data;
			return (WSAStartup(MAKEWORD(2, 2), &data) == 0);
#else
			return true;
#endif
		}

		int WebSocketClient::OnMessage(
			mg_connection *conn,
			int flags,
			char *data,
			size_t data_len,
			void *user_data)
		{
			mg_context* ctx = mg_get_context(conn);
			WebSocketClient* that = reinterpret_cast<WebSocketClient*>(mg_get_user_data(ctx));
			AKASSERT(that != nullptr);
			that->m_handler->OnMessage(std::string(data, data_len));

			return 1;
		}

		void WebSocketClient::OnClose(const struct mg_connection* conn, void* user_data)
		{
			mg_context* ctx = mg_get_context(conn);
			WebSocketClient* that = reinterpret_cast<WebSocketClient*>(mg_get_user_data(ctx));
			AKASSERT(that != nullptr);

			if (that->m_connection != nullptr)
			{
				that->m_handler->OnConnectionLost();
			}
		}

		bool WebSocketClient::SendUTF8(const std::string& message, std::string& out_errorMessage)
		{
			if (m_connection == nullptr)
			{
				out_errorMessage = "Could not send message, the connection is not established.";
				return false;
			}

			AKASSERT(!message.empty());
			int bytesSent = mg_websocket_client_write(m_connection, WEBSOCKET_OPCODE_TEXT, message.c_str(), message.length());

			if (bytesSent != message.length())
			{
				out_errorMessage = "Could not send websocket message.";
				return false;
			}

			return true;
		}
		
		bool WebSocketClient::Connect(const char* host, const int port)
		{
			AKASSERT(host != nullptr);
			AKASSERT(host[0] != 0);
			AKASSERT(port != 0);

			if (!EnsureNetworkInit())
			{
				return false;
			}

			char errorBuffer[256];

			/* Then connect a first client */
			m_connection = mg_connect_websocket_client(
				host,
				port,
				0,
				errorBuffer,
				sizeof(errorBuffer),
				"/waapi",
				NULL,
				OnMessage,
				OnClose,
				this);

			return (m_connection != nullptr);
		}

		void WebSocketClient::Close()
		{
			if (m_connection != nullptr)
			{
				// Make sure we don't call OnClose when explicitly closing.
				auto tmpConnection = m_connection;
				m_connection = nullptr;
				// There is a thread join in mg_close_connection, thus we are guaranteed
				// that OnClose callback has completed before leaving Close.
				mg_close_connection(tmpConnection);
			}
		}
	}
}
