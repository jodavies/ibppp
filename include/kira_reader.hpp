#ifndef KIRA_READER_H
#define KIRA_READER_H

#include <iostream>

#include "table_reader.hpp"
#include "table_writer.hpp"
#include "types.hpp"

// #[ class kira_reader
class kira_reader {

	private:

		rule_t     read_rule(std::istream&);
		rhs_t      read_rhs(std::istream&);

		void begin_rules(std::istream&);
		bool more_rules(std::istream&);
		void end_rules(std::istream&);
		friend void table_reader::stream_rules(kira_reader&, const std::string&, table_writer&,
			const uint32_t);

		std::string filename;

		const std::string class_name = "kira_reader";

	public:

		kira_reader(std::string);
		~kira_reader() = default;

		void stream_rules(table_writer&, uint32_t);

};

// #]

#endif
