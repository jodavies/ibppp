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

		// For error reporting: read the stream until the next line break or
		// error_context characters, whichever happens first. We don't want
		// malformed input to cause huge amounts of data to be printed in
		// the error message.
		std::string read_error_context(std::istream& stream) {
			const unsigned error_context = 140;
			char c = '\0';
			bool got_c = false;
			std::string err;
			err.reserve(error_context);
			while ( err.size() < error_context && stream.get(c) ) {
				got_c = true;
				if ( c == '\n' ) break;
				err += c;
			}
			if ( got_c && c != '\n' ) err += "...";
			return err;
		}

	public:

		fire_reader(std::string);
		~fire_reader() = default;

		void stream_rules(table_writer&, uint32_t);

};

// #]

#endif
