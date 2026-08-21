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

// #[ kira_reader::stream_rules
void kira_reader::stream_rules(table_writer& tw, uint32_t num_workers) {
	table_reader::stream_rules(*this, filename, tw, num_workers);
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

// #[ kira_reader::begin_rules
void kira_reader::begin_rules(std::istream& stream) {
	parse::expect_char(stream, '{');
}
// #]
// #[ kira_reader::more_rules
[[nodiscard]]
bool kira_reader::more_rules(std::istream& stream) {
	return parse::try_consume_char(stream, ',');
}
// #]
// #[ kira_reader::end_rules
void kira_reader::end_rules(std::istream& stream) {
	parse::expect_char(stream, '}');
}
// #]
