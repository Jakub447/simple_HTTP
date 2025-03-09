#ifndef ERROR_CODES_H
#define ERROR_CODES_H

#include <string>
#include <unordered_map>

namespace HTTP_Server
{

	// Define error codes
	enum HTTP_error_code
	{
		HTTP_ERR_OK = 200,					// No error
		HTTP_ERR_CREAT = 201,					// created
		HTTP_ERR_NO_CONTENT = 204,					// no content
		HTTP_ERR_NOT_MODIFIED = 304,					// NNOT MODIFIED
		HTTP_ERR_INVALID_REQUEST = 400,		// Bad Request
		HTTP_ERR_UNAUTHORIZED = 401,		// Unauthorized
		HTTP_ERR_FORBIDDEN = 403,			// Forbidden
		HTTP_ERR_NOT_FOUND = 404,			// Not Found
		HTTP_ERR_METHOD_NOT_ALLOWED = 405,	// Method Not Allowed
		HTTP_ERR_REQUEST_TIMEOUT = 408,		// Request Timeout
		HTTP_ERR_URI_TOO_LONG = 414,		// URI Too Long
		HTTP_ERR_INTERNAL_SERVER = 500,		// Internal Server Error
		HTTP_ERR_NOT_IMPLEMENTED = 501,		// Not Implemented
		HTTP_ERR_SERVICE_UNAVAILABLE = 503, // Service Unavailable
		HTTP_ERR_GATEWAY_TIMEOUT = 504,		// Gateway Timeout
		HTTP_ERR_UNKNOWN = 999				// Unknown Error
	};

	enum app_error_code
	{
		APP_ERR_OK = 0,				// No error
		APP_ERR_FAILURE = -1,		// generic error
		APP_ERR_SOCK_CREAT = -2,	// socket creation error
		APP_ERR_SOCK_OPT = -3,		// socket options error
		APP_ERR_BIND = -4,			// binding error
		APP_ERR_LISTEN = -5,		// listen error
		APP_ERR_NO_DIR = -6,		// Unable to create directories for file
		APP_ERR_NO_FILE_OPEN = -7,	// unable oto pen file error
		APP_ERR_FILE_WRITE = -8,	// file write error
		APP_ERR_NO_REQ_HANDLER = -9, // no handler error
		APP_ERR_NULL_PTR = -10 		// null pointer
	};

	// Error descriptions mapped to server error codes
	inline const std::unordered_map<HTTP_error_code, std::string> HTTP_ERROR_DESCRIPTIONS = {
		{HTTP_ERR_OK, "Success"},
		{HTTP_ERR_CREAT, "Created"},
		{HTTP_ERR_NO_CONTENT, "no content"},
		{HTTP_ERR_NOT_MODIFIED, "not modified"},
		{HTTP_ERR_INVALID_REQUEST, "Bad Request: The server could not understand the request."},
		{HTTP_ERR_UNAUTHORIZED, "Unauthorized: Access is denied due to invalid credentials."},
		{HTTP_ERR_FORBIDDEN, "Forbidden: You do not have permission to access this resource."},
		{HTTP_ERR_NOT_FOUND, "Not Found: The requested resource could not be found."},
		{HTTP_ERR_METHOD_NOT_ALLOWED, "Method Not Allowed: The HTTP method is not supported for this resource."},
		{HTTP_ERR_REQUEST_TIMEOUT, "Request Timeout: The server timed out waiting for the request."},
		{HTTP_ERR_URI_TOO_LONG, "URI Too Long: The requested URI is too long for the server to process."},
		{HTTP_ERR_INTERNAL_SERVER, "Internal Server Error: An unexpected error occurred on the server."},
		{HTTP_ERR_NOT_IMPLEMENTED, "Not Implemented: The server does not support the requested functionality."},
		{HTTP_ERR_SERVICE_UNAVAILABLE, "Service Unavailable: The server is currently unable to handle the request."},
		{HTTP_ERR_GATEWAY_TIMEOUT, "Gateway Timeout: The server acted as a gateway and timed out waiting for a response."},
		{HTTP_ERR_UNKNOWN, "Unknown Error: An unspecified error occurred."}};

	// Error descriptions mapped to application error codes
	inline const std::unordered_map<app_error_code, std::string> APP_ERROR_DESCRIPTIONS = {
		{APP_ERR_OK, "Success"},
		{APP_ERR_FAILURE, "General failure error"},
		{APP_ERR_SOCK_CREAT, "Could not create socket"},
		{APP_ERR_SOCK_OPT, "Could not set proper socket options"},
		{APP_ERR_BIND, "Binding failed."},
		{APP_ERR_LISTEN, "Could not set proper lsitening on socket."},
		{APP_ERR_NO_DIR, "Could not create directory for the file."},
		{APP_ERR_NO_FILE_OPEN, "Could not open the file."},
		{APP_ERR_FILE_WRITE, "Could not write to file"},
		{APP_ERR_NO_REQ_HANDLER, "Could not find request handler for given request!"}};

	// Function to retrieve error description based on http error code
	inline std::string get_srv_error_description(HTTP_error_code code)
	{
		auto it = HTTP_ERROR_DESCRIPTIONS.find(code);
		if (it != HTTP_ERROR_DESCRIPTIONS.end())
		{
			return it->second;
		}
		return "Unknown Error: The error code is not recognized.";
	}

	// Function to retrieve error description based on application error code
	inline std::string get_app_error_description(app_error_code code)
	{
		auto it = APP_ERROR_DESCRIPTIONS.find(code);
		if (it != APP_ERROR_DESCRIPTIONS.end())
		{
			return it->second;
		}
		return "Unknown Error: The error code is not recognized.";
	}

}

#endif // ERROR_CODES_H