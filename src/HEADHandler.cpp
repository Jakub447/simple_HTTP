
#include "HEADHandler.hpp"
#include "MimeTypeRecognizer.hpp"
#include "ResponseCache.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <openssl/sha.h> // Requires OpenSSL for hashing
#include <sys/stat.h>
#include "utils.hpp"
#include <memory>

#include "../liblogger/liblogger.hpp"

#include "error_codes.hpp"


namespace HTTP_Server
{

	static std::string generate_ETag(const std::string &filename)
	{
		std::filesystem::path filePath(filename);

		if (!std::filesystem::exists(filePath))
			return "";

		// Get the last modified timestamp of the file
		auto fileTime = std::filesystem::last_write_time(filePath);

		// Convert the timestamp to a string or hash it directly
		std::ostringstream oss;
		oss << fileTime.time_since_epoch().count();

		// Compute SHA256 hash of the timestamp
		unsigned char hash[SHA256_DIGEST_LENGTH];
		SHA256(reinterpret_cast<const unsigned char *>(oss.str().data()), oss.str().size(), hash);

		// Convert hash to hex string
		std::ostringstream etag;
		for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i)
		{
			etag << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
		}

		return etag.str();
	}

	static bool should_cache_response(const HTTPHeaders &headers)
	{
		lib_logger::LOG(lib_logger::LogLevel::TRACE,"");
		// Check for a `Cache-Control` header with `no-store` directive
		if (headers.has_header("Cache-Control"))
		{
			std::string cache_control = headers.get_header("Cache-Control").front();
			if (cache_control.find("no-store") != std::string::npos)
			{
				return false;
			}
		}

		// Other conditions to determine caching can go here
		return true;
	}

	int HEADHandler::handle_method(const std::string &root_dir, const HTTP_request_info &req_info, const HTTPHeaders &req_headers, HTTPHeaders &resp_headers, HTTP_request_response &resp_info, ResponseCache &response_cache, std::unique_ptr<CacheEntry> &cache_entry, bool &is_served_from_cache)
	{
		lib_logger::LOG(lib_logger::LogLevel::TRACE,"");
		lib_logger::LOG(lib_logger::LogLevel::INFO,"serving GET method");
		resp_info.prot_ver = req_info.prot_ver;
		resp_info.resp_code = HTTP_ERR_OK;
		resp_info.status_message = get_srv_error_description((HTTP_error_code)resp_info.resp_code);

		std::string filename = concatenate_path(root_dir, req_info.URI);
		lib_logger::LOG(lib_logger::LogLevel::DEBUG,"filename after fun: %s", filename.c_str());

		std::string currentETag = generate_ETag(filename);
		std::string cache_key = req_info.URI; // Use the URI as the cache key

		// Check cache first
		auto cached_entry = response_cache.get(cache_key);
		if (cached_entry && response_cache.validate_cache_entry(cached_entry, req_headers))
		{

			lib_logger::LOG(lib_logger::LogLevel::DEBUG,"Serving from cache!");
			is_served_from_cache = true;
			cache_entry = std::make_unique<CacheEntry>(std::move(cached_entry.value()));

			// resp_info.resp_code = HTTP_ERR_NOT_MODIFIED;
			// resp_info.status_message = get_srv_error_description((HTTP_error_code)resp_info.resp_code);
			resp_info.resp_final_body = std::move(cached_entry->body);
			return APP_ERR_OK; // Successfully served from cache
		}

		resp_headers.add_header("ETag", currentETag);
		resp_headers.add_header("Cache-Control", "max-age=60");

		lib_logger::LOG(lib_logger::LogLevel::DEBUG,"File: %s", filename.c_str());

		MimeTypeRecognizer recognizer;
		MimeTypeInfo file_mime_type_info = recognizer.get_mime_type_Info(filename);
		resp_headers.add_header("Content-Type", file_mime_type_info.mimeType);


		if(!std::filesystem::exists(filename))
		{
			resp_info.resp_code = HTTP_ERR_NOT_FOUND;
			resp_info.status_message = get_srv_error_description((HTTP_error_code)resp_info.resp_code);

			lib_logger::LOG(lib_logger::LogLevel::WARNING,"404 NOT FOUND");
		}

		resp_headers.add_header("Content-Length", std::to_string(resp_info.resp_final_body->length()));

		// Decide if the response should be cached based on headers or other conditions
		if (!resp_info.resp_final_body->empty() && should_cache_response(resp_headers))
		{
			// Call put to store in the cache
			response_cache.put(cache_key, std::move(resp_info.resp_final_body), resp_headers, currentETag);
		}

		return APP_ERR_OK;
	}
}