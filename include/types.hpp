#ifndef TYPES_H
#define TYPES_H

#include <string>
#include <vector>

// #[ global types:
struct integral_t {
	std::string head;    // a function name or topology number
	std::string indices; // the list of propagator indices
};

struct coeff_t {
	std::string s;       // just a wrapper around a string
};

struct rhs_t {
	integral_t mi;       // a "rhs" is an integral and a coefficient
	coeff_t num;
	coeff_t den;
};

struct rule_t {
	integral_t lhs;      // a "rule" is a lhs integral, and a linear combination of rhs
	std::vector<rhs_t> rhs;
};

struct table_t {
	std::vector<rule_t> rule;
};

// #]

#endif
