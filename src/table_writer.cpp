#include <fstream>
#include <format>
#include <mutex>

// For streaming compressed files in and out
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/gzip.hpp>

#include <flint/flint.h>
#include <flint/gr.h>
#include <flint/fmpz.h>
#include <flint/fmpz_mpoly.h>
#include <flint/fmpz_mpoly_factor.h>
#include <flint/fmpz_poly.h>
#include <flint/fmpz_poly_factor.h>

#include "flint_interface.hpp"
#include "table_writer.hpp"


// #[ table_writer::table_writer
table_writer::table_writer(std::string filename_in, std::vector<std::string> vars_in,
	std::string lhs_in, std::string rhs_in)
	: filename(filename_in), ctx(vars_in.size()), var_names(vars_in),
		f_lhs(lhs_in), f_rhs(rhs_in)
{
	// Make sure var_names doesn't contain duplicate entries:
	std::vector var_names_dedup = var_names;
	std::sort(var_names_dedup.begin(), var_names_dedup.end());
	auto last_unique = std::unique(var_names_dedup.begin(), var_names_dedup.end());
	if (last_unique != var_names_dedup.end()) {
		throw std::runtime_error(
			std::format("{}::{}: invalid duplicate variable", class_name, __func__)
		);
	}

	// Create a var names copy with ep instead of d, and make vectors of mpoly
	// for both variable lists, which we need later.
	for ( size_t i = 0; i < var_names.size(); i++ ) {
		var_mpoly.emplace_back(var_names[i], var_names, ctx.d);
		if ( var_names[i] == "d" ) {
			d_var_index = i;
			var_names_ep.push_back("ep");
			// "4-2*d" is correct here: we are simply sending var->4-2*var, not
			// caring what it is called. The variable becomes "ep" in the final
			// conversion to a string representation. This way, there is no need
			// to make a higher variable-count context for flint.
			var_mpoly_ep.emplace_back("4-2*d", var_names, ctx.d);
		}
		else {
			var_names_ep.push_back(var_names[i]);
			var_mpoly_ep.emplace_back(var_names[i], var_names, ctx.d);
		}
	}
	// Make a vector of the variable mpoly pointers, for variable change with compose:
	for ( auto& mpp : var_mpoly_ep ) {
		var_mpoly_ep_pointers.push_back(mpp.d);
	}

	// Keep a copy of C string pointers for mpoly::to_string, we don't want to create it every call.
	for ( size_t i = 0; i < var_names_ep.size(); i++ ) {
		var_names_ep_c.push_back(var_names_ep[i].c_str());
	}

	// Make sure we found "d": for now it is required
	if (d_var_index == std::numeric_limits<std::size_t>::max()) {
		throw std::runtime_error(
			std::format("{}::{}: variable list does not contain 'd'", class_name, __func__)
		);
	}
}
// #]

// #[ table_writer::create_worker_tw

std::unique_ptr<table_writer> table_writer::create_worker_tw(uint32_t worker_number) {

	// Return a table_writer based on this one, with a worker-specific output filename.
	std::string worker_filename = filename;
	auto pos = worker_filename.find('#');
	if ( pos == std::string::npos ) {
		throw std::runtime_error(
			std::format("{}::{}: output filename contains no '#': {}", class_name, __func__, filename)
		);
	}
	worker_filename.replace(pos, 1, std::to_string(worker_number));

	auto wrt = std::make_unique<table_writer>(worker_filename, var_names, f_lhs, f_rhs);
	// The constructor does not open the output file, we do it explicitly:
	wrt->open_output_file();
	return wrt;
}
// #]

// #[ table_writer::open_output_file

void table_writer::open_output_file() {
	raw_out.open(filename, std::ios::binary);
	if ( ! raw_out.is_open() ) {
		throw std::runtime_error(
			std::format("{}::{}: unable to open file {}", class_name, __func__, filename)
		);
	}
	out.push(boost::iostreams::gzip_compressor());
	out.push(raw_out);
}
// #]

// #[ table_writer::write_form_fill

void table_writer::write_form_fill(const rule_t& rule) {

	// Create the whole output string in memory, and then finally write to the file.
	// Each thread should have its own writer object, so there is no need to lock for file access.

	std::string fill_str;
	fill_str = "Fill " + f_lhs + rule.lhs.head + "(" + rule.lhs.indices + ") =\n";
	for ( const auto& rhs : rule.rhs ) {
		fill_str += "\t+ " + f_rhs + rhs.mi.head + "(" + rhs.mi.indices + ")";
		coeff_t formatted_coeff = format_coeff(rhs.num, rhs.den);
		fill_str +=" * " + formatted_coeff.s + "\n";
	}
	fill_str += "\t;\n\n";

	out << fill_str;
}
// #]

