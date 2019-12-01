/**
 * \file pp.h
 *
 * Preprocessor utilities.
 *
 * Macros defined in this library occupy the PP_* namespace. This file also
 * reserves the IPP_* namespace for internal helper macros.
 *
 * Many of the definitions in this file are adapted from
 * [this repository](https://github.com/pfultz2/Cloak/wiki/C-Preprocessor-tricks,-tips,-and-idioms).
 */

#ifndef GOLF_PP_H
#define GOLF_PP_H

/**
 * \brief Concatenate two tokens.
 *
 * Both arguments are eagerly expanded.
 *
 * \pre
 * The result of pasting the expansion of `x` and the expansion of `y` must form
 * a valid token.
 */
#define PP_CAT(x, y) IPP_CAT(x, y)

/**
 * \brief Construct a string literal from a token or sequence of tokens.
 *
 * The argument `x` is eagerly expanded, and the expansion is turned into a
 * string literal.
 */
#define PP_STRING(x) IPP_STRING(x)

/**
 * \brief Conditionally expand based on a boolean.
 *
 * The arguments `c`, `t`, and `f` are eagerly expanded. If `c` expands to `1`,
 * then the entire macro `PP_IF(c, t, f)` expands to the expansion of `t`.
 * Otherwise, the result is the expansion of `f`.
 *
 * \pre
 * `c` must expand to either the literal `0` or the literal `1`.
 */
#define PP_IF(c, t, f) PP_CAT(IPP_IF, c)(t, f)

/**
 * \brief Negate a boolean.
 *
 * If `b` expands to `0`, the result is `1`. Otherwise, the result is `0`.
 *
 * \pre
 * `b` must expand to either `0` or `1`.
 */
#define PP_NOT(b) PP_CAT(IPP_NOT, b)

/**
 * \brief Take the logical "and" of two booleans.
 *
 * If both `x` and `y` expand to `1`, the result is `1`. Otherwise, the result
 * is `0`.
 *
 * \pre
 * `x` and `y` must each expand to the literal `0` or `1`.
 */
#define PP_AND(x, y) PP_CAT(IPP_AND, x)(y)

/**
 * \brief Identity function.
 *
 * Expands to the expansion of the argument.
 */
#define PP_ID(x) x

/**
 * \brief Constant function.
 *
 * The macro expression `PP_CONST(x)(...)` expands to the expansion of `x`,
 * regardless of what arguments are passed in the outer parentheses.
 */
#define PP_CONST(x) x IPP_EAT

/**
 * \brief Generate a compile-time error with a message containing `err`.
 *
 * `err` may be any token or sequence of tokens.
 *
 * \note
 * This macro only generates an error if its result is included in the generated
 * code. If its result is discarded (for example, by
 * `PP_IF(0, PP_ERROR(err), 1))`) then no error will be raised.
 *
 * \par
 * These semantics prevent some of the nicer compile-time error mechanisms from
 * being used to implement this macro. For example, something like
 * `_Pragma("GCC error msg")` would allow very nicely formatted error messages.
 * However, this pragma generates an error message as soon as it is expanded,
 * even if it is eventually discarded. Since macros like `PP_IF` eagerly
 * evaluate their arguments, this is unsuitable for non-trivial, conditional
 * errors.
 */
#define PP_ERROR(err) _Static_assert(0, PP_STRING(err))
    // We implement PP_ERROR by generating a _Static_assert which always fails,
    // and which contains the error message we want. The typical usage of this
    // macro is as an expression, so this will probably also produce an error
    // message complaining about a missing semicolon (since _Static_assert is a
    // declaration) and could lead to even messier output depending on where it
    // is used. Empirically, though, the compiler will usually manage to include
    // the failed static assert somewhere in the output.

/**
 * \brief Compare two tokens.
 *
 * If `x` and `y` expand to identical tokens, `PP_TOKEN_EQ(x, y)` expands to
 * `1`. Otherwise, it expands to `0`.
 *
 * \pre
 * Both `x` and `y` must expand to comparable tokens. A token `t` is comparable
 * if there is a macro definition of the form `#define PP_COMPARE_t(x) x` in
 * scope.
 */
#define PP_TOKEN_EQ(x, y) \
    PP_IF(IPP_IS_COMPARABLE(x), \
        PP_IF(IPP_IS_COMPARABLE(y), \
            IPP_COMPARE(x, y), \
                /* If both `x` and `y` are comparable, call the internal token
                 * comparison function (which assumes its arguments are
                 * comparable).
                 */\
\
        /* Otherwise, fail with a helpful error message. */\
            PP_ERROR(the token y is not comparable)), \
        PP_ERROR(PP_ERROR(the token x is not comparable)))

