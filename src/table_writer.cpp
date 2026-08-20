#include <fstream>
#include <format>

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
	std::string lhs_in, std::string rhs_in, bool trivial_coeff_in)
	: filename(filename_in), trivial_coeff(trivial_coeff_in), ctx(vars_in.size()),
		var_names(vars_in), f_lhs(lhs_in), f_rhs(rhs_in)
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
		else if ( var_names[i] == "ep" ) {
			throw std::runtime_error(
				std::format("{}::{}: variable list cannot contain 'ep'", class_name, __func__)
			);
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

	auto wrt = std::make_unique<table_writer>(worker_filename, var_names, f_lhs, f_rhs,
		trivial_coeff);
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
	if ( rule.rhs.empty() ) {
		// There are no rhs: the integral is 0.
		fill_str += "\t0\n";
	}
	else {
		for ( const auto& rhs : rule.rhs ) {
			fill_str += "\t+ " + f_rhs + rhs.mi.head + "(" + rhs.mi.indices + ")";
			coeff_t formatted_coeff = format_coeff(rhs.coeff);
			fill_str +=" * " + formatted_coeff.s + "\n";
		}
	}
	fill_str += "\t;\n\n";

	out << fill_str;
}
// #]

// #[ table_writer::format_coeff
//
// Format an integral coefficient for the output. Replace d with 4-2*ep, and cancel
// any new gcd between num and den. Then produce the num and den strings.
coeff_t table_writer::format_coeff(const coeff_t& integral_coeff) {

	flint::mpoly tmp(ctx.d);
	flint::mpoly numep(ctx.d);
	flint::mpoly denep(ctx.d);

	// Replace d with 4-2*ep in num and den, and divide out any resulting non-trivial gcd.
	// The resulting expressions are in numep and denep.
	const char* func = __func__;
	auto dtoep = [&](const fmpz_mpoly_t num, const fmpz_mpoly_t den) {
		if ( ! fmpz_mpoly_compose_fmpz_mpoly(numep.d, num, var_mpoly_ep_pointers.data(), ctx.d,
			ctx.d) ) {

			throw std::runtime_error(
				std::format("{}::{}: FLINT mpoly compose failed", class_name, func)
			);
		}
		if ( ! fmpz_mpoly_compose_fmpz_mpoly(denep.d, den, var_mpoly_ep_pointers.data(), ctx.d,
			ctx.d) ) {

			throw std::runtime_error(
				std::format("{}::{}: FLINT mpoly compose failed", class_name, func)
			);
		}
		fmpz_mpoly_gcd_cofactors(tmp.d, numep.d, denep.d, numep.d, denep.d, ctx.d);
	};

	if ( trivial_coeff ) {
		// Here we assume we can parse the coefficient string as "num/den" (from FIRE). Otherwise,
		// sending FIRE coefficients through the mpolyq parser is a ~10% performance regression.
		flint::mpoly num(ctx.d);
		flint::mpoly den(ctx.d);
		auto split = integral_coeff.s.find('/');
		if ( split == std::string::npos ) {
			num.set(integral_coeff.s, var_names);
			den.set("1", var_names);
		}
		else {
			num.set(integral_coeff.s.substr(0,split), var_names);
			auto check = integral_coeff.s.substr(split+1).find('/');
			if ( check != std::string::npos ) {
				throw std::runtime_error(
					std::format("{}::{}: extra '/' in int coeff: {}", class_name, __func__,
						integral_coeff.s)
				);
			}
			den.set(integral_coeff.s.substr(split+1), var_names);
		}
		dtoep(num.d, den.d);
	}
	else {
		// Here we parse more general rational polynomial expressions (from Kira).
		flint::mpolyq coeff(integral_coeff.s, var_names, ctx.d);
		dtoep(fmpz_mpoly_q_numref(coeff.d), fmpz_mpoly_q_denref(coeff.d));
	}


	// Create the output. We write the numerator as a sum of ep powers multiplied by, in general,
	// multivariate polynomial coefficients, stored in "num" functions to stop FORM immediately
	// multiplying them out. The whole numerator is wrapped in "numep" for the same reason.
	std::string res;
	if ( fmpz_mpoly_is_zero(numep.d, ctx.d) ) {
		// This should not happen!
		throw std::runtime_error(
			std::format("{}::{}: vanishing MI coefficient: {}", class_name, __func__, integral_coeff.s)
		);
	}
	else if ( fmpz_mpoly_is_one(numep.d, ctx.d) ) {
		res = "1";
	}
	else {
		res = "numep(";
		flint::mpoly_univar numep_univar(ctx.d);
		fmpz_mpoly_to_univar(numep_univar.d, numep.d, d_var_index, ctx.d);
		const int64_t length = fmpz_mpoly_univar_length(numep_univar.d, ctx.d);

		for ( int64_t term = length-1; term >= 0; term-- ) {
			fmpz_mpoly_univar_get_term_coeff(tmp.d, numep_univar.d, term, ctx.d);
			const int64_t exponent = fmpz_mpoly_univar_get_term_exp_si(numep_univar.d, term, ctx.d);

			res += "+num(";
			res += tmp.to_string(var_names_ep_c.data());
			res += ")";
			if ( exponent > 0 ) {
				res += std::string("*");
				res += var_names_ep[d_var_index];
				if ( exponent > 1 ) {
					res += std::string("^");
					res += std::to_string(exponent);
				}
			}
		}
		res += ")";
	}


	// We write the denominator as a product of factors:
	//  - the overall constant is written as den(overall constant), unless it is 1
	//  - poles in ep are written as 1/ep^n, so that FORM can easily discard ep powers within
	//    numep, depending on the power of the pole which appears
	//  - factors depending on ep are written as denep(ep+...)^n, to facilitate later series
	//    expansion in FORM
	//  - ep-independent factors are written as den(...)^n
	if ( fmpz_mpoly_is_one(denep.d, ctx.d) ) {
		res += "/1";
	}
	else {
		// Factor the new denominator:
		flint::mpoly_factor denep_fac(ctx.d);
		fmpz_mpoly_factor(denep_fac.d, denep.d, ctx.d);
		// Make sure the factor ordering is fixed, independent of FLINT version
		fmpz_mpoly_factor_sort(denep_fac.d, ctx.d);
		const int64_t num_factors = fmpz_mpoly_factor_length(denep_fac.d, ctx.d);

		flint::fmpz overall_constant;
		fmpz_mpoly_factor_get_constant_fmpz(overall_constant.d, denep_fac.d, ctx.d);
		if ( ! fmpz_is_one(overall_constant.d) ) {
			res += "*den(";
			res += overall_constant.to_string();
			res += ")";
		}

		for ( int64_t i = 0; i < num_factors; i++ ) {
			const int64_t exponent = fmpz_mpoly_factor_get_exp_si(denep_fac.d, i, ctx.d);
			fmpz_mpoly_factor_get_base(tmp.d, denep_fac.d, i, ctx.d);
			std::string denep_fac_str = tmp.to_string(var_names_ep_c.data());

			// Check if the base contains "ep" (or "d") (the symbols don't have names
			// until we print them: only d_var_index actually matters here)
			const int64_t deg_ep = fmpz_mpoly_degree_si(tmp.d, d_var_index, ctx.d);

			if ( fmpz_mpoly_equal(tmp.d, var_mpoly[d_var_index].d, ctx.d) ) {
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

