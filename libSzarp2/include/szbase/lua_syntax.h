#ifndef __LUA_SYNTAX_H__
#define __LUA_SYNTAX_H__

#include "config.h"

#ifdef LUA_PARAM_OPTIMISE

#include <string>
#include <vector>
#include <boost/variant.hpp>
#include <boost/optional.hpp>
#include <boost/tuple/tuple.hpp>

namespace lua_grammar {

	enum mul_op {
		MUL,
		DIV,
		REM
	};

	enum add_op {
		PLUS,
		MINUS,
	};

	enum cmp_op {
		LT,
		LTE,
		GT,
		GTE,
		EQ,
		NEQ
	};

	enum un_op {
		NEG,
		NOT,	
		LEN
	};

	struct nil { std::wstring nil_; nil() : nil_(L"nil") {} };
	struct threedots { std::wstring threedots_; threedots() : threedots_(L"threedots") {} };

	typedef std::wstring identifier;

	struct namelist {
		namelist& operator=(const namelist& v);
		namelist& operator=(const std::vector<std::wstring>& v);
		std::vector<std::wstring> namelist_;
	};

	struct parlist1 {
		namelist namelist_;
		boost::optional<threedots> threedots_;
	};

	typedef boost::variant<
		parlist1,
		threedots> parlist;

	struct chunk;

	struct block {
		block() {};
		block& operator=(const boost::recursive_wrapper<chunk>& c);
		boost::recursive_wrapper<chunk> chunk_;
	};

	struct expression;
	typedef boost::recursive_wrapper<expression> expf;

	typedef boost::variant<
		boost::recursive_wrapper<boost::tuple<expression, expression> >,
		boost::recursive_wrapper<boost::tuple<identifier, expression> >,
		expf> field;

	struct tableconstructor {
		std::vector<field> tableconstructor_;
		tableconstructor& operator=(const std::vector<field>& v);
		tableconstructor& operator=(const tableconstructor& v);
	};

	typedef boost::variant<
		boost::recursive_wrapper<std::vector<expression> >,
		tableconstructor,
		std::wstring> args;

	typedef boost::variant<
		expf,
		identifier> exp_identifier;

	typedef boost::variant<
		args,
		boost::tuple<identifier, args> > namearg;

	typedef boost::variant<
		exp_identifier,
		namearg> exp_ident_arg_namearg;

	typedef boost::variant<
		identifier, 
		boost::tuple<exp_identifier, std::vector<exp_ident_arg_namearg> > > postfixexp;
		
	struct var_seq {
		std::vector<namearg> nameargs;
		exp_identifier exp_identifier_;
	};

	struct varc {
		exp_identifier exp_identifier_;
		std::vector<var_seq> var_seqs;
	};

	typedef boost::variant<
		identifier,
		varc> var;

	struct funcbody {
		parlist parlist_;
		block block_;
	};

	typedef boost::variant<
		nil,
		bool,
		double,
		std::wstring,
		threedots,
		funcbody,
		postfixexp,
		tableconstructor,
		expf> term;

	typedef std::vector<term> pow_exp;

	typedef boost::tuple<std::vector<un_op>, pow_exp> unop_exp;

	typedef std::vector<boost::tuple<mul_op, unop_exp> > mul_list;
	struct mul_exp {
		unop_exp unop;
		mul_list muls;
	};

	typedef std::vector<boost::tuple<add_op, mul_exp> > add_list;
	struct add_exp {
		mul_exp mul;
		add_list adds;
	};

	typedef std::vector<add_exp> concat_exp;

	typedef std::vector<boost::tuple<cmp_op, concat_exp> > cmp_list;
	struct cmp_exp {
		concat_exp concat;
		cmp_list cmps;
	};

	typedef std::vector<cmp_exp> and_exp;

	typedef std::vector<and_exp> or_exp;

	struct expression {
		expression& operator=(const or_exp&);
		or_exp o;
	};

	struct while_loop {
		expression expression_;
		block block_;
	};

	struct repeat_loop {
		block block_;
		expression expression_;
	};

	struct assignment {
		std::vector<var> varlist;
		std::vector<expression> explist;
	};

	typedef std::vector<boost::tuple<expression, block> > elseif;

	struct if_stat {
		expression if_exp;
		block block_;
		elseif elseif_;
		boost::optional<block> else_;
	};

	struct for_from_to_loop {
		identifier identifier_;
		expression from;
		expression to;
		boost::optional<expression> step;
		block block_;
	};

	struct for_in_loop {
		namelist namelist_;
		std::vector<expression> expressions;
		block block_;
	};

	struct funcname {
		std::vector<identifier> identifiers;
		boost::optional<identifier> method_name;
	};

	struct function_declaration {
		funcname funcname_;
		funcbody funcbody_;
	};

	struct local_assignment : assignment {};

	struct local_function_declaration : public function_declaration { };

	struct return_ {
		return_() {}
		return_(std::vector<expression>& v) : expressions(v) {}
		std::vector<expression> expressions;
	};

	struct break_ { };

	typedef boost::variant<
		assignment,
		block,
		while_loop,
		repeat_loop,
		if_stat,
		for_in_loop,
		postfixexp,
		for_from_to_loop,
		function_declaration,
		local_assignment,
		local_function_declaration> stat;

	typedef boost::variant<
		return_,
		break_> laststat;

	struct chunk {
		chunk();
		chunk(const block&);
		std::vector<stat> stats;
		boost::optional<laststat> laststat_;	
	};


	bool parse(std::wstring::const_iterator& iter, std::wstring::const_iterator &end, chunk& chunk_);
};

#endif

#endif
