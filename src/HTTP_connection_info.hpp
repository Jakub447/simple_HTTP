#ifndef HTTP_connection_info_H
#define HTTP_connection_info_H

#include <string>
#include <memory>

namespace HTTP_Server
{

	enum HTTPMethod
	{
		GET,
		PUT,
		POST,
		DELETE,
		PATCH,
		HEAD,
		OPTIONS,
		UNKNOWN_METHOD
	};

	struct HTTP_request_info
	{
		int prot_ver;
		HTTPMethod method;
		std::string URI;
		std::string body;

		// Constructor
		HTTP_request_info() = default;
	};

	struct HTTP_request_response
	{
		int resp_code;
		int prot_ver;
		std::string status_message;
		std::unique_ptr<std::string> resp_final_header;
		std::unique_ptr<std::string> resp_final_body;
		std::unique_ptr<std::string> resp_full_message;
		bool is_body_large;
		std::string file_name;

		// Constructor
		HTTP_request_response() = default;
	};

	// Helper function to convert enum back to string (for debugging)
	std::string http_method_to_string(HTTPMethod method);
}

#endif // HTTP_connection_info_H