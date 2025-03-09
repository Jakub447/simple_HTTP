#ifndef RESPONSE_CACHE_HPP
#define RESPONSE_CACHE_HPP

#include <string>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <optional>
#include <memory>
#include "HttpHeaders.hpp"

namespace HTTP_Server
{
	// Cache entry structure
	struct CacheEntry
	{
		std::unique_ptr<std::string> body;
		HTTPHeaders selected_headers;
		std::chrono::time_point<std::chrono::steady_clock> timestamp;
		std::string cached_Etag;
		std::chrono::seconds max_age; // Duration until the cache entry is considered stale

		// Default constructor (sets body to nullptr)
		CacheEntry()
			: body(nullptr), max_age(0) // Initialize to default values
		{
		}

		// Move constructor
		CacheEntry(CacheEntry &&other) noexcept
			: body(std::move(other.body)),
			  selected_headers(std::move(other.selected_headers)),
			  timestamp(other.timestamp),
			  cached_Etag(std::move(other.cached_Etag)),
			  max_age(other.max_age)
		{
			// No need to move the unique_ptr as we are just moving data
		}

		// Move assignment operator
		CacheEntry &operator=(CacheEntry &&other) noexcept
		{
			if (this != &other)
			{
				body = std::move(other.body);
				selected_headers = std::move(other.selected_headers);
				timestamp = other.timestamp;
				cached_Etag = std::move(other.cached_Etag);
				max_age = other.max_age;
			}
			return *this;
		}

		// Disable copy constructor (explicitly delete it if not automatically deleted)
		CacheEntry(const CacheEntry &) = delete;
		CacheEntry &operator=(const CacheEntry &) = delete;
	};

	class ResponseCache
	{
	private:
		std::unordered_map<std::string, CacheEntry> cache;
		std::mutex mutex;

		// Check if a header should be stored in the cache according to RFC 9111
		bool should_store_header(const std::string &headerName);

	public:
		// Retrieve a cache entry based on key
		std::optional<CacheEntry> get(const std::string &key);

		// Store a new entry in the cache
		void put(const std::string &key, std::unique_ptr<std::string> &&body, const HTTPHeaders &headers, const std::string &Etag);

		// Update an existing cache entry
		//void update(const std::string &key, const std::string &body, const HTTPHeaders &headers, const std::string &Etag);

		// Update an existing cache entry by appending to the body and headers
		//void update_and_append(const std::string &key, const std::string &additional_body, const HTTPHeaders &additional_headers, const std::string &Etag);

		// Generate appropriate cache-related response headers
		void generate_cache_headers(HTTPHeaders &resp_headers, const CacheEntry &entry);

		// Handle cache-related request headers (e.g., If-None-Match, If-Modified-Since)
		bool validate_cache_entry(const std::optional<CacheEntry>& entry, const HTTPHeaders &request_headers);
	};
}

#endif // RESPONSE_CACHE_HP