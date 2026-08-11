#ifndef TABLE_WRITER_H
#define TABLE_WRITER_H

#include <fstream>
#include <mutex>

// For streaming compressed files in and out
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/gzip.hpp>

#include "flint_interface.hpp"
#include "types.hpp"

// #[ class table_writer
class table_writer {

	private:
		std::ofstream raw_out;
		boost::iostreams::filtering_stream<boost::iostreams::output> out;
		std::mutex out_lock;
		std::string filename;

		coeff_t format_num(const coeff_t&);
		coeff_t format_den(const coeff_t&);

		// Global flint context and mpolys which we use for variable changing,
		// which we must make sure to only read!
		flint::mpoly_ctx ctx;
		std::vector<std::string> var_names;
		std::vector<flint::mpoly> var_mpoly;
		std::vector<std::string> var_names_ep;
		std::vector<const char*> var_names_ep_c;
		std::vector<flint::mpoly> var_mpoly_ep;
		std::vector<fmpz_mpoly_struct*> var_mpoly_ep_pointers;
		size_t d_var_index = std::numeric_limits<std::size_t>::max();

		std::string f_lhs, f_rhs;

		const std::string class_name = "table_writer";

	public:
		table_writer(std::string, std::vector<std::string>, std::string, std::string);
		void write_form_fill(const rule_t&);
};
// #]

#endif
