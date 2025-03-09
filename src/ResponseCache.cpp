#include "ResponseCache.hpp"
#include <iostream>
#include <memory>

#include "../liblogger/liblogger.hpp"

namespace HTTP_Server
{
	// Check if a header should be stored according to RFC 9111
	bool ResponseCache::should_store_header(const std::string &headerName)
	{
		lib_logger::LOG(lib_logger::LogLevel::TRACE,"");
		// Define headers to store
		static const std::vector<std::string> headersToStore = {
			"Content-Type", "Content-Length", "Cache-Control", "Expires",
			"Date", "Last-Modified", "ETag", "Content-Encoding", "Vary"};

		// Only store headers that are listed
		return std::find(headersToStore.begin(), headersToStore.end(), headerName) != headersToStore.end();
	}

	// Retrieve a cache entry
	std::optional<CacheEntry> ResponseCache::get(const std::string &key)
	{
		lib_logger::LOG(lib_logger::LogLevel::TRACE,"");
		std::lock_guard<std::mutex> lock(mutex);
		auto it = cache.find(key);
		if (it != cache.end())
		{
			// Check if the cached entry is still valid
			auto now = std::chrono::steady_clock::now();
			if (now - it->second.timestamp < it->second.max_age)
			{
				return std::make_optional<CacheEntry>(std::move(it->second)); // Return valid cache entry
			}
			else
			{
				cache.erase(it); // Remove stale entry
			}
		}
		return std::nullopt; // Cache miss
	}

	// Store a new cache entry
	void ResponseCache::put(const std::string &key, std::unique_ptr<std::string> &&body, const HTTPHeaders &headers, const std::string &Etag)
	{
		lib_logger::LOG(lib_logger::LogLevel::TRACE,"");
		std::lock_guard<std::mutex> lock(mutex);
		CacheEntry entry;
		
		entry.body = std::move(body);
		entry.cached_Etag = Etag;

		// Store only selected headers
		for (const auto &headerName : headers.get_all_header_pairs())
		{
			if (should_store_header(headerName.first))
			{
				entry.selected_headers.add_header(headerName.first, headerName.second);
			}
		}

		// Extract and process the max-age value from Cache-Control if present
		std::string cache_control = headers.get_header("Cache-Control").empty() ? "" : headers.get_header("Cache-Control").front();
		if (cache_control.find("max-age=") != std::string::npos)
		{
			size_t pos = cache_control.find("max-age=") + 8;
			size_t end_pos = cache_control.find(',', pos);
			entry.max_age = std::chrono::seconds(std::stoi(cache_control.substr(pos, end_pos - pos)));
		}
		else
		{
			entry.max_age = std::chrono::seconds(0); // No caching if no max-age is specified
		}

		entry.timestamp = std::chrono::steady_clock::now();
		cache.insert_or_assign(key, std::move(entry)); // Store in cache
	}

	// Update an existing cache entry
	/*void ResponseCache::update(const std::string &key, const std::string &body, const HTTPHeaders &headers, const std::string &Etag)
	{
		lib_logger::LOG(lib_logger::LogLevel::TRACE,"");
		std::lock_guard<std::mutex> lock(mutex);
		if (cache.find(key) != cache.end())
		{
			put(key, std::move(body), headers, Etag); // Reuse the put method to update
		}
	}*/

	// Generate cache-related headers for response
	void ResponseCache::generate_cache_headers(HTTPHeaders &resp_headers, const CacheEntry &entry)
	{
		lib_logger::LOG(lib_logger::LogLevel::TRACE,"");
		resp_headers.add_header("ETag", entry.cached_Etag);
		resp_headers.add_header("Cache-Control", "max-age=" + std::to_string(entry.max_age.count()));
	}

	// Handle cache-related request headers to decide whether to serve from cache or revalidate
	bool ResponseCache::validate_cache_entry(const std::optional<CacheEntry>& entry, const HTTPHeaders &request_headers)
	{
		lib_logger::LOG(lib_logger::LogLevel::TRACE,"");
		// Check for ETag revalidation
		auto if_none_match = request_headers.get_header("If-None-Match");
		if (!if_none_match.empty() && if_none_match.front() == entry->cached_Etag)
		{
			return true; // Client's ETag matches; can serve from cache
		}

		// Check for Last-Modified revalidation (optional implementation)
		// Similar logic could be used for 'If-Modified-Since'

		return false; // Needs revalidation or cache miss
	}

	// Update an existing cache entry by appending to the body and headers
	/*void ResponseCache::update_and_append(const std::string &key, const std::string &additional_body, const HTTPHeaders &additional_headers, const std::string &Etag)
	{
		lib_logger::LOG(lib_logger::LogLevel::TRACE,"");
		std::lock_guard<std::mutex> lock(mutex);
		auto it = cache.find(key);
		if (it != cache.end())
		{
			// Append to the body
			*it->second.body += additional_body;

			// Add or update the selected headers based on the new headers
			for (const auto &headerName : additional_headers.get_all_header_pairs())
			{
				if (should_store_header(headerName.first))
				{
					it->second.selected_headers.add_header(headerName.first, headerName.second);
				}
			}

			// Update ETag and timestamp
			it->second.cached_Etag = Etag;
			it->second.timestamp = std::chrono::steady_clock::now();
		}
		else
		{
			// If no existing entry, create a new one
			put(key, additional_body, additional_headers, Etag);
		}
	}*/
}