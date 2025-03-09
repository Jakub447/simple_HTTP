
#include "GETHandler.hpp"
#include "MimeTypeRecognizer.hpp"
#include "ResponseCache.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <openssl/sha.h> // Requires OpenSSL for hashing
#include <sys/stat.h>
#include <memory>
#include "utils.hpp"


#include "../liblogger/liblogger.hpp"

#include "error_codes.hpp"


namespace HTTP_Server
{

	// Function to convert FileOpenMode to std::ios flags
	static std::ios_base::openmode to_open_mode(bool is_binary)
	{
		lib_logger::LOG(lib_logger::LogLevel::TRACE,"");
		if (is_binary)
		{
			return std::ios::in | std::ios::binary;
		}
		else
		{
			return std::ios::in;
		}
	}

	static std::streamsize read_file_to_string(const std::string &filename, bool is_binary, bool &is_body_large, std::unique_ptr<std::string> &content)
	{
		std::ifstream file(filename, to_open_mode(is_binary));
		if (!file)
		{
			lib_logger::LOG(lib_logger::LogLevel::ERROR, "Unable to open file: %s", filename.c_str());
			return 0;
		}

		file.seekg(0, std::ios::end);
		std::streamsize file_size = file.tellg();
		file.seekg(0, std::ios::beg);

		// Define a threshold for what is considered a "large" file
    	const std::streamsize MAX_FILE_SIZE_THRESHOLD = 104857600; // For example, 100MB

		is_body_large = (file_size > MAX_FILE_SIZE_THRESHOLD);

		if(is_body_large)
		{
			return file_size;
		}

		content->clear();

		content->reserve(file_size);

		constexpr size_t buffer_size = 8192; // 8KB buffer
		std::unique_ptr<char []> buffer = std::make_unique<char []>(buffer_size);


		while (file.read(buffer.get(), buffer_size))
		{
			content->append(buffer.get(), file.gcount());
		}
		if (file.gcount() > 0)
		{
			content->append(buffer.get(), file.gcount());
		}

		return file_size;
	}

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

	int GETHandler::handle_method(const std::string &root_dir, const HTTP_request_info &req_info, const HTTPHeaders &req_headers, HTTPHeaders &resp_headers, HTTP_request_response &resp_info, ResponseCache &response_cache, std::unique_ptr<CacheEntry> &cache_entry, bool &is_served_from_cache)
	{
		lib_logger::LOG(lib_logger::LogLevel::TRACE,"");
		lib_logger::LOG(lib_logger::LogLevel::INFO,"serving GET method");
		resp_info.prot_ver = req_info.prot_ver;
		resp_info.resp_code = HTTP_ERR_OK;
		resp_info.status_message = get_srv_error_description((HTTP_error_code)resp_info.resp_code);

		std::string filename = concatenate_path(root_dir, req_info.URI);
		resp_info.file_name = filename;

		std::string currentETag = generate_ETag(filename);
		std::string cache_key = req_info.URI; // Use the URI as the cache key

		if (!resp_info.resp_final_body)
		{
			resp_info.resp_final_body = std::make_unique<std::string>();
		}

		// Check cache first
		auto cached_entry = response_cache.get(cache_key);
		if (cached_entry && response_cache.validate_cache_entry(cached_entry, req_headers))
		{

			lib_logger::LOG(lib_logger::LogLevel::DEBUG,"Serving from cache!");
			is_served_from_cache = true;
			cache_entry = std::make_unique<CacheEntry>(std::move(cached_entry.value()));

			// resp_info.resp_code = HTTP_ERR_NOT_MODIFIED;
			// resp_info.status_message = get_srv_error_description((HTTP_error_code)resp_info.resp_code);
			resp_info.resp_final_body = std::move(cache_entry->body);
			return APP_ERR_OK; // Successfully served from cache
		}

		resp_headers.add_header("ETag", currentETag);
		resp_headers.add_header("Cache-Control", "max-age=60");

		lib_logger::LOG(lib_logger::LogLevel::DEBUG,"File: %s", filename.c_str());

		MimeTypeRecognizer recognizer;
		MimeTypeInfo file_mime_type_info = recognizer.get_mime_type_Info(filename);
		resp_headers.add_header("Content-Type", file_mime_type_info.mimeType);
		std::streamsize file_size = read_file_to_string(filename, file_mime_type_info.is_binary,resp_info.is_body_large,resp_info.resp_final_body);

		if( 0 == file_size)
		{
			resp_info.resp_code = HTTP_ERR_NOT_FOUND;
			resp_info.status_message = get_srv_error_description((HTTP_error_code)resp_info.resp_code);

			lib_logger::LOG(lib_logger::LogLevel::WARNING,"404 NOT FOUND");
		}

		resp_headers.add_header("Content-Length", std::to_string(file_size));

		// Decide if the response should be cached based on headers or other conditions, dont store anything big, lol
		if (!resp_info.resp_final_body->empty() && (should_cache_response(resp_headers) && (false == resp_info.is_body_large)))
		{
			// Call put to store in the cache
			response_cache.put(cache_key, std::make_unique<std::string>(*resp_info.resp_final_body), resp_headers, currentETag);
		}

		return APP_ERR_OK;
	}
}