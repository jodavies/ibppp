#ifndef FIRE_READER_H
#define FIRE_READER_H

#include <iostream>
#include <unordered_map>

#include "table_reader.hpp"
#include "table_writer.hpp"
#include "types.hpp"

// #[ class fire_reader
class fire_reader {

	private:

		using integral_id_map = std::unordered_map<std::string, integral_t>;

		rule_t     read_rule(std::istream&);
		rhs_t      read_rhs(std::istream&);
		integral_t read_integral(std::istream&);

		void begin_rules(std::istream&);
		bool more_rules(std::istream&);
		void end_rules(std::istream&);
		friend void table_reader::stream_rules(fire_reader&, const std::string&, table_writer&,
			const uint32_t);

		integral_id_map read_integral_id_map();

		std::string filename;
		integral_id_map id_map;

		const std::string class_name = "fire_reader";

	public:

		fire_reader(std::string);
		~fire_reader() = default;

		void stream_rules(table_writer&, uint32_t);
};

// #]

#endif
