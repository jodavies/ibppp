#include <iostream>
#include <fstream>
#include <format>
#include <unordered_map>
#include <chrono>
#include <thread>

// For streaming compressed files in and out
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/filter/gzip.hpp>

#include "table_reader.hpp"
#include "fire_reader.hpp"
#include "types.hpp"
#include "parse.hpp"
#include "blocking_queue.hpp"
#include "table_writer.hpp"

// #[ fire_reader::fire_reader
fire_reader::fire_reader(std::string filename_in)
	: filename(filename_in)
{
	std::cout << class_name << ": " << filename << std::endl;
	id_map = read_integral_id_map();
}
// #]

// #[ fire_reader::stream_rules
void fire_reader::stream_rules(table_writer& tw, uint32_t num_workers) {
	table_reader::stream_rules(*this, filename, tw, num_workers);
}
// #]

// #[ fire_reader::read_integral_id_map
//
// Get the integral_id_map only, without storing anything else. Unfortunately the map comes
// at the end of the file, so we have to run through the whole thing and find its start.
// It is not great for performance, but there is no choice.

fire_reader::integral_id_map fire_reader::read_integral_id_map() {

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

	// Open a list level, putting us at the start of the "rules" list. Then skip over it.
	parse::expect_char(stream, '{');
	parse::skip_nested_braces(stream, '{', '}');

	// The id map follows:
	parse::expect_char(stream, ',');
	parse::expect_char(stream, '{');

	// Next comes the integral id to integral indices map.
	// It is a list of entries with the format:
	// 	{intID,{topoNumber,{index1,index2,...}}}

	integral_id_map new_map;

	do {
		parse::expect_char(stream, '{');
		std::string new_id = parse::read_until(stream, ',');

		integral_t new_integral = read_integral(stream);
		parse::expect_char(stream, '}');

		auto [it, inserted] = new_map.emplace(std::move(new_id), std::move(new_integral));
		if ( ! inserted ) {
			throw std::runtime_error(
				std::format("{}::{}: duplicate integral id: {}", class_name, __func__, it->first)
			);
		}

	} while ( parse::try_consume_char(stream, ',') );

	// Finally there should be two more '}' and then the end of the stream:
	parse::expect_char(stream, '}');
	parse::expect_char(stream, '}');
	stream >> std::ws;
	if ( ! stream.eof() ) {
		std::string con = parse::read_error_context(stream);
		throw std::runtime_error(
			std::format("{}::{}: unexpected extra stream content: {}", class_name, __func__, con)
		);
	}

	std::chrono::duration<double> elapsed_time = std::chrono::steady_clock::now() - start_time;
	std::cout << class_name << ": integral_id_map constructed in: " << std::fixed
		<< std::setprecision(3) << elapsed_time.count() << "s" << std::endl;

	return new_map;
}
// #]
// #[ fire_reader::read_rule
//
// A FIRE table "rule" has the format:
// 	{lhs integral id,
// 		{
// 			{rhs integral 1 id, "coefficient"},
// 			{rhs integral 2 id, "coefficient"},
// 			...
// 			{rhs integral n id, "coefficient"}
// 		}
// 	}
//
// Read a rule from "stream" and return it.

rule_t fire_reader::read_rule(std::istream& stream) {

	if ( ! parse::try_consume_char(stream, '{') ) {
		std::string con = parse::read_error_context(stream);
		throw std::runtime_error(
			std::format("{}::{}: invalid rule start: {}", class_name, __func__, con)
		);
	}

	std::string id = parse::read_until(stream, ',');
	rule_t rule;
	auto it = id_map.find(id);
	if ( it == id_map.end() ) {
		throw std::runtime_error(
			std::format("{}::{}: unknown lhs integral id: {}", class_name, __func__, id)
		);
	}
	rule.lhs = it->second;

	// Now comes a list of rhs integrals and their coefficients.
	// If an integral is zero, we'll find "{}" and so have no rhs integrals.
	parse::expect_char(stream, '{');
	if ( ! parse::try_consume_char(stream, '}') ) {
		do {
			rule.rhs.push_back(read_rhs(stream));
		} while ( parse::try_consume_char(stream, ',') );
		parse::expect_char(stream, '}');
	}

	// Close the rule:
	parse::expect_char(stream, '}');

	return rule;
}
// #]
// #[ fire_reader::read_rhs
//
// A FIRE table "rhs" has the format:
// 	{rhs integral id, "coefficient"}
//
// Read a rhs from "stream" and return it.

rhs_t fire_reader::read_rhs(std::istream& stream) {

	if ( ! parse::try_consume_char(stream, '{') ) {
		std::string con = parse::read_error_context(stream);
		throw std::runtime_error(
			std::format("{}::{}: invalid rhs start: {}", class_name, __func__, con)
		);
	}

	rhs_t rhs;
	std::string id = parse::read_until(stream, ',');
	auto it = id_map.find(id);
	if ( it == id_map.end() ) {
		throw std::runtime_error(
			std::format("{}::{}: unknown rhs integral id: {}", class_name, __func__, id)
		);
	}
	rhs.mi = it->second;

	// Now read the coefficient.
	parse::expect_char(stream, '"');
	rhs.coeff.s = parse::read_until(stream, '"');

	// Close the rhs:
	parse::expect_char(stream, '}');

	return rhs;
}
// #]
// #[ fire_reader::read_integral

integral_t fire_reader::read_integral(std::istream& stream) {

	if ( ! parse::try_consume_char(stream, '{') ) {
		std::string con = parse::read_error_context(stream);
		throw std::runtime_error(
			std::format("{}::{}: invalid integral start: {}", class_name, __func__, con)
		);
	}

	std::string head = parse::read_until(stream, ',');
	parse::expect_char(stream, '{');
	std::string indices = parse::read_until(stream, '}');
	parse::expect_char(stream, '}');

	return integral_t(head, indices);
}
// #]

// #[ fire_reader::begin_rules
void fire_reader::begin_rules(std::istream& stream) {
	parse::expect_char(stream, '{');
	parse::expect_char(stream, '{');
}
// #]
// #[ fire_reader::more_rules
[[nodiscard]]
bool fire_reader::more_rules(std::istream& stream) {
	return parse::try_consume_char(stream, ',');
}
// #]
// #[ fire_reader::end_rules
void fire_reader::end_rules(std::istream& stream) {
	parse::expect_char(stream, '}');
	// The id_map follows the rules.
	parse::expect_char(stream, ',');
	parse::expect_char(stream, '{');
}
// #]
