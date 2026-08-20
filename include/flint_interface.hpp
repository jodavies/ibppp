#ifndef FLINTINTERFACE_H
#define FLINTINTERFACE_H

#include <iostream>
#include <format>

#include <flint/flint.h>
#include <flint/gr.h>
#include <flint/fmpz.h>
#include <flint/fmpz_mpoly.h>
#include <flint/fmpz_mpoly_factor.h>
#include <flint/fmpz_mpoly_q.h>
#include <flint/fmpz_poly.h>
#include <flint/fmpz_poly_factor.h>

// #[ flint wrappers: modified from FORM flintinterface.h
namespace flint {

	using std::string;
	using std::cout;
	using std::endl;

	// Small wrappers around the flint structs to enable RAII init and clear.
	// "d" represents the data, and this public member will be passed to flint functions.
	// Delete copy and move constructors to avoid accidental multiple-references to flint data.
	// We don't (currently) pass or return these objects between interface functions.

	class fmpz {
		public:
			fmpz_t d;
			fmpz() { fmpz_init(d); }
			~fmpz() noexcept { fmpz_clear(d); }

			fmpz(const fmpz&) = delete;
			fmpz& operator=(const fmpz&) = delete;
			fmpz(fmpz&&) = delete;
			fmpz& operator=(fmpz&&) = delete;

			void print(const string& text) const { cout << text; fmpz_print(d); cout << endl; }
			[[nodiscard]] std::string to_string() {
				char *str = fmpz_get_str(NULL, 10, d);
				std::string res(str);
				flint_free(str);
				return res;
			}
	};

	class poly {
		public:
			fmpz_poly_t d;
			poly() { fmpz_poly_init(d); }
			~poly() noexcept { fmpz_poly_clear(d); }

			poly(const poly&) = delete;
			poly& operator=(const poly&) = delete;
			poly(poly&&) = delete;
			poly& operator=(poly&&) = delete;

			void print(const string& text) const {
				cout << text; fmpz_poly_print_pretty(d, "x"); cout << endl;
			}
			void print(const string& text, const char* var) {
				cout << text; fmpz_poly_print_pretty(d, var); cout << endl;
			}
	};

	class poly_factor {
		public:
			fmpz_poly_factor_t d;
			poly_factor() { fmpz_poly_factor_init(d); }
			~poly_factor() noexcept { fmpz_poly_factor_clear(d); }

			poly_factor(const poly_factor&) = delete;
			poly_factor& operator=(const poly_factor&) = delete;
			poly_factor(poly_factor&&) = delete;
			poly_factor& operator=(poly_factor&&) = delete;
	};

	class mpoly {
		private:
			fmpz_mpoly_ctx_struct *ctx; // We need to keep a copy of the context pointer for clearing.
		public:
			fmpz_mpoly_t d;
			explicit mpoly(fmpz_mpoly_ctx_struct *ctx_in) : ctx(ctx_in) { fmpz_mpoly_init(d, ctx); }
			explicit mpoly(const std::string& str, std::vector<std::string>& vars,
				fmpz_mpoly_ctx_struct *ctx_in) : ctx(ctx_in) {
				fmpz_mpoly_init(d, ctx);
				std::vector<const char*> vars_c;
				for ( const auto& v: vars ) {
					vars_c.push_back(v.c_str());
				}
				if ( fmpz_mpoly_set_str_pretty(d, str.c_str(), vars_c.data(), ctx) ) {
					throw std::runtime_error(
						std::format("{}: fmpz_mpoly parse error: {}", __func__, str)
					);
				}
			}
			~mpoly() noexcept { fmpz_mpoly_clear(d, ctx); }

			mpoly(const mpoly&) = delete;
			mpoly& operator=(const mpoly&) = delete;
			mpoly(mpoly&& other) noexcept : ctx(other.ctx) {
				fmpz_mpoly_init(d, ctx);
				fmpz_mpoly_swap(d, other.d, ctx);
			}
			mpoly& operator=(mpoly&& other) {
				if (this != &other) {
					if (ctx != other.ctx) {
						throw std::runtime_error(
							std::format("{}: context mismatch", __func__)
						);
					}
					fmpz_mpoly_swap(d, other.d, ctx);
				}
				return *this;
			}

			void set(const string& str, std::vector<std::string>& vars) {
				std::vector<const char*> vars_c;
				for ( const auto& v: vars ) {
					vars_c.push_back(v.c_str());
				}
				if ( fmpz_mpoly_set_str_pretty(d, str.c_str(), vars_c.data(), ctx) ) {
					throw std::runtime_error(
						std::format("{}: fmpz_mpoly parse error: {}", __func__, str)
					);
				}
			}

			void print(const string& text) const {
				cout << text;
				fmpz_mpoly_print_pretty(d, 0, ctx);
				cout << endl;
			}
			void print(const string& text, const char** vars) {
				cout << text;
				fmpz_mpoly_print_pretty(d, vars, ctx);
				cout << endl;
			}
			[[nodiscard]] std::string to_string(const char** vars) {
				char *str = fmpz_mpoly_get_str_pretty(d, vars, ctx);
				std::string res(str);
				flint_free(str);
				return res;
			}
	};

	class mpolyq {
		private:
			fmpz_mpoly_ctx_struct *ctx; // We need to keep a copy of the context pointer for clearing.
		public:
			fmpz_mpoly_q_t d;
			explicit mpolyq(fmpz_mpoly_ctx_struct *ctx_in) : ctx(ctx_in) { fmpz_mpoly_q_init(d, ctx); }
			explicit mpolyq(const std::string& str, std::vector<std::string>& vars,
				fmpz_mpoly_ctx_struct *ctx_in) : ctx(ctx_in) {
				fmpz_mpoly_q_init(d, ctx);
				std::vector<const char*> vars_c;
				for ( const auto& v: vars ) {
					vars_c.push_back(v.c_str());
				}
				if ( fmpz_mpoly_q_set_str_pretty(d, str.c_str(), vars_c.data(), ctx) ) {
					throw std::runtime_error(
						std::format("{}: fmpz_mpoly_q parse error: {}", __func__, str)
					);
				}
			}
			~mpolyq() noexcept { fmpz_mpoly_q_clear(d, ctx); }

			mpolyq(const mpolyq&) = delete;
			mpolyq& operator=(const mpolyq&) = delete;
			mpolyq(mpolyq&& other) noexcept : ctx(other.ctx) {
				fmpz_mpoly_q_init(d, ctx);
				fmpz_mpoly_q_swap(d, other.d, ctx);
			}
			mpolyq& operator=(mpolyq&& other) {
				if (this != &other) {
					if (ctx != other.ctx) {
						throw std::runtime_error(
							std::format("{}: context mismatch", __func__)
						);
					}
					fmpz_mpoly_q_swap(d, other.d, ctx);
				}
				return *this;
			}

			void print(const string& text) const {
				cout << text;
				fmpz_mpoly_q_print_pretty(d, 0, ctx);
				cout << endl;
			}
			void print(const string& text, const char** vars) {
				cout << text;
				fmpz_mpoly_q_print_pretty(d, vars, ctx);
				cout << endl;
			}
			[[nodiscard]] std::string to_string(const char** vars) {
				char *str = fmpz_mpoly_q_get_str_pretty(d, vars, ctx);
				std::string res(str);
				flint_free(str);
				return res;
			}
	};

	class mpoly_factor {
		private:
			fmpz_mpoly_ctx_struct *ctx; // We need to keep a copy of the context pointer for clearing.
		public:
			fmpz_mpoly_factor_t d;
			explicit mpoly_factor(fmpz_mpoly_ctx_struct *ctx_in) : ctx(ctx_in) {
				fmpz_mpoly_factor_init(d, ctx);
			}
			~mpoly_factor() noexcept { fmpz_mpoly_factor_clear(d, ctx); }

			mpoly_factor(const mpoly_factor&) = delete;
			mpoly_factor& operator=(const mpoly_factor&) = delete;
			mpoly_factor(mpoly_factor&&) = delete;
			mpoly_factor& operator=(mpoly_factor&&) = delete;
	};