// #[ table_writer::format_coeff
//
// Format an integral coefficient for the ouput. Replace d with 4-2*ep, and cancel
// any new gcd between num and den. Then produce the num and den strings.
coeff_t table_writer::format_coeff(const coeff_t& num_str, const coeff_t& den_str) {

	flint::mpoly num(num_str.s, var_names, ctx.d);
	flint::mpoly den(den_str.s, var_names, ctx.d);
	flint::mpoly numep(ctx.d);
	flint::mpoly denep(ctx.d);

	// Variable change d->ep:
	fmpz_mpoly_compose_fmpz_mpoly(numep.d, num.d, var_mpoly_ep_pointers.data(), ctx.d, ctx.d);
	fmpz_mpoly_compose_fmpz_mpoly(denep.d, den.d, var_mpoly_ep_pointers.data(), ctx.d, ctx.d);

	flint::mpoly gcd(ctx.d);
	// Divide out a possible gcd between num and den, after replacing d with 4-2*ep
	fmpz_mpoly_gcd_cofactors(gcd.d, numep.d, denep.d, numep.d, denep.d, ctx.d);

	// Create the output
	std::string res;
	if ( fmpz_mpoly_is_one(numep.d, ctx.d) ) {
		res = "1";
	}
	else {
		res = "numep(";
		flint::mpoly_univar numep_univar(ctx.d);
		fmpz_mpoly_to_univar(numep_univar.d, numep.d, d_var_index, ctx.d);
		const int64_t length = fmpz_mpoly_univar_length(numep_univar.d, ctx.d);

		for ( int64_t term = length-1; term >= 0; term-- ) {
			// Re-use num to store the coefficients:
			fmpz_mpoly_univar_get_term_coeff(num.d, numep_univar.d, term, ctx.d);
			const int64_t exponent = fmpz_mpoly_univar_get_term_exp_si(numep_univar.d, term, ctx.d);

			res += "+num(" + num.to_string(var_names_ep_c.data()) + ")";
			if ( exponent > 0 ) {
				res += std::string("*") + var_names_ep[d_var_index];
				if ( exponent > 1 ) {
					res += std::string("^") + std::to_string(exponent);
				}
			}
		}
		res += ")";
	}

	if ( fmpz_mpoly_is_one(denep.d, ctx.d) ) {
		res += "/1";
	}
	else {
		// Factor the new denominator:
		flint::mpoly_factor denep_fac(ctx.d);
		fmpz_mpoly_factor(denep_fac.d, denep.d, ctx.d);
		const int64_t num_factors = fmpz_mpoly_factor_length(denep_fac.d, ctx.d);

		flint::fmpz overall_constant;
		fmpz_mpoly_factor_get_constant_fmpz(overall_constant.d, denep_fac.d, ctx.d);
		if ( ! fmpz_is_one(overall_constant.d) ) {
			std::string overall_constant_str = overall_constant.to_string();
			res += "*den(";
			res += overall_constant_str;
			res += ")";
		}

		for ( int64_t i = 0; i < num_factors; i++ ) {
			const int64_t exponent = fmpz_mpoly_factor_get_exp_si(denep_fac.d, i, ctx.d);
			// Re-use the "den" mpoly to store the bases
			fmpz_mpoly_factor_get_base(den.d, denep_fac.d, i, ctx.d);
			std::string denep_fac_str = den.to_string(var_names_ep_c.data());

			// Check if the base contains "ep" (or "d") (the symbols don't have names
			// until we print them: only d_var_index actually matters here)
			const int64_t deg_ep = fmpz_mpoly_degree_si(den.d, d_var_index, ctx.d);

			if ( fmpz_mpoly_equal(den.d, var_mpoly[d_var_index].d, ctx.d) ) {
				res += "/";
				res += denep_fac_str;
				if ( exponent != 1 ) {
					res += "^";
					res += std::to_string(exponent);
				}
			}
			else {
				if ( deg_ep > 0 ) {
					res += "*denep(";
				}
				else {
					res += "*den(";
				}
				res += denep_fac_str;
				res += ")";
				if ( exponent != 1 ) {
					res += "^";
					res += std::to_string(exponent);
				}
			}
		}
	}

	return coeff_t(std::move(res));
}
// #]

