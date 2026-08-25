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
	std::string lhs_in, std::string rhs_in, bool trivial_coeff_in, bool ep_expand_in,
	int32_t ep_order_in)
	: filename(filename_in), trivial_coeff(trivial_coeff_in), ctx(vars_in.size()),
		var_names(vars_in), f_lhs(lhs_in), f_rhs(rhs_in), ep_expand(ep_expand_in),
		ep_order(ep_order_in)
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
		trivial_coeff, ep_expand, ep_order);
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
	if ( ep_expand ) {
		std::cout << class_name << ": [expand to ep^" << ep_order << "] form-fill: " << filename << std::endl;
	}
	else {
		std::cout << class_name << ": form-fill:" << filename << std::endl;
	}
}
// #]

// #[ table_writer::write_form_fill

void table_writer::write_form_fill(const rule_t& rule) {

	// Create the whole output string in memory, and then finally write to the file.
	// Each thread should have its own writer object, so there is no need to lock for file access.

	std::string fill_str;
	fill_str.reserve(1024);
	fill_str = "Fill " + f_lhs + rule.lhs.head + "(" + rule.lhs.indices + ") =\n";
	if ( rule.rhs.empty() ) {
		// There are no rhs: the integral is 0.
		fill_str += "\t0\n";
	}
	else {
		for ( const auto& rhs : rule.rhs ) {
			// format_coeff returns a vector. In ep-exact mode it will have a single entry
			// representing the coefficient. In ep-expansion mode it has an entry for each
			// ep power and its coefficient. We multiple each by the MI, so we can just loop:
			std::vector<std::string> formatted = format_coeff(rhs.coeff);
			for ( const auto& term : formatted ) {
				fill_str += "\t+ " + f_rhs + rhs.mi.head + "(" + rhs.mi.indices + ")" + " * ";
				fill_str += term;
				fill_str += "\n";
			}
		}
	}
	fill_str += "\t;\n\n";

	// Avoid formatted-output stream
	out.rdbuf()->sputn(fill_str.data(), fill_str.size());
}
// #]

// #[ table_writer::format_coeff
//
// Format an integral coefficient for the output. Replace d with 4-2*ep, and cancel
// any new gcd between num and den. Optionally, Laurent-expand around ep->0 to ep_order.
// The final formatted result is returned as a vector of strings, with two modes:
// 	- ep_expand false: exact coefficient string in a single vector entry
// 	- ep_expand true : ep coefficient strings each as a vector entry
std::vector<std::string> table_writer::format_coeff(const coeff_t& integral_coeff) {

	flint::mpoly tmp(ctx.d);
	flint::mpoly numep(ctx.d);
	flint::mpoly denep(ctx.d);

	// Replace d with 4-2*ep in num and den, and divide out any resulting non-trivial gcd.
	// The resulting expressions are in numep and denep.
	auto dtoep = [&](const fmpz_mpoly_t num, const fmpz_mpoly_t den) {
		flint::compose_one_var(numep.d, tmp.d, num, var_mpoly_ep_pointers[d_var_index], d_var_index,
			ctx.d);
		flint::compose_one_var(denep.d, tmp.d, den, var_mpoly_ep_pointers[d_var_index], d_var_index,
			ctx.d);
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


	if ( ep_expand ) {
		flint::mpolyq tmp(ctx.d);
		fmpz_mpoly_swap(numep.d, fmpz_mpoly_q_numref(tmp.d), ctx.d);
		fmpz_mpoly_swap(denep.d, fmpz_mpoly_q_denref(tmp.d), ctx.d);
		fmpz_mpoly_q_canonicalise(tmp.d, ctx.d);
		return format_coeff_ep_expand(tmp.d);
	}


	// Create the ep-exact output. We write the numerator as a sum of ep powers multiplied by, in
	// general, multivariate polynomial coefficients, stored in "num" functions to stop FORM
	// immediately multiplying them out. The whole numerator is wrapped in "numep" for the same
	// reason.
	std::vector<std::string> res;
	res.push_back("");
	if ( fmpz_mpoly_is_zero(numep.d, ctx.d) ) {
		// This should not happen!
		throw std::runtime_error(
			std::format("{}::{}: vanishing MI coefficient: {}", class_name, __func__, integral_coeff.s)
		);
	}
	else if ( fmpz_mpoly_is_one(numep.d, ctx.d) ) {
		res[0] = "1";
	}
	else {
		res[0] = "numep(";
		flint::mpoly_univar numep_univar(ctx.d);
		fmpz_mpoly_to_univar(numep_univar.d, numep.d, d_var_index, ctx.d);
		const int64_t length = fmpz_mpoly_univar_length(numep_univar.d, ctx.d);

		for ( int64_t term = length-1; term >= 0; term-- ) {
			fmpz_mpoly_univar_get_term_coeff(tmp.d, numep_univar.d, term, ctx.d);
			const int64_t exponent = fmpz_mpoly_univar_get_term_exp_si(numep_univar.d, term, ctx.d);

			res[0] += "+num(";
			res[0] += tmp.to_string(var_names_ep_c.data());
			res[0] += ")";
			if ( exponent > 0 ) {
				res[0] += std::string("*");
				res[0] += var_names_ep[d_var_index];
				if ( exponent > 1 ) {
					res[0] += std::string("^");
					res[0] += std::to_string(exponent);
				}
			}
		}
		res[0] += ")";
	}


	// We write the denominator as a product of factors:
	//  - the overall constant is written as den(overall constant), unless it is 1
	//  - poles in ep are written as 1/ep^n, so that FORM can easily discard ep powers within
	//    numep, depending on the power of the pole which appears
	//  - factors depending on ep are written as denep(ep+...)^n, to facilitate later series
	//    expansion in FORM
	//  - ep-independent factors are written as den(...)^n
	if ( fmpz_mpoly_is_one(denep.d, ctx.d) ) {
		res[0] += "/1";
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
			res[0] += "*den(";
			res[0] += overall_constant.to_string();
			res[0] += ")";
		}

		for ( int64_t i = 0; i < num_factors; i++ ) {
			const int64_t exponent = fmpz_mpoly_factor_get_exp_si(denep_fac.d, i, ctx.d);
			fmpz_mpoly_factor_get_base(tmp.d, denep_fac.d, i, ctx.d);
			std::string denep_fac_str = tmp.to_string(var_names_ep_c.data());

			// Check if the base contains "ep" (or "d") (the symbols don't have names
			// until we print them: only d_var_index actually matters here)
			const int64_t deg_ep = fmpz_mpoly_degree_si(tmp.d, d_var_index, ctx.d);

			if ( fmpz_mpoly_equal(tmp.d, var_mpoly[d_var_index].d, ctx.d) ) {
				res[0] += "/";
				res[0] += denep_fac_str;
				if ( exponent != 1 ) {
					res[0] += "^";
					res[0] += std::to_string(exponent);
				}
			}
			else {
				if ( deg_ep > 0 ) {
					res[0] += "*denep(";
				}
				else {
					res[0] += "*den(";
				}
				res[0] += denep_fac_str;
				res[0] += ")";
				if ( exponent != 1 ) {
					res[0] += "^";
					res[0] += std::to_string(exponent);
				}
			}
		}
	}

	return res;
}
// #]

// #[ table_writer::format_coeff_ep_expand
//
// Expand the coefficient in ep, and then produce a vector formatted strings for the coefficients.
// In the context, ep is at d_var_index.
std::vector<std::string> table_writer::format_coeff_ep_expand(const fmpz_mpoly_q_t coeff) {

	fmpz_mpoly_q_t *series_coeffs;
	int64_t leading_exponent;

	// Expand in ep. The function allocates series_coeffs. We'll need to free it properly.
	flint::fmpz_mpoly_q_laurent_series(&series_coeffs, &leading_exponent, coeff, d_var_index,
		ep_order, ctx.d);
	// The number of terms in the result depends on how deeply we expanded, and what (possibly
	// negative) the leading power of the expansion is:
	const int64_t n_terms = ep_order - leading_exponent + 1;

	std::vector<std::string> res;

	flint::mpoly tmp(ctx.d);
	std::string str;
	for ( int i = 0; i < n_terms; i++ ) {
		if ( fmpz_mpoly_q_is_zero(series_coeffs[i], ctx.d) ) {
			continue;
		}
		str  = "ep^";
		str += std::to_string(leading_exponent + i);
		fmpz_mpoly_swap(tmp.d, fmpz_mpoly_q_numref(series_coeffs[i]), ctx.d);
		if ( fmpz_mpoly_is_one(tmp.d, ctx.d) ) {
			str += " * 1";
		}
		else {
			str += " * num(";
			str += tmp.to_string(var_names_ep_c.data());
			str += ")";
		}
		fmpz_mpoly_swap(tmp.d, fmpz_mpoly_q_denref(series_coeffs[i]), ctx.d);
		if ( ! fmpz_mpoly_is_one(tmp.d, ctx.d) ) {
			str += "*den(";
			str += tmp.to_string(var_names_ep_c.data());
			str += ")";
		}
		res.push_back(std::move(str));
	}
	str  = "ep^";
	str += std::to_string(ep_order+1);
	str += " * warnep";
	res.push_back(std::move(str));

	// Clean up
	for ( int i = 0; i < n_terms; i++ ) {
		fmpz_mpoly_q_clear(series_coeffs[i], ctx.d);
	}
	flint_free(series_coeffs);

	return res;
}

// #]