/**
 * \brief Determine if a macro expands to the empty token sequence.
 *
 * If `x` expands to an empty token sequence, `PP_IS_EMPTY(x)` expands to `1`.
 * Otherwise, it expands to `0`.
 */
#define PP_IS_EMPTY(x) IPP_CHECK(PP_CAT(IPP_IS_EMPTY_PROBE, x)())
    // `IPP_IS_EMPTY_PROBE()` expands to `~, 1,`, which yields `1` when passed
    // to `IPP_CHECK`. `IPP_IS_EMPTY_PROBEx()`, where `x` is non-empty, expands
    // to itself, since the only macro we define which starts with
    // `IPP_IS_EMPTY_PROBE` is `IPP_IS_EMPTY_PROBE`. This is a single argument,
    // and so `IPP_CHECK`, will yield `0`.

////////////////////////////////////////////////////////////////////////////////
// Internal helpers
//
#ifndef DOXYGEN

////////////////////////////////////////////////////////////////////////////////
// Token fiddling
//
#define IPP_CAT(x, y) x ## y
    // `PP_CAT` helper. Having this level of indirection (as opposed to defining
    // `PP_CAT` itself as `x ## y`) ensures that `x` and `y` are fully expanded
    // before we paste them.

#define IPP_STRING(x) #x
    // `PP_STRING` helper. As with `PP_CAT`, having a helper macro invoke
    // operator `#` ensures that the argument is fully expanded before it is
    // stringized.

#define IPP_EAT(...)
    // Consume a token.

////////////////////////////////////////////////////////////////////////////////
// Logic
//

// IF pattern matching.
#define IPP_IF0(t, f) f
#define IPP_IF1(t, f) t

// NOT pattern matching.
#define IPP_NOT0 1
#define IPP_NOT1 0

// AND pattern matching.
#define IPP_AND0(y) 0
#define IPP_AND1(y) y

////////////////////////////////////////////////////////////////////////////////
// Detection
//

#define IPP_CHECK(...) IPP_CHECK_N(__VA_ARGS__, 0,)
    // If given exactly one argument `x`, `IPP_CHECK(x)` expands to `0`. If
    // given more than one argument, the result is the second argument.
#define IPP_CHECK_N(x, n, ...) n
    // Helper for IPP_CHECK.

#define IPP_IS_PAREN(x) IPP_CHECK(IPP_IS_PAREN_PROBE x)
    // Expands to `1` if `x` expands to `()`, and `0` otherwise.
#define IPP_IS_PAREN_PROBE(...) ~, 1,
    // If `x` is `()`, then `IPP_IS_PAREN(x)` expands to
    // `IPP_CHECK(IPP_IS_PAREN_PROBE())` which further expands to
    // `IPP_CHECK(~, 1,)`. Since `IPP_CHECK` is given more than one argument, it
    // expands to the second argument `1`.
    //
    // If `x` is not `()`, then `IPP_IS_PAREN_PROBE` is not expanded, so the
    // whole thing evaluates to `IPP_CHECK(IPP_IS_PAREN_PROBE x)`. Since
    // `IPP_CHECK` is given a single argument (the token sequence
    // `IPP_IS_PAREN_PROBE x`) it expands to `0`.

#define IPP_IS_EMPTY_PROBE(...) ~, 1,
    // Helper for `PP_IS_EMPTY`.

////////////////////////////////////////////////////////////////////////////////
// Comparison
//
#define IPP_COMPARE(x, y) \
    PP_NOT(IPP_IS_PAREN(PP_CAT(PP_COMPARE_, x)(PP_CAT(PP_COMPARE_,y))(())))
        // If `x` and `y` are different tokens, then
        // `PP_COMPARE_x(PP_COMPARE_y(()))` will fully expand to `()`, and so
        // `IPP_IS_PAREN` will return `1`.
        //
        // But if `y` is the same token as `x`, then we have
        // `PP_COMPARE_x(PP_COMPARE_x(()))`. The preprocessor will disable
        // expansion of `PP_COMPARE_x` after expanding it the first time (it
        // does this to prevent recursive macro expansion) and so the whole
        // thing will expand to `PP_COMPARE_x(())`, which is not identical to
        // `()`, so `IPP_IS_PAREN` will return `0`.

#define IPP_IS_COMPARABLE(x) IPP_IS_PAREN(PP_CAT(PP_COMPARE_, x)(()))
    // Expands to `1` if the macro `PP_COMPARE_x(y)` is defined as `y`, and `0`
    // otherwise.
    //
    // If this macro is properly defined, then `PP_COMPARE_x(())` should expand
    // to `()`, and so `IPP_IS_PAREN` will return `1`. Otherwise, `IPP_IS_PAREN`
    // will return `0`.

#endif // DOXYGEN

#endif
