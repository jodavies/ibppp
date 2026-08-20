#include <iostream>
#include <fstream>
#include <format>
#include <unordered_map>
#include <chrono>
#include <thread>

// For streaming compressed files in and out
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/filter/gzip.hpp>

#include "kira_reader.hpp"
#include "types.hpp"
#include "parse.hpp"
#include "blocking_queue.hpp"
#include "table_writer.hpp"

// #[ kira_reader::kira_reader
kira_reader::kira_reader(std::string filename_in)
	: filename(filename_in)
{
	std::cout << class_name << ": " << filename << std::endl;
}
// #]

// #[ kira_reader::read_rule
//
// A (kira2math format) Kira table "rule" has the format:
// 	topo[indices] ->
// 	 + topo[indices]*((num)/(den))
// 	 + topo[indices]*((num)/trivialden)
// 	 + topo[indices]*(trivialnum/(den))
// 	 + topo[indices]*(trivialnum/trivialden)
// 	 + topo[indices]*(num)
// 	 ...
// or
// 	 + 0
// and terminates with a ",".
// The num and den may themselves contain "/", if the output is from FireFly.
// In addition, if the output is from FireFly, num and den polynomial terms
// have rational coefficients.
// We just read the whole coefficient into a string, and rely on FLINT for parsing.
//
// Read a rule from "stream" and return it.

rule_t kira_reader::read_rule(std::istream& stream) {

	rule_t rule;

	// Read the lhs integral and the "->"
	parse::consume_ws(stream);
	rule.lhs.head = parse::read_until(stream, '[');
	rule.lhs.indices = parse::read_until(stream, ']');
	parse::expect_char(stream, '-');
	parse::expect_char(stream, '>');

	// Now follow the rhs integrals and coefficients.
	// Each rhs integral is on a single line which starts with "+".
	// If the lhs integral vanishes, we'll just have "+ 0".
	// Pick up the first '+':
	parse::expect_char(stream, '+');
	do {
		if ( parse::try_consume_char(stream, '0') ) {
			// The lhs integral vanishes. We don't expect to have already read any rhs!
			if ( rule.rhs.size() != 0 ) {
				throw std::runtime_error(
					std::format("{}::{}: found \"+ 0\" on non-trivial rhs of {}[{}]",
						class_name, __func__, rule.lhs.head, rule.lhs.indices)
				);
			}
			break;
		}
		rule.rhs.push_back(read_rhs(stream));
	} while ( parse::try_consume_char(stream, '+') );

	return rule;
}
// #]
// #[ kira_reader::read_rhs
//
// A Kira table "rhs" has the format:
// 	 + topo[indices]*coefficient
//
// We should have already consumed the '+' in the caller.
//
// Read a rhs from "stream" and return it.

rhs_t kira_reader::read_rhs(std::istream& stream) {

	rhs_t rhs;

	rhs.mi.head = parse::read_until(stream, '[');
	rhs.mi.indices = parse::read_until(stream, ']');

	parse::expect_char(stream, '*');

	// The coefficient is now everything until EOL:
	rhs.coeff.s = parse::read_until(stream, '\n');
	return rhs;
}
// #]

// #[ kira_reader::stream_rules
//

void kira_reader::stream_rules(table_writer& tw, uint32_t num_workers) {

	if ( num_workers == 0 ) {
		throw std::invalid_argument("num_workers must be > 0");
	}

	std::chrono::time_point<std::chrono::steady_clock> start_time = std::chrono::steady_clock::now();

	// Set up the stream:
	std::ifstream raw_stream(filename, std::ios::binary);
	if ( ! raw_stream.is_open() ) {
		throw std::runtime_error(
			std::format("{}::{}: unable to open file {}", class_name, __func__, filename)
		);
	}
	boost::iostreams::filtering_streambuf<boost::iostreams::input> streambuf;
	streambuf.push(boost::iostreams::gzip_decompressor(), 16*1024);
	streambuf.push(raw_stream);
	std::istream stream(&streambuf);

	// Move to the start of the rules:
	parse::expect_char(stream, '{');

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
			rule_t rule = read_rule(stream);
			if ( ! queue.push(std::move(rule)) ) {
				break;
			}
			total_rules++;
		} while ( parse::try_consume_char(stream, ',') );
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

	// Read the final closing } of the list of rules
	parse::expect_char(stream, '}');

	std::chrono::duration<double> elapsed_time = std::chrono::steady_clock::now() - start_time;
	// Print some runtime statistics, including the time and (gzip compressed) processing rate
	std::cout << class_name << ": " << __func__ << " [" << num_workers << " workers]: processed "
		<< total_rules << " rules in " << std::fixed << std::setprecision(2) << elapsed_time.count()
		<< "s [" << std::setprecision(1) << total_rules/1000.0/elapsed_time.count() << " kr/s, "
		<< raw_stream.tellg()/1024.0/1024.0/elapsed_time.count() << " MiB/s]" << std::endl;
}

// #]

