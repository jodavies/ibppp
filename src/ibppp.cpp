#include <iostream>

#include <boost/program_options.hpp>

#include "fire_reader.hpp"
#include "table_writer.hpp"

using namespace std;


int main(int argc, char* argv[]) {

	boost::program_options::options_description desc("Allowed options");
	desc.add_options()
		("help", "Print usage options")
		("cpus", boost::program_options::value<unsigned>()->default_value(1),
			"Number of formatting threads")
		("fire-table", boost::program_options::value<std::string>(),
			"Relative path of gzip-compressed fire table")
		("form-fill", boost::program_options::value<std::string>(),
			"Relative path of FORM Fill output file")
		("f-lhs", boost::program_options::value<std::string>()->default_value("f"),
			"Function name for LHS integrals")
		("f-rhs", boost::program_options::value<std::string>()->default_value("f"),
			"Function name for RHS integrals")
		("vars", boost::program_options::value<std::string>(),
			"Comma-separated list of variable names")
	;
	boost::program_options::variables_map vm;
	boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc), vm);
	boost::program_options::notify(vm);

	if (vm.count("help")) {
		std::cout << desc << std::endl;
		return 0;
	}

	if (vm.count("cpus")) {
		if (vm.at("cpus").as<unsigned>() == 0) {
			vm.at("cpus").value() = 1;
		}
		else if (vm.at("cpus").as<unsigned>() > 256) {
			// "ridiculous" ?
			vm.at("cpus").value() = 256;
		}
	}

	std::vector<std::string> vars;
	if (vm.count("vars")) {
		std::stringstream vars_stream(vm.at("vars").as<std::string>());
		std::string new_var;
		while (std::getline(vars_stream, new_var, ',')) {
			vars.push_back(new_var);
		}
	}
	else {
		throw std::runtime_error("no variables specified");
	}

	if (vm.count("fire-table") == 0) {
		throw std::runtime_error("no input file specified");
	}
	if (vm.count("form-fill") == 0) {
		throw std::runtime_error("no output file specified");
	}


	fire_reader fr(vm.at("fire-table").as<std::string>());
	table_writer tw(vm.at("form-fill").as<std::string>(), vars,
		vm.at("f-lhs").as<std::string>(), vm.at("f-rhs").as<std::string>());
	fr.stream_rules(tw, vm.at("cpus").as<unsigned>());

	return 0;
}

