#include <unistd.h>
#include <arpa/inet.h>
#include <cerrno>
#include <iostream>
#include <fcntl.h>
#include <cstring>
#include <string>
#include <memory>
#include <vector>
#include <thread>
#include <chrono>
#include <sys/select.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <openssl/ssl.h>
#include <openssl/err.h>

#include "HTTPServer.hpp"
#include "RequestAnalyzer.hpp"
#include "ResponseBuilder.hpp"
#include "../liblogger/liblogger.hpp"
#include "error_codes.hpp"

namespace HTTP_Server
{

	// Constructor to initialize port and buffer_size with default values
	HTTPServer::HTTPServer(int port, int buffer_size, std::string root_directory, SSL_CTX *ctx)
		: port(port), buffer_size(buffer_size), server_socket(-1), root_directory(root_directory), ctx(ctx) {}

	// Helper function to set a socket as non-blocking
	static void set_non_blocking(int socket)
	{
		lib_logger::LOG(lib_logger::LogLevel::TRACE, "");
		int flags = fcntl(socket, F_GETFL, 0);
		fcntl(socket, F_SETFL, flags | O_NONBLOCK);
	}

	static void init_openssl()
	{
		lib_logger::LOG(lib_logger::LogLevel::TRACE, "");
		SSL_load_error_strings();
		OpenSSL_add_ssl_algorithms();
	}

	static void cleanup_openssl()
	{
		lib_logger::LOG(lib_logger::LogLevel::TRACE, "");
		EVP_cleanup();
	}

	static SSL_CTX *create_context()
	{
		lib_logger::LOG(lib_logger::LogLevel::TRACE, "");
		const SSL_METHOD *method = TLS_server_method();
		SSL_CTX *ctx = SSL_CTX_new(method);

		SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, nullptr);

