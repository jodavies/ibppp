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

	public:

		kira_reader(std::string);
		~kira_reader() = default;

		void stream_rules(table_writer&, uint32_t);

};

// #]

#endif
