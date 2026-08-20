#include <algorithm>
#include <cctype>
#include <format>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>

#include "fire_reader.hpp"
#include "kira_reader.hpp"
#include "table_writer.hpp"


int main(int argc, char* argv[]) {

	try {
		boost::program_options::options_description desc("Allowed options");
		desc.add_options()
			("help,h", "Print usage options.")
			("cpus", boost::program_options::value<int>()->default_value(1),
				"Number of formatting worker threads.")
			("fire-table", boost::program_options::value<std::string>(),
				"Relative path of gzip-compressed FIRE table.")
			("kira-table", boost::program_options::value<std::string>(),
				"Relative path of gzip-compressed Kira table.")
			("form-fill", boost::program_options::value<std::string>(),
				"Relative path of FORM Fill output files; one '#' in the name is mandatory, and "
				"replaced with the worker id.")
			("f-lhs", boost::program_options::value<std::string>()->default_value(""),
				"Function name for LHS integrals; for --fire-table the topo id number from the table "
				"is appended, for --kira-table the whole function head from the table is appended. "
				"An empty value \"\" is accepted (and is the default).")
			("f-rhs", boost::program_options::value<std::string>()->default_value(""),
				"Function name for RHS integrals, see also --f-lhs.")
			("vars", boost::program_options::value<std::string>(),
				"Comma-separated list of variable names, which must contain d, and must not contain "
				"ep.")
		;
		boost::program_options::variables_map vm;
		boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc), vm);
		boost::program_options::notify(vm);

		if ( vm.count("help") ) {
			std::cout << desc << std::endl;
			return 0;
		}

		const auto cpus = vm.at("cpus").as<int>();
		if ( cpus <= 0 ) {
			throw std::runtime_error(
				std::format("invalid number of cpus: {}", cpus)
			);
		}

		std::vector<std::string> vars;
		if ( vm.count("vars") ) {
			std::stringstream vars_stream(vm.at("vars").as<std::string>());
			std::string new_var;
			while (std::getline(vars_stream, new_var, ',')) {
				boost::algorithm::trim(new_var);
				if ( new_var.empty() ||
					std::any_of(new_var.begin(), new_var.end(), [](unsigned char c) {
						return std::isspace(c); }) ) {
					throw std::runtime_error(
						std::format("invalid variable: \"{}\" in \"{}\"", new_var,
							vm.at("vars").as<std::string>())
					);
				}
				vars.push_back(new_var);
			}
		}
		else {
			throw std::runtime_error("no variables specified");
		}

		if ( vm.count("fire-table") == 0 && vm.count("kira-table") == 0 ) {
			throw std::runtime_error("no input file specified");
		}
		if ( vm.count("fire-table") != 0 && vm.count("kira-table") != 0 ) {
			throw std::runtime_error("both kira and fire input files specified");
		}
		const auto table = vm.count("fire-table") != 0 ? vm.at("fire-table").as<std::string>()
			: vm.at("kira-table").as<std::string>();

		if ( vm.count("form-fill") == 0 ) {
			throw std::runtime_error("no output file specified");
		}
		const auto form_fill = vm.at("form-fill").as<std::string>();
		if ( std::count(form_fill.begin(), form_fill.end(), '#') != 1 ) {
			throw std::runtime_error(
				std::format("output file name does not contain one '#': {}", form_fill)
			);
		}

		const auto lhs = vm.at("f-lhs").as<std::string>();
		const auto rhs = vm.at("f-rhs").as<std::string>();

		if ( vm.count("fire-table") != 0 ) {
			fire_reader fr(table);
			table_writer tw(form_fill, vars, lhs, rhs, true);
			fr.stream_rules(tw, cpus);
		}
		else {
			kira_reader kr(table);
			table_writer tw(form_fill, vars, lhs, rhs, false);
			kr.stream_rules(tw, cpus);
		}
	}

	catch ( const boost::program_options::error& err ) {
		std::cerr << "ibppp: option error: " << err.what() << std::endl;
		return 1;
	}

	catch ( const std::exception& err ) {
		std::cerr << "ibppp: error: " << err.what() << std::endl;
		return 1;
	}

	return 0;
}