		if (!ctx)
		{
			perror("Unable to create SSL context");
			ERR_print_errors_fp(stderr);
			exit(EXIT_FAILURE);
		}
		return ctx;
	}

	void HTTPServer::configure_context()
	{
		lib_logger::LOG(lib_logger::LogLevel::TRACE, "");
		if (SSL_CTX_use_certificate_file(ctx, "server.crt", SSL_FILETYPE_PEM) <= 0 ||
			SSL_CTX_use_PrivateKey_file(ctx, "server.key", SSL_FILETYPE_PEM) <= 0)
		{
			ERR_print_errors_fp(stderr);
			exit(EXIT_FAILURE);
		}
	}

	static bool rate_limit_exceeded(std::string &client_ip, int client_fd)
	{
		lib_logger::LOG(lib_logger::LogLevel::TRACE, "");
		// Rate limiting logic
		auto now = std::chrono::steady_clock::now();
		auto &rate_info = rate_limits[client_ip];

		if (std::chrono::duration_cast<std::chrono::seconds>(now - rate_info.last_request_time).count() >= WINDOW_DURATION_S)
		{
			rate_info.request_count = 0; // Reset count every second
			rate_info.last_request_time = now;
		}

		rate_info.request_count++;

		if (rate_info.request_count > MAX_REQUESTS_PER_SECOND)
		{
			lib_logger::LOG(lib_logger::LogLevel::WARNING, "Rate limit exceeded for: %s number of entries: %d in: %d seconds", client_ip.c_str(), rate_info.request_count, MAX_REQUESTS_PER_SECOND);
			close(client_fd);
			return true;
		}
		return false;
	}

	int HTTPServer::server_init()
	{
		lib_logger::LOG(lib_logger::LogLevel::TRACE, "");

		lib_logger::Logger::Instance().Set_log_level(lib_logger::LogLevel::DEBUG);
		// lib_logger::Logger::Instance().Set_max_file_size(1024 * 1024);
		// lib_logger::Logger::Instance().Set_output_file("log-1.txt");

		std::string test_string = "world";
		lib_logger::LOG(lib_logger::LogLevel::TRACE, "Hello, %s! This is a test.", test_string.c_str());

		lib_logger::LOG(lib_logger::LogLevel::TRACE, "this is a test");
		lib_logger::LOG(lib_logger::LogLevel::DEBUG, "this is a test");
		lib_logger::LOG(lib_logger::LogLevel::INFO, "this is a test");
		lib_logger::LOG(lib_logger::LogLevel::WARNING, "this is a test");
		lib_logger::LOG(lib_logger::LogLevel::ERROR, "this is a test");
		lib_logger::LOG(lib_logger::LogLevel::CRITICAL, "this is a test");

		init_openssl();

		ctx = create_context();
		configure_context();

		// Create a socket
		server_socket = socket(AF_INET, SOCK_STREAM, 0);
		if (server_socket == -1)
		{

			lib_logger::LOG(lib_logger::LogLevel::ERROR, "Failed to create socket");
			return APP_ERR_SOCK_CREAT;
		}

		//set_non_blocking(server_socket);

		// Configure server address structure
		server_addr.sin_family = AF_INET;		  // IPv4
		server_addr.sin_addr.s_addr = INADDR_ANY; // Any incoming interface
		server_addr.sin_port = htons(port);		  // Port number

		// Set the SO_REUSEADDR option to reuse the port
		int opt = 1;
		if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
		{
			perror("setsockopt failed");
			close(server_socket);
			return APP_ERR_SOCK_OPT;
		}

		// Bind the socket to the specified port
		if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
		{
			lib_logger::LOG(lib_logger::LogLevel::ERROR, "Binding failed: %s", strerror(errno));
			close(server_socket);
			return APP_ERR_BIND;
		}

		// Listen for incoming connections
		if (listen(server_socket, 10) == -1)
		{
			lib_logger::LOG(lib_logger::LogLevel::ERROR, "Listen failed");
			close(server_socket);
			return APP_ERR_LISTEN;
		}

		lib_logger::LOG(lib_logger::LogLevel::INFO, "Server is running on port: %d...", port);
		return APP_ERR_OK;
	}

	void HTTPServer::run()
	{
		lib_logger::LOG(lib_logger::LogLevel::TRACE, "");
		ResponseCache response_cache;

		while (true)
		{

			sockaddr_in client_addr{};
			socklen_t client_len = sizeof(client_addr);
			lib_logger::LOG(lib_logger::LogLevel::ERROR, "before accept: ");
			int client_fd = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
			if (client_fd < 0)
			{
				lib_logger::LOG(lib_logger::LogLevel::ERROR, "Accept failed: %s", strerror(errno));
				continue;
			}
lib_logger::LOG(lib_logger::LogLevel::ERROR, " accepted: ");
			std::string client_ip = inet_ntoa(client_addr.sin_addr);
			if (rate_limit_exceeded(client_ip, client_fd))
			{
				continue;
			}
lib_logger::LOG(lib_logger::LogLevel::ERROR, "after rate limit ");
			// Set up the SSL object for the connection
			SSL *ssl = SSL_new(ctx);
			SSL_set_fd(ssl, client_fd);
			if (SSL_accept(ssl) <= 0)
			{
				ERR_print_errors_fp(stderr);
				close(client_fd);
				SSL_free(ssl);
				continue;
			}

			try
			{
				std::thread conn_thread(&HTTPServer::handle_client_request, this, client_fd, std::ref(response_cache), ssl);
				conn_thread.detach();
			}
			catch (const std::exception &e)
			{
				lib_logger::LOG(lib_logger::LogLevel::ERROR, "Unable to create thread: %s", e.what());
				close(client_fd);
				SSL_shutdown(ssl);
				SSL_free(ssl);
			}
		}
	}

	// The modified handle_client_request now loops (or "recurse") internally,
	// using select() for timeout. New data resets the timeout.
	void HTTPServer::handle_client_request(int client_fd, ResponseCache &response_cache, SSL *ssl)
	{
		lib_logger::LOG(lib_logger::LogLevel::TRACE, "");
		bool connection_active = true;
		char buffer[1024];

		while (connection_active)
		{
			// Set up file descriptor set for select()
			fd_set readfds;
			FD_ZERO(&readfds);
			FD_SET(client_fd, &readfds);

			// Set timeout duration for select()
			timeval tv;
			tv.tv_sec = CLIENT_TIMEOUT_MS / 1000;
			tv.tv_usec = (CLIENT_TIMEOUT_MS % 1000) * 1000;

			int sel = select(client_fd + 1, &readfds, nullptr, nullptr, &tv);
			if (sel > 0 && FD_ISSET(client_fd, &readfds))
			{
				// New data is available; read it
				int bytes_read = SSL_read(ssl, buffer, sizeof(buffer) - 1);
				if (bytes_read <= 0)
				{
					// Read error or connection closed
					lib_logger::LOG(lib_logger::LogLevel::DEBUG, "No more data or error from client: %d", client_fd);
					break;
				}
				buffer[bytes_read] = '\0';
				std::string request(buffer);

				lib_logger::LOG(lib_logger::LogLevel::DEBUG, "Received request: %s", request.c_str());

				// Process the request using the RequestAnalyzer and ResponseBuilder
				RequestAnalyzer analyzer(request);
				analyzer.parse_request();

				lib_logger::LOG(lib_logger::LogLevel::DEBUG, "HTTP ver: %d", analyzer.get_prot());
				lib_logger::LOG(lib_logger::LogLevel::DEBUG, "HTTP method: %s", http_method_to_string(analyzer.get_method()).c_str());
				lib_logger::LOG(lib_logger::LogLevel::DEBUG, "URI: %s", analyzer.get_URI().c_str());

				ResponseBuilder resp_builder(analyzer.get_info(), root_directory, analyzer.get_headers(), analyzer.get_body());
				int ret = 0;
				std::unique_ptr<CacheEntry> cache_entry;
				bool is_served_from_cache = false;
				if ((ret = resp_builder.handle_HTTP_request(response_cache, cache_entry, is_served_from_cache, analyzer.get_body())) != 0)
				{
					lib_logger::LOG(lib_logger::LogLevel::ERROR, "Error handling HTTP request: %d", ret);
					break;
				}

				if ((ret = resp_builder.prepare_headers(response_cache, cache_entry, is_served_from_cache)) != 0)
				{
					lib_logger::LOG(lib_logger::LogLevel::ERROR, "Error preparing headers: %d", ret);
					break;
				}

				if ((ret = resp_builder.prepare_full_message()) != 0)
				{
					lib_logger::LOG(lib_logger::LogLevel::ERROR, "Error preparing full message: %d", ret);
					break;
				}

				// Send the response headers
				if (SSL_write(ssl, resp_builder.get_headers().c_str(), resp_builder.get_headers().size()) <= 0)
				{
					lib_logger::LOG(lib_logger::LogLevel::ERROR, "Error sending headers");
					break;
				}

				// Send the response body (if any)
				if (!resp_builder.get_body().empty())
				{
					ssize_t totalSent = 0;
					while (totalSent < resp_builder.get_body().size())
					{
						int bytesSent = SSL_write(ssl, resp_builder.get_body().c_str() + totalSent,
												  resp_builder.get_body().size() - totalSent);
						if (bytesSent <= 0)
						{
							lib_logger::LOG(lib_logger::LogLevel::ERROR, "Error sending body");
							connection_active = false;
							break;
						}
						totalSent += bytesSent;
					}
				}

				// After processing, the loop “restarts” so any new data resets the timeout.
				// Optionally, you can add logic here to decide whether to keep the connection alive,
				// for example by checking a "Connection: close" header.
			}
			else if (sel == 0)
			{

				// Timeout reached with no activity.
				std::string timeout_response =
					"HTTP/1.1 408 Request Timeout\r\n"
					"Connection: close\r\n"
					"Content-Length: 0\r\n\r\n";
				SSL_write(ssl, timeout_response.c_str(), timeout_response.size());
				lib_logger::LOG(lib_logger::LogLevel::DEBUG, "Connection timeout for client: %d. Sent timeout response.", client_fd);
				break;

				// Timeout reached with no activity; close connection.
				lib_logger::LOG(lib_logger::LogLevel::DEBUG, "Connection timeout for client: %d", client_fd);
				break;
			}
			else
			{
				// select() error occurred
				lib_logger::LOG(lib_logger::LogLevel::ERROR, "Select error on client %d: %s", client_fd, strerror(errno));
				break;
			}
		}

		// Cleanup after leaving the loop (timeout, error, or connection closed)
		close(client_fd);
		SSL_shutdown(ssl);
		SSL_free(ssl);
		lib_logger::LOG(lib_logger::LogLevel::DEBUG, "Closed connection for client: %d", client_fd);
	}

	HTTPServer::~HTTPServer()
	{
		close(server_socket);
		SSL_CTX_free(ctx);
		cleanup_openssl();
	}
}