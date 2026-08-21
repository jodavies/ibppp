#ifndef TABLE_READER_H
#define TABLE_READER_H

#include <iomanip>
#include <thread>

#include "blocking_queue.hpp"
#include "parse.hpp"
#include "table_writer.hpp"
#include "types.hpp"


namespace table_reader {

// #[ stream_rules
	template <typename Reader>
	void stream_rules(Reader& reader, const std::string& filename, table_writer& tw,
		const uint32_t num_workers) {

		if ( num_workers == 0 ) {
			throw std::invalid_argument(
				std::format("{}: num_workers ({}) must be > 0", __func__, num_workers)
			);
		}

		auto start_time = std::chrono::steady_clock::now();

		// Set up the stream:
		std::ifstream raw_stream(filename, std::ios::binary);
		if ( ! raw_stream.is_open() ) {
			throw std::runtime_error(
				std::format("{}: unable to open file {}", __func__, filename)
			);
		}
		boost::iostreams::filtering_streambuf<boost::iostreams::input> streambuf;
		streambuf.push(boost::iostreams::gzip_decompressor(), 16*1024);
		streambuf.push(raw_stream);
		std::istream stream(&streambuf);

		// Move the (format-specific) stream to the start of the rules
		reader.begin_rules(stream);

		blocking_queue<rule_t> queue(4*num_workers);
		// Start writer threads and wait for input to arrive in the queue:
		std::vector<std::jthread> workers;
		std::exception_ptr worker_error;
		std::mutex worker_error_mutex;
		workers.reserve(num_workers);
		for ( uint32_t i = 0; i < num_workers; i++ ) {
			workers.emplace_back([&,i] {
				try {
					{
						auto worker_tw = tw.create_worker_tw(i);
						rule_t rule;
						while ( queue.pop(rule) ) {
							worker_tw->write_form_fill(rule);
						}
					}
					// Now that the worker_tw have left scope (destroying the FLINT members)
					// clean up the FLINT caches.
					flint_cleanup();
				}
				catch (...) {
					{
						std::lock_guard lock(worker_error_mutex);
						if ( ! worker_error ) {
							worker_error = std::current_exception();
						}
					}
					queue.abort();
				}
			});
		}

		// The main thread reads the stream and fills the queue:
		uint64_t total_rules = 0;
		try {
			do {
				rule_t rule = reader.read_rule(stream);
				if ( ! queue.push(std::move(rule)) ) {
					break;
				}
				total_rules++;
			} while ( reader.more_rules(stream) );
		}
		catch (...) {
			queue.close();
			workers.clear();
			throw;
		}
		queue.close();
		workers.clear();
		if ( worker_error ) {
			std::rethrow_exception(worker_error);
		}

		// Verify that we've reached the end of the rules as expected
		reader.end_rules(stream);

		auto elapsed_time = std::chrono::steady_clock::now() - start_time;

		// Print some runtime statistics, including the time and (gzip compressed) processing rate
		std::cout << __func__ << ": [" << num_workers << " workers]:"
			<< " processed " << total_rules << " rules in "
			<< std::fixed << std::setprecision(2) << elapsed_time.count() << "s"
			<< std::setprecision(1)
			<< " ["
			<< total_rules/1000.0/elapsed_time.count() << " kr/s"
			<< ", "
			<< raw_stream.tellg()/1024.0/1024.0/elapsed_time.count() << " MiB/s"
			<< "]"
			<< std::endl;
	}
// #]

}

#endif
