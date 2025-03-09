#ifndef RESPONSE_BUILDER_H
#define RESPONSE_BUILDER_H

#include <string>
#include <map>
#include <vector>
#include <sstream>
#include <algorithm>
#include <iostream>
#include <memory>
#include "HTTP_connection_info.hpp"
#include "HttpHeaders.hpp"
#include "ResponseCache.hpp"
#include <memory>


namespace HTTP_Server
{
	class ResponseBuilder
	{
	public:
		ResponseBuilder(const HTTP_request_info &req_info, const std::string &root_dir, const HTTPHeaders &req_headers, const std::string body)
			: req_info(req_info), root_dir(root_dir), req_headers(req_headers), req_body(body) {};

		int handle_HTTP_request(ResponseCache& response_cache, std::unique_ptr<CacheEntry>& cache_entry, bool& is_served_from_cache, std::string body);
		int prepare_headers(ResponseCache &response_cache, std::unique_ptr<CacheEntry>& cache_entry,const bool& is_served_from_cache);
		int prepare_full_message();
		std::unique_ptr<std::string> get_full_message();

		const std::unique_ptr<std::string>& get_headers(){return std::move(resp_info.resp_final_header);};
		const std::unique_ptr<std::string>& get_body(){return resp_info.resp_final_body;};
		std::string get_resp_status(){return resp_info.status_message;};
		int get_resp_code(){return resp_info.resp_code;};
		bool is_body_large(){return resp_info.is_body_large;};
		std::string get_file_name(){return resp_info.file_name;};

		int update_resp_info(int new_resp_code, std::string new_status_message);

	private:
		HTTP_request_info req_info;
		std::string root_dir;
		HTTPHeaders req_headers;
		HTTPHeaders resp_headers;
		std::string req_body;
		HTTP_request_response resp_info;

		std::string prepare_protocol_version(const int& prot_ver);
		int prepare_status_line();
	};

}

#endif // RESPONSE_BUILDER_H