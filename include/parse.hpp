#ifndef PARSE_H
#define PARSE_H

#include <iostream>

// #[ namespace parse
namespace parse {

	[[nodiscard]]
	bool try_consume_char(std::istream& stream, const char consume) {
		stream >> std::ws;
		if ( stream.peek() != consume ) {
			return false;
		}
		stream.get();
		return true;
	}

	void expect_char(std::istream& stream, const char expected) {
		char c;
		stream >> std::ws >> c;
		if ( c != expected ) {
			throw std::runtime_error(
				std::format("expect_char: expected '{}', read '{}'", expected, c)
			);
		}
	}

	[[nodiscard]]
	std::string read_until(std::istream& stream, char until) {
		std::string str;
		std::getline(stream, str, until);
		return str;
	}

	void skip_nested_braces(std::istream& stream, const char open, const char close) {

		int level = 0;
		expect_char(stream, '{');
		level++;

		while ( level > 0 ) {
			char ch;
			stream >> std::ws >> ch;
			if ( ch == open ) level++;
			else if ( ch == close ) level--;
		}

	}

}

// #]

#endif
