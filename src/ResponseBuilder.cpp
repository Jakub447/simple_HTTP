
#include "ResponseBuilder.hpp"
#include "IMethodHandler.hpp"
#include "HTTPMethodFactory.hpp"
#include "MimeTypeRecognizer.hpp"
#include "ResponseCache.hpp"
#include "utils.hpp"
#include <iostream>
#include <memory>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>

namespace HTTP_Server
{
	static std::string prot_version_to_string(const int &prot_ver)
	{
		if (11 == prot_ver)
		{
			return "1.1";
		}
		else if (10 == prot_ver)
		{
			return "1.0";
		}
		return "Not a valid version";
	}

	std::string ResponseBuilder::prepare_protocol_version(const int &req_prot_ver)
	{
		std::string protocol_string = "HTTP/" + prot_version_to_string(resp_info.prot_ver);
		const std::string protocol_example = "HTTP/3.0";

		if (protocol_example.length() <= protocol_string.length())
		{
			return protocol_string;
		}

		return "";
	}

	int ResponseBuilder::prepare_status_line()
	{
		std::string protocol_string = prepare_protocol_version(resp_info.prot_ver);
		std::string status_code = std::to_string(resp_info.resp_code);
		std::string status_message = resp_info.status_message;

		resp_info.resp_final_header = protocol_string + " " + status_code + " " + status_message + "\r\n";
		return 0;
	}

	// Function to find the first available default file
	static std::string find_default_file(const std::string &root_dir, const std::string &URI, std::vector<std::string> defaultFiles)
	{
		for (const auto &fileName : defaultFiles)
		{
			std::cout << "fileName: " << fileName << std::endl;
			// std::cout << "defaultFiles: "<< defaultFiles << std::endl;
			std::cout << "URI: " << URI << std::endl;

			std::ifstream file(concatenatePath(root_dir, fileName));
			// std::cout << "file: "<< file << std::endl;
			if (file.good())
			{
				return URI + fileName; // Return the first file that exists
			}
		}
		return ""; // Return an empty string if no default file is found
	}

	static std::string alter_root_URI(const std::string &root_dir, const std::string &URI)
	{
		std::vector<std::string> defaultFiles = {
			"index.html",
			"index.htm",
			// root_dir+"index.php",
			// root_dir+"index.asp",
			// root_dir+"index.aspx",
			// root_dir+"index.jsp",
			"default.html",
			"default.htm",
			"home.html",
			"home.htm",
			"main.html",
			"main.htm",
			"welcome.html",
			"welcome.htm"};

		if ("/" == URI)
		{
			std::string altered_URI = find_default_file(root_dir, URI, defaultFiles);
			if (URI.empty())
			{
				return "";
			}
			else
			{
				return altered_URI;
			}
		}
		else // do nothing
		{
			return URI;
		}
	}

	int ResponseBuilder::handle_HTTP_request(ResponseCache &response_cache, std::unique_ptr<CacheEntry> &cache_entry, bool &is_served_from_cache, std::string body)
	{

		std::string altered_URI = alter_root_URI(root_dir, req_info.URI);

		if (altered_URI.empty())
		{
			resp_info.resp_code = 405;
			resp_info.status_message = "NOT FOUND";
			return -1;
		}

		req_info.URI = altered_URI;

		req_info.body = body;

		// Use the connectionInfo struct to handle variables
		auto handler = HTTPMethodFactory::create_handler(req_info.method);
		if (handler)
		{
			std::cout << "handler found!" << std::endl;
			return handler->handle_method(root_dir, req_info, req_headers, resp_headers, resp_info, response_cache, cache_entry, is_served_from_cache);
		}
		else
		{
			std::cout << "handler not found!" << std::endl;
			return -1;
		}
	}

	static std::string get_http_date()
	{
		// Get the current time in GMT
		std::time_t now = std::time(nullptr);
		std::tm *gmtTime = std::gmtime(&now);

		// Create a string stream to format the date
		std::ostringstream oss;
		oss << std::put_time(gmtTime, "%a, %d %b %Y %H:%M:%S GMT");

		return oss.str();
	}

	/*static const std::vector<std::string> headersToStore = {
		"Content-Type", "Content-Length", "Cache-Control", "Expires",
		"Date", "Last-Modified", "ETag", "Content-Encoding", "Vary"};*/

	int ResponseBuilder::prepare_headers(ResponseCache &response_cache, std::unique_ptr<CacheEntry>& cache_entry,const bool& is_served_from_cache)
	{
		prepare_status_line();

		if (is_served_from_cache)
		{
			// Use headers from the cache entry
			for (const auto &header : cache_entry->selected_headers.get_all_header_pairs())
			{
				resp_headers.addHeader(header.first, header.second);
			}

			// Generate any dynamic headers that need to be fresh
			//resp_headers.addHeader("Date", get_http_date());
			//resp_headers.addHeader("Connection", "keep-alive");

			// If serving from cache, add `Age` header
			auto age = std::chrono::duration_cast<std::chrono::seconds>(
						   std::chrono::steady_clock::now() - cache_entry->timestamp)
						   .count();
			resp_headers.addHeader("Age", std::to_string(age));
		}
		else
		{
			//resp_headers.addHeader("Server", "Own_http_server/0.1");
			//resp_headers.addHeader("Content-Length", std::to_string(resp_info.resp_final_body.length()));
			//resp_headers.addHeader("Date", get_http_date());
			//resp_headers.addHeader("Connection", "keep-alive");
		}

		resp_headers.addHeader("Server", "Own_http_server/0.1");
		resp_headers.addHeader("Date", get_http_date());
		resp_headers.addHeader("Connection", "keep-alive");

		resp_info.resp_final_header += resp_headers.GetAllHeaders();

		resp_info.resp_final_header += "\r\n";

		return 0;
	}

	int ResponseBuilder::prepare_full_message()
	{
		resp_info.resp_full_message = resp_info.resp_final_header + resp_info.resp_final_body;
		return 0;
	}

	std::string ResponseBuilder::get_full_message()
	{
		return resp_info.resp_full_message;
	}

	int ResponseBuilder::update_resp_info(int new_resp_code, std::string new_status_message)
	{
		resp_info.resp_code = new_resp_code;
		resp_info.status_message = new_status_message;
		return 0;
	}

}
