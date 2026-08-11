#ifndef FIRE_READER_H
#define FIRE_READER_H

#include <iostream>
#include <unordered_map>

#include "types.hpp"
#include "table_writer.hpp"

// #[ class fire_reader
class fire_reader {

	private:

		using integral_id_map = std::unordered_map<std::string, integral_t>;

		rule_t     read_rule(std::istream&);
		rhs_t      read_rhs(std::istream&);
		integral_t read_integral(std::istream&);

		integral_id_map read_integral_id_map();

		std::string filename;
		integral_id_map id_map;

		const std::string class_name = "fire_reader";

		std::string read_error_context(std::istream& stream) {
			const unsigned error_context = 140;
			char c;
			std::string err;
			err.reserve(error_context);
			while ( err.size() < error_context && stream.get(c) ) {
				if ( c == '\n' ) break;
				err += c;
			}
			if ( c != '\n' ) err += "...";
			return err;
		}

	public:

		fire_reader(std::string);
		~fire_reader() = default;

		void stream_rules(table_writer&, uint32_t);

};

// #]

#endif
