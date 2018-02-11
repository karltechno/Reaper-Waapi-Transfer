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

#include "WebSocketClient.h"

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#endif

#include "civetweb.h"

#include "IWebSocketClientHandler.h"

namespace AK
{
	namespace WwiseAuthoringAPI
	{
		bool WebSocketClient::m_isNetworkInitialized = false;

		void WebSocketClient::InitializeNetwork()
		{
#if defined(_WIN32) || defined(_WIN64)

			WSADATA data;
			int result = WSAStartup(MAKEWORD(2, 2), &data);

			if (result != 0)
			{
				std::string error = "Could not initialize network: ";

				switch (result)
				{
				case WSASYSNOTREADY:
					error += "WSASYSNOTREADY";
					break;

				case WSAVERNOTSUPPORTED:
					error += "WSAVERNOTSUPPORTED";
					break;

				case WSAEINPROGRESS:
					error += "WSAEINPROGRESS";
					break;

				case WSAEPROCLIM:
					error += "WSAEPROCLIM";
					break;

				case WSAEFAULT:
					error += "WSAEFAULT";
					break;

				default:
					error += "Unknown error";
					break;
				}

				throw std::runtime_error(error);
			}
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
			assert(that != nullptr);
			that->m_handler->OnMessage(std::string(data, data_len));

			return 1;
		}

		void WebSocketClient::OnClose(const struct mg_connection* conn, void* user_data)
		{
			mg_context* ctx = mg_get_context(conn);
			WebSocketClient* that = reinterpret_cast<WebSocketClient*>(mg_get_user_data(ctx));
			assert(that != nullptr);

			if (that->m_connection != nullptr)
			{
				that->m_connection = nullptr;
				that->m_handler->OnConnectionLost();
			}
		}

		void WebSocketClient::SendUTF8(const std::string& message)
		{
			if (m_connection == nullptr)
			{
				throw std::runtime_error("Could not send message, the connection is not established.");
			}

			assert(!message.empty());
			int bytesSent = mg_websocket_client_write(m_connection, WEBSOCKET_OPCODE_TEXT, message.c_str(), message.length());

			if (bytesSent != message.length())
			{
				throw std::runtime_error("Could not send websocket message.");
			}
		}

		void WebSocketClient::Connect(const char* host, const int port)
		{
			assert(host != nullptr);
			assert(host[0] != 0);
			assert(port != 0);

			EnsureNetworkInit();

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

			if (m_connection == nullptr)
			{
				throw std::runtime_error(std::string("Could not establish connection: ") + errorBuffer);
			}
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
