#ifndef PARSE_H
#define PARSE_H

#include <iostream>

// #[ namespace parse
namespace parse {

	[[nodiscard]]
	inline bool try_consume_char(std::istream& stream, const char consume) {
		stream >> std::ws;
		if ( stream.peek() != consume ) {
			return false;
		}
		stream.get();
		return true;
	}

	inline void consume_ws(std::istream& stream) {
		stream >> std::ws;
	}

	inline void expect_char(std::istream& stream, const char expected) {
		char c;
		if ( ! (stream >> std::ws >> c) ) {
			throw std::runtime_error(
				std::format("{}: expected '{}', but reached end of stream", __func__, expected)
			);
		}
		if ( c != expected ) {
			throw std::runtime_error(
				std::format("{}: expected '{}', read '{}'", __func__, expected, c)
			);
		}
	}

	[[nodiscard]]
	inline std::string read_until(std::istream& stream, char until) {
		std::string str;
		if ( ! std::getline(stream, str, until) ) {
			throw std::runtime_error(
				std::format("{}: reached end of stream before {}", __func__, until)
			);
		}
		return str;
	}

	// Note that this is not aware of strings. Open/close chars inside the strings are
	// included in the brace-level counting. Since the strings in valid FIRE tables
	// will always have matching opening and closing braces, this is OK.
	inline void skip_nested_braces(std::istream& stream, const char open, const char close) {
		int level = 0;
		expect_char(stream, open);
		level++;
		// Using sbumpc here greatly improves performance
		auto* buf = stream.rdbuf();
		while ( level > 0 ) {
			const int c = buf->sbumpc();
			if ( c == std::char_traits<char>::eof() ) {
				throw std::runtime_error(
					std::format("{}: reached end of stream inside open {}/{} braces", __func__,
						open, close)
				);
			}
			if ( c == open ) level++;
			else if ( c == close ) level--;
		}
	}

}

// #]

#endif
