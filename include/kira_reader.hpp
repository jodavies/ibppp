#ifndef KIRA_READER_H
#define KIRA_READER_H

#include <iostream>

#include "types.hpp"
#include "table_writer.hpp"

// #[ class kira_reader
class kira_reader {

	private:

		rule_t     read_rule(std::istream&);
		rhs_t      read_rhs(std::istream&);

		std::string filename;

		const std::string class_name = "kira_reader";

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

		kira_reader(std::string);
		~kira_reader() = default;

		void stream_rules(table_writer&, uint32_t);

};

// #]

#endif