	class mpoly_ctx {
		public:
			fmpz_mpoly_ctx_t d;
			explicit mpoly_ctx(int64_t nvars) { fmpz_mpoly_ctx_init(d, nvars, ORD_LEX); }
			~mpoly_ctx() noexcept { fmpz_mpoly_ctx_clear(d); }

			mpoly_ctx(const mpoly_ctx&) = delete;
			mpoly_ctx& operator=(const mpoly_ctx&) = delete;
			mpoly_ctx(mpoly_ctx&&) = delete;
			mpoly_ctx& operator=(mpoly_ctx&&) = delete;
	};

	class mpoly_univar {
		private:
			fmpz_mpoly_ctx_struct *ctx; // We need to keep a copy of the context pointer for clearing.
		public:
			fmpz_mpoly_univar_t d;
			explicit mpoly_univar(fmpz_mpoly_ctx_struct *ctx_in) : ctx(ctx_in) {
				fmpz_mpoly_univar_init(d, ctx);
			}
			~mpoly_univar() noexcept { fmpz_mpoly_univar_clear(d, ctx); }
	};

	// When we replace d with "4-2*d" (and later print the d as ep) we can use the built-in
	// fmpz_mpoly_compose_fmpz_mpoly. However we have a specific replacement: d is replaced
	// by a polynomial only in d, and the other variables are not touched.
	// We can do a bit better than the more general fmpz_mpoly_compose_fmpz_mpoly by manually
	// constructing the result: take all of the coefficients of d with fmpz_mpoly_univar_t
	// and multiply out the Horner representation with d -> 4-2*d.
	// This gives a ~10% runtime improvement.
	// So here: f is the polynomial, g is the replacement of "d", var is the index of "d" in
	// the context. The result is returned in "res" and "t" is temporary storage, passed from
	// the caller.
	inline void compose_one_var(fmpz_mpoly_t res, fmpz_mpoly_t t, const fmpz_mpoly_t f,
		const fmpz_mpoly_t g, const slong var, const fmpz_mpoly_ctx_t ctx) {

		fmpz_mpoly_univar_t uf;
		fmpz_mpoly_univar_init(uf, ctx);

		fmpz_mpoly_to_univar(uf, f, var, ctx);
		const slong n = fmpz_mpoly_univar_length(uf, ctx);

		if ( n == 0 ) {
			fmpz_mpoly_zero(res, ctx);
		}
		else {
			// Put the coefficient of the largest power of var in "res", and store its exponent:
			fmpz_mpoly_univar_get_term_coeff(res, uf, 0, ctx);
			slong exp_prev = fmpz_mpoly_univar_get_term_exp_si(uf, 0, ctx);

			// Multiply out the Horner representation, accounting for possible gaps between the
			// current and previous exponent:
			for ( slong i = 1; i < n; i++ ) {
				const slong exp = fmpz_mpoly_univar_get_term_exp_si(uf, i, ctx);
				const slong gap = exp_prev - exp;
				if ( gap == 1 ) {
					// Just multiply by g:
					fmpz_mpoly_mul(res, res, g, ctx);
				}
				else {
					// Multiply by g^gap:
					fmpz_mpoly_pow_ui(t, g, gap, ctx);
					fmpz_mpoly_mul(res, res, t, ctx);
				}

				// Now put the coefficient of the next power of var in "t", and add it to "res":
				fmpz_mpoly_univar_get_term_coeff(t, uf, i, ctx);
				fmpz_mpoly_add(res, res, t, ctx);

				exp_prev = exp;
			}

			// We are done, unless the lowest power of var is > 0:
			const slong exp = fmpz_mpoly_univar_get_term_exp_si(uf, n-1, ctx);
			if ( exp != 0 ) {
				fmpz_mpoly_pow_ui(t, g, exp, ctx);
				fmpz_mpoly_mul(res, res, t, ctx);
			}

			fmpz_mpoly_set(res, res, ctx);
		}

		fmpz_mpoly_univar_clear(uf, ctx);
	}

};

// #]

#endif
