/* 
  SZARP: SCADA software 
  

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
*/

#include "szarp_base_common/lua_param_optimizer.h"

#ifdef LUA_PARAM_OPTIMISE

//#define LUA_OPTIMIZER_DEBUG

#ifdef LUA_OPTIMIZER_DEBUG
#include <fstream>
std::ofstream lua_opt_debug_stream("/tmp/lua_optimizer_debug");
#endif

namespace LuaExec {

ParamRef::ParamRef() {
	m_exec_engine = NULL;
}

void ParamRef::PushExecutionEngine(ExecutionEngine *exec_engine) {
	m_exec_engines.push_back(m_exec_engine);
	m_exec_engine = exec_engine;
}

void ParamRef::PopExecutionEngine() {
	assert(m_exec_engines.size());
	m_exec_engine = m_exec_engines.back();
	m_exec_engines.pop_back();
}

Var::Var(size_t var_no) : m_var_no(var_no), m_ee(NULL) {}

void Var::PushExecutionEngine(ExecutionEngine *exec_engine) {
	m_exec_engines.push_back(m_ee);
	m_ee = exec_engine;
}

void Var::PopExecutionEngine() {
	assert(m_exec_engines.size());
	m_ee = m_exec_engines.back();
	m_exec_engines.pop_back();
}


template<> Val BinExpression<std::logical_or<Val> >::Value() {
	return m_e1->Value() || m_e2->Value();
}

template<> Val BinExpression<std::logical_and<Val> >::Value() {
	return m_e1->Value() && m_e2->Value();
}

template<> Val BinExpression<std::modulus<Val> >::Value() {
	return int(m_e1->Value()) % int(m_e2->Value());
}

Val& Var::operator()() {
	return m_ee->Vars()[m_var_no];
}

Val& Var::operator=(const Val& val) {
	return m_ee->Vars()[m_var_no] = val;
}

void StatementList::AddStatement(PStatement statement) {
	m_statements.push_back(statement);
}

void StatementList::Execute() {
	size_t l = m_statements.size();
	for (size_t i = 0; i < l; i++)
		m_statements[i]->Execute();
}

PStatement StatementConverter::operator() (const assignment &a) {
#ifdef LUA_OPTIMIZER_DEBUG
	lua_opt_debug_stream << "Converting assignement" << std::endl;
#endif
	boost::shared_ptr<StatementList> ret = boost::make_shared<StatementList>();
	for (size_t i = 0; i < a.varlist.size(); i++) {
		try {
			const identifier& identifier_ = boost::get<identifier>(a.varlist[i]);
#ifdef LUA_OPTIMIZER_DEBUG
			lua_opt_debug_stream << "Identifier: " << SC::S2A(identifier_) << std::endl;
#endif
			VarRef variable = m_param_converter->GetGlobalVar(identifier_);
			PExpression expression;
			if (i < a.explist.size())
				expression = m_param_converter->ConvertExpression(a.explist[i]);
			else
				expression = boost::make_shared<NilExpression>();
			ret->AddStatement(boost::make_shared<AssignmentStatement>(variable, expression));
		} catch (boost::bad_get&) {
			throw ParamConversionException(L"Only assignment to variables allowed in optimized expressions");
		}
	}
	return ret;
}

PStatement StatementConverter::operator() (const block &b) {
#ifdef LUA_OPTIMIZER_DEBUG
	lua_opt_debug_stream <<  "Coverting block" << std::endl;
#endif
	return m_param_converter->ConvertBlock(b);
}

PStatement StatementConverter::operator() (const while_loop &w) {
#ifdef LUA_OPTIMIZER_DEBUG
	lua_opt_debug_stream << "Converting while loop" << std::endl;
#endif
	PExpression condition = m_param_converter->ConvertExpression(w.expression_);
	PStatement block = m_param_converter->ConvertBlock(w.block_);
	return boost::make_shared<WhileStatement>(condition, block);
}

PStatement StatementConverter::operator() (const repeat_loop &r) {
#ifdef LUA_OPTIMIZER_DEBUG
	lua_opt_debug_stream << "Converting while loop" << std::endl;
#endif
	PExpression condition = m_param_converter->ConvertExpression(r.expression_);
	PStatement block = m_param_converter->ConvertBlock(r.block_);
	return boost::make_shared<RepeatStatement>(condition, block);
}

PStatement StatementConverter::operator() (const if_stat &if_) {
#ifdef LUA_OPTIMIZER_DEBUG
	lua_opt_debug_stream << "Converting if statement" << std::endl;
	lua_opt_debug_stream << "Converting if expression" << std::endl;
#endif
	PExpression cond = m_param_converter->ConvertExpression(if_.if_exp);
#ifdef LUA_OPTIMIZER_DEBUG
	lua_opt_debug_stream << "Converting if block" << std::endl;
#endif
	PStatement block = m_param_converter->ConvertBlock(if_.block_);
	std::vector<std::pair<PExpression, PStatement> > elseif;
	for (size_t i = 0; i < if_.elseif_.size(); i++) {
#ifdef LUA_OPTIMIZER_DEBUG
		lua_opt_debug_stream << "Converting elseif" << std::endl;
#endif
		elseif.push_back(std::make_pair(m_param_converter->ConvertExpression(if_.elseif_[i].get<0>()),
				m_param_converter->ConvertBlock(if_.elseif_[i].get<1>())));
	}
	PStatement else_;
	if (if_.else_) {
#ifdef LUA_OPTIMIZER_DEBUG
		lua_opt_debug_stream << "Converting else" << std::endl;
#endif
		else_ = m_param_converter->ConvertBlock(*if_.else_);
	} else
		else_ = boost::make_shared<EmptyStatement>();
	return boost::make_shared<IfStatement>(cond, block, elseif, else_);
}

PStatement StatementConverter::operator() (const for_in_loop &a) {
	throw ParamConversionException(L"For in loops not supported by converter");
}

PStatement StatementConverter::operator() (const postfixexp &a) {
	throw ParamConversionException(L"Postfix expressions as statments not supported by converter");
}

PStatement StatementConverter::operator() (const for_from_to_loop &for_) {
#ifdef LUA_OPTIMIZER_DEBUG
	lua_opt_debug_stream <<  "Converting for from to loop" << std::endl;
#endif
	VarRef var = m_param_converter->GetLocalVar(for_.identifier_);
	PExpression from = m_param_converter->ConvertExpression(for_.from);
	PExpression to = m_param_converter->ConvertExpression(for_.to);
	PExpression step;
	if (for_.step)
		step = m_param_converter->ConvertExpression(*for_.step);
	else
		step = boost::make_shared<NumberExpression>(1);
	PStatement block = m_param_converter->ConvertBlock(for_.block_);
	return boost::make_shared<ForLoopStatement>(var, from, to, step, block);
}

PStatement StatementConverter::operator() (const function_declaration &a) {
	throw ParamConversionException(L"Function declarations not supported by converter");
}

PStatement StatementConverter::operator() (const local_assignment &a) {
#ifdef LUA_OPTIMIZER_DEBUG
	lua_opt_debug_stream << "Converting local assignment" << std::endl;
#endif
	boost::shared_ptr<StatementList> ret = boost::make_shared<StatementList>();
	for (size_t i = 0; i < a.varlist.size(); i++) {
		try {
			const identifier& identifier_ = boost::get<identifier>(a.varlist[i]);
#ifdef LUA_OPTIMIZER_DEBUG
			lua_opt_debug_stream << "Identifier: " << SC::S2A(identifier_) << std::endl;
#endif
			VarRef variable = m_param_converter->GetLocalVar(identifier_);
			PExpression expression;
			if (i < a.explist.size())
				expression = m_param_converter->ConvertExpression(a.explist[i]);
			else
				expression = boost::make_shared<NilExpression>();
			ret->AddStatement(boost::make_shared<AssignmentStatement>(variable, expression));
		} catch (boost::bad_get&) {
			throw ParamConversionException(L"Only assignment to variables allowed in optimized expressions");
		}
	}
	return ret;
}

PStatement StatementConverter::operator() (const local_function_declaration &f) {
	throw ParamConversionException(L"Function declarations not supported by converter");
}

PExpression ExpressionConverter::ConvertTerm(const term& term_) {
#ifdef LUA_OPTIMIZER_DEBUG
	lua_opt_debug_stream << "Converting term" << std::endl;
#endif
	return m_param_converter->ConvertTerm(term_);
}

struct pow_functor : public std::binary_function<const Val&, const Val&, Val> {
	Val operator() (const Val& s1, const Val& s2) const {
		return pow(s1, s2);
	}
};

PExpression ExpressionConverter::ConvertPow(const pow_exp& exp) {
	pow_exp::const_reverse_iterator i = exp.rbegin();
	PExpression p = ConvertTerm(*i);
	while (++i != exp.rend()) {
#ifdef LUA_OPTIMIZER_DEBUG
		lua_opt_debug_stream << "Converting power expression" << std::endl;
#endif
		PExpression p2 = ConvertTerm(*i);
		p = boost::make_shared<BinExpression<pow_functor> >(p2, p);
	}
	return p;
}

PExpression ExpressionConverter::ConvertUnOp(const unop_exp& unop) {
	PExpression p(ConvertPow(unop.get<1>()));
	const std::vector<un_op>& v = unop.get<0>();
	for (std::vector<un_op>::const_reverse_iterator i = v.rbegin();
			i != v.rend();
			i++)
		switch (*i) {
			case NEG:
#ifdef LUA_OPTIMIZER_DEBUG
				lua_opt_debug_stream << "Converting negate unop expression" << std::endl;
#endif
				p = boost::make_shared<UnExpression<std::negate<Val> > >(p);
				break;
			case NOT:
#ifdef LUA_OPTIMIZER_DEBUG
				lua_opt_debug_stream << "Converting not unop expression" << std::endl;
#endif
				p = boost::make_shared<UnExpression<std::logical_not<Val> > >(p);
				break;
			case LEN:
				throw ParamConversionException(L"Opeartor '#' not supported in optimized params");
				break;
		}
	return p;
}

PExpression ExpressionConverter::ConvertMul(const mul_exp& mul_) {
	PExpression p = ConvertUnOp(mul_.unop);
	for (size_t i = 0; i < mul_.muls.size(); i++) {
		PExpression p2 = ConvertUnOp(mul_.muls[i].get<1>());
		switch (mul_.muls[i].get<0>()) {
			case MUL:
#ifdef LUA_OPTIMIZER_DEBUG
				lua_opt_debug_stream << "Converting mul binop expression" << std::endl;
#endif
				p = boost::make_shared<BinExpression<std::multiplies<Val> > >(p, p2);
				break;
			case DIV:
#ifdef LUA_OPTIMIZER_DEBUG
				lua_opt_debug_stream << "Converting div binop expression" << std::endl;
#endif
				p = boost::make_shared<BinExpression<std::divides<Val> > >(p, p2);
				break;
			case REM:
#ifdef LUA_OPTIMIZER_DEBUG
				lua_opt_debug_stream << "Converting rem binop expression" << std::endl;
#endif
				p = boost::make_shared<BinExpression<std::modulus<Val> > >(p, p2);
				break;
		}
	}
	return p;
}

PExpression ExpressionConverter::ConvertAdd(const add_exp& add_) {
	PExpression p = ConvertMul(add_.mul);
	for (size_t i = 0; i < add_.adds.size(); i++) {
		PExpression p2 = ConvertMul(add_.adds[i].get<1>());
		switch (add_.adds[i].get<0>()) {
			case PLUS:
#ifdef LUA_OPTIMIZER_DEBUG
				lua_opt_debug_stream << "Converting plus binop expression" << std::endl;
#endif
				p = boost::make_shared<BinExpression<std::plus<Val> > >(p, p2);
				break;
			case MINUS:
#ifdef LUA_OPTIMIZER_DEBUG
				lua_opt_debug_stream << "Converting minus binop expression" << std::endl;
#endif
				p = boost::make_shared<BinExpression<std::minus<Val> > >(p, p2);
				break;
		}
	}
	return p;
}

PExpression ExpressionConverter::ConvertConcat(const concat_exp &concat) {
	if (concat.size() > 1)
		throw ParamConversionException(L"Concatenation parameter cannot be used is optimized parameters");
	return ConvertAdd(concat[0]);
}

PExpression ExpressionConverter::ConvertCmp(const cmp_exp& cmp_exp_) {
	PExpression p = ConvertConcat(cmp_exp_.concat);
	for (size_t i = 0; i < cmp_exp_.cmps.size(); i++) {
		PExpression p2 = ConvertConcat(cmp_exp_.cmps[i].get<1>());
		switch (cmp_exp_.cmps[i].get<0>()) {
			case LT:
#ifdef LUA_OPTIMIZER_DEBUG
				lua_opt_debug_stream << "Converting less binop expression" << std::endl;
#endif
				p = boost::make_shared<BinExpression<std::less<Val> > >(p, p2);
				break;
			case LTE:
#ifdef LUA_OPTIMIZER_DEBUG
				lua_opt_debug_stream << "Converting less-equal binop expression" << std::endl;
#endif
				p = boost::make_shared<BinExpression<std::less_equal<Val> > >(p, p2);
				break;
			case EQ:
#ifdef LUA_OPTIMIZER_DEBUG
				lua_opt_debug_stream << "Converting equal binop expression" << std::endl;
#endif
				p = boost::make_shared<BinExpression<std::equal_to<Val> > >(p, p2);
				break;
			case GTE:
#ifdef LUA_OPTIMIZER_DEBUG
				lua_opt_debug_stream << "Converting greater equal binop expression" << std::endl;
#endif
				p = boost::make_shared<BinExpression<std::greater_equal<Val> > >(p, p2);
				break;
			case GT:
#ifdef LUA_OPTIMIZER_DEBUG
				lua_opt_debug_stream << "Converting greater binop expression" << std::endl;
#endif
				p = boost::make_shared<BinExpression<std::greater<Val> > >(p, p2);
				break;
			case NEQ:
#ifdef LUA_OPTIMIZER_DEBUG
				lua_opt_debug_stream << "Converting not equal binop expression" << std::endl;
#endif
				p = boost::make_shared<BinExpression<std::not_equal_to<Val> > >(p, p2);
				break;
		}
	}
	return p;
}

PExpression ExpressionConverter::ConvertAnd(const and_exp& and_exp_) {
	PExpression p = ConvertCmp(and_exp_[0]);
	for (size_t i = 1; i < and_exp_.size(); i++) {
#ifdef LUA_OPTIMIZER_DEBUG
		lua_opt_debug_stream << "Converting and binop expression" << std::endl;
#endif
		p = boost::make_shared<BinExpression<std::logical_and<Val> > >(p, ConvertCmp(and_exp_[i]));
	}
	return p;
}

PExpression ExpressionConverter::ConvertOr(const or_exp& or_exp_) {
	PExpression p = ConvertAnd(or_exp_[0]);
	for (size_t i = 1; i < or_exp_.size(); i++) {
#ifdef LUA_OPTIMIZER_DEBUG
		lua_opt_debug_stream << "Converting or binop expression" << std::endl;
#endif
		p = boost::make_shared<BinExpression<std::logical_or<Val> > >(p, ConvertAnd(or_exp_[i]));
	}
	return p;
}

PExpression ExpressionConverter::ConvertExpression(const expression& expression_) {
#ifdef LUA_OPTIMIZER_DEBUG
	lua_opt_debug_stream << "Converting expression" << std::endl;
#endif
	return ConvertOr(expression_);
}

PExpression TermConverter::operator()(const nil& nil_) {
#ifdef LUA_OPTIMIZER_DEBUG
	lua_opt_debug_stream << "Converting nil expression" << std::endl;
#endif
	return boost::make_shared<NilExpression>();
}

PExpression TermConverter::operator()(const bool& bool_) {
#ifdef LUA_OPTIMIZER_DEBUG
	lua_opt_debug_stream << "Converting boolean value" << std::endl;
#endif
	return boost::make_shared<NumberExpression>(bool_ ? 1. : 0.);
}

PExpression TermConverter::operator()(const double& double_) {
#ifdef LUA_OPTIMIZER_DEBUG
	lua_opt_debug_stream << "Converting number expression" << std::endl;
#endif
	return boost::make_shared<NumberExpression>(double_);
}

PExpression TermConverter::operator()(const std::wstring& string) {
	throw ParamConversionException(L"String cannot appear as termns in optimized params");
}

PExpression TermConverter::operator()(const threedots& threedots_) {
	throw ParamConversionException(L"Threedots opeartors cannot appear in optimized params");
}

PExpression TermConverter::operator()(const funcbody& funcbody_) {
	throw ParamConversionException(L"Function bodies cannot appear in optimized expressions");
}

PExpression TermConverter::operator()(const tableconstructor& tableconstrutor_) {
	throw ParamConversionException(L"Table construtor are not supported in optimized expressions");
}

PExpression TermConverter::operator()(const postfixexp& postfixexp_) {
#ifdef LUA_OPTIMIZER_DEBUG
	lua_opt_debug_stream << "Converting postfix expression" << std::endl;
#endif
	return m_param_converter->ConvertPostfixExp(postfixexp_);
}

PExpression TermConverter::operator()(const expression& expression_) {
	return m_param_converter->ConvertExpression(expression_);
}

PExpression PostfixConverter::operator()(const identifier& identifier_) {
#ifdef LUA_OPTIMIZER_DEBUG
	lua_opt_debug_stream << "Converting variable expression, var: " << SC::S2A(identifier_) << std::endl;
#endif
	return boost::make_shared<VarExpression>(m_param_converter->FindVar(identifier_));
}

const std::vector<expression>& PostfixConverter::GetArgs(const std::vector<exp_ident_arg_namearg>& e) {
#ifdef LUA_OPTIMIZER_DEBUG
	lua_opt_debug_stream << "Converting args, number of exp_ident_arg_namerg :) expressions:" << e.size() << std::endl;
#endif
	if (e.size() != 1)
		throw ParamConversionException(L"Only postfix expression in form functioname(exp, exp, ...) are allowed");
	try {
		const namearg& namearg_ = boost::get<namearg>(e[0]);
		const args& args_ = boost::get<args>(namearg_);
/*
		const boost::recursive_wrapper<std::vector<expression> > &exps_
			= boost::get<boost::recursive_wrapper<std::vector<expression> > >(args_);
*/
		const std::vector<expression> &exps_
			= boost::get<std::vector<expression> >(args_);
		return exps_;
	} catch (boost::bad_get &e) {
		throw ParamConversionException(L"Only postfix expression in form functioname(exp, exp, ...) are allowed");
	}
}

PExpression PostfixConverter::operator()(const boost::tuple<exp_identifier, std::vector<exp_ident_arg_namearg> > &exp) {
	try {
		const exp_identifier& ei = exp.get<0>();
		const identifier& identifier_ = boost::get<identifier>(ei);
#ifdef LUA_OPTIMIZER_DEBUG
		lua_opt_debug_stream << "Converting postfix expression, identifier: " << SC::S2A(identifier_) << std::endl;
#endif
		const std::vector<expression>& args = GetArgs(exp.get<1>());
		return m_param_converter->ConvertFunction(identifier_, args);
	} catch (boost::bad_get &e) {
		throw ParamConversionException(L"Postfix expression (functioncall) must start with identifier in optimized params");
	}
}

PExpression SzbMoveTimeConverter::Convert(const std::vector<expression>& expressions) {
#ifdef LUA_OPTIMIZER_DEBUG
	lua_opt_debug_stream << "Converting szb_move_time expression" << std::endl;
#endif
	if (expressions.size() < 3)
		throw ParamConversionException(L"szb_move_time requires three arguments");

	return boost::make_shared<SzbMoveTimeExpression>
		(m_param_converter->ConvertExpression(expressions[0]),
		 m_param_converter->ConvertExpression(expressions[1]),
		 m_param_converter->ConvertExpression(expressions[2]));
}

PExpression IsNanConverter::Convert(const std::vector<expression>& expressions) {
#ifdef LUA_OPTIMIZER_DEBUG
	lua_opt_debug_stream << "Converting isnan expression"  << std::endl;
#endif
	if (expressions.size() < 1)
		throw ParamConversionException(L"isnan takes one argument");

	return boost::make_shared<IsNanExpression>
		(m_param_converter->ConvertExpression(expressions[0]));
}

PExpression NanConverter::Convert(const std::vector<expression>& expressions) {
#ifdef LUA_OPTIMIZER_DEBUG
	lua_opt_debug_stream << "Converting nan expression" << std::endl;
#endif
	return boost::make_shared<NilExpression>();
}

std::wstring get_string_expression(const expression &e) {
	const or_exp& or_exp_ = e;
	if (or_exp_.size() != 1)
		throw ParamConversionException(L"Expression is not string");

	const and_exp& and_exp_ = or_exp_[0];
	if (and_exp_.size() != 1)
		throw ParamConversionException(L"Expression is not string");

	const cmp_exp& cmp_exp_ = and_exp_[0];
	if (cmp_exp_.cmps.size())
		throw ParamConversionException(L"Expression is not string");

	const concat_exp& concat_exp_ = cmp_exp_.concat;
	if (concat_exp_.size() != 1)
		throw ParamConversionException(L"Expression is not string");

	const add_exp& add_exp_ = concat_exp_[0];
	if (add_exp_.adds.size())
		throw ParamConversionException(L"Expression is not string");

	const mul_exp& mul_exp_ = add_exp_.mul;
	if (mul_exp_.muls.size())
		throw ParamConversionException(L"Expression is not string");

	const unop_exp& unop_exp_ = mul_exp_.unop;
	if (unop_exp_.get<0>().size())
		throw ParamConversionException(L"Expression is not string");

	const pow_exp& pow_exp_ = unop_exp_.get<1>();
	if (pow_exp_.size() != 1)
		throw ParamConversionException(L"Expression is not string");

	try {
		const std::wstring& name = boost::get<std::wstring>(pow_exp_[0]);
		return name;
	} catch (boost::bad_get &e) {
#ifdef LUA_OPTIMIZER_DEBUG
		lua_opt_debug_stream << "Failed to convert expression to string: " << e.what() << std::endl;
#endif
		throw ParamConversionException(L"Expression is not string");
	}
}

std::wstring ParamValueConverter::GetParamName(const expression& e) {
	try {
		return get_string_expression(e);
	} catch (const ParamConversionException& e) {
		throw ParamConversionException(L"First parameter of p function should be literal string");
	}

}

PExpression ParamValueConverter::Convert(const std::vector<expression>& expressions) {
#ifdef LUA_OPTIMIZER_DEBUG
	lua_opt_debug_stream << "Converting p(..) expression" << std::endl;
#endif
	if (expressions.size() < 3)
		throw ParamConversionException(L"p function requires three arguemnts");
	std::wstring param_name = GetParamName(expressions[0]);
#ifdef LUA_OPTIMIZER_DEBUG
	lua_opt_debug_stream << "Parameter name: " << SC::S2A(param_name) << std::endl;
#endif
	ParRefRef param_ref = m_param_converter->GetParamRef(param_name);
	return boost::make_shared<ParamValue>(param_ref,
			m_param_converter->ConvertExpression(expressions[1]),
			m_param_converter->ConvertExpression(expressions[2]));
}

PExpression InSeasonConverter::Convert(const std::vector<expression>& expressions) {
#ifdef LUA_OPTIMIZER_DEBUG
	lua_opt_debug_stream << "Converting isnan(..) expression" << std::endl;
#endif
	if (expressions.size() < 2)
		throw ParamConversionException(L"in season function requires two arguments");
	std::wstring prefix;
	try {
		prefix = get_string_expression(expressions[0]);
#ifdef LUA_OPTIMIZER_DEBUG
		lua_opt_debug_stream << "in_season parameter: " << SC::S2A(prefix) << std::endl;
#endif
	} catch (ParamConversionException &e) {
		throw ParamConversionException(L"First argument to in_season function should be literal configuration prefix");
	}

	IPKContainer* ic = IPKContainer::GetObject();
	TSzarpConfig* sc = ic->GetConfig(prefix);
	if (sc == NULL)
		throw ParamConversionException(std::wstring(L"Prefix ") + prefix + L" given in in_season not found");
	return boost::make_shared<InSeasonExpression>(sc, m_param_converter->ConvertExpression(expressions[1]));
}

ParamConverter::ParamConverter(ParamReferenceFactory *param_reference_factory) : m_param_reference_factory(param_reference_factory) {
	m_function_converters[L"szb_move_time"] = boost::make_shared<SzbMoveTimeConverter>(this);
	m_function_converters[L"isnan"] = boost::make_shared<IsNanConverter>(this);
	m_function_converters[L"nan"] = boost::make_shared<NanConverter>(this);
	m_function_converters[L"p"] = boost::make_shared<ParamValueConverter>(this);
	m_function_converters[L"in_season"] = boost::make_shared<InSeasonConverter>(this);
}

VarRef ParamConverter::GetGlobalVar(std::wstring identifier) {
	std::map<std::wstring, VarRef>::iterator i = m_vars_map[0].find(identifier);
	if (i == m_vars_map[0].end()) {
		size_t s = m_param->m_vars.size();
		m_param->m_vars.push_back(s);
		VarRef i(&m_param->m_vars, s);
		m_vars_map[0][identifier] = i;
		return i;
	} else {
		return i->second;
	}
}

VarRef ParamConverter::GetLocalVar(const std::wstring& identifier) {
	std::map<std::wstring, VarRef>::iterator i = m_vars_map.back().find(identifier);
	if (i == m_vars_map.back().end()) {
		size_t s = m_param->m_vars.size();
		m_param->m_vars.push_back(s);
		VarRef i(&m_param->m_vars, s);
		m_vars_map.back()[identifier] = i;
		return i;
	} else {
		return i->second;
	}
}

VarRef ParamConverter::FindVar(const std::wstring& identifier) {
	for (std::vector<frame>::reverse_iterator i = m_vars_map.rbegin();
			i != m_vars_map.rend();
			i++) {
		std::map<std::wstring, VarRef>::iterator j = i->find(identifier);
		if (j != i->end())
			return j->second;
	}
	throw ParamConversionException(std::wstring(L"Variable ") + identifier + L" is unbound");
}

ParRefRef ParamConverter::GetParamRef(const std::wstring param_name) {
	return m_param_reference_factory->GetParamReference(param_name);
#if 0
	std::pair<szb_buffer_t*, TParam*> bp;
	if (m_szbase->FindParam(param_name, bp) == false)
		throw ParamConversionException(std::wstring(L"Param ") + param_name + L" not found");

	for (size_t i = 0; i < m_param->m_par_refs.size(); i++)
		if (m_param->m_par_refs[i].m_buffer == bp.first && m_param->m_par_refs[i].m_param == bp.second)
			return ParRefRef(&m_param->m_par_refs, i);

	size_t pi = m_param->m_par_refs.size();
	ParamRef pr;
	pr.m_buffer = bp.first;
	pr.m_param = bp.second;
	pr.m_param_index = pi;
	m_param->m_par_refs.push_back(pr);
	m_param->m_last_update_times[bp.first] = 0szbase;

	return ParRefRef(&m_param->m_par_refs, pi);
#endif
}

PStatement ParamConverter::ConvertBlock(const block& block) {
	m_vars_map.push_back(frame());
	PStatement ret = ConvertChunk(block.chunk_.get());
	m_vars_map.pop_back();
	return ret;
}

PExpression ParamConverter::ConvertFunction(const identifier& identifier_, const std::vector<expression>& args) {
	std::map<std::wstring, boost::shared_ptr<FunctionConverter> >::iterator i = m_function_converters.find(identifier_);
	if (i == m_function_converters.end())
		throw ParamConversionException(std::wstring(L"Function ") + identifier_ + L" not supported in optimized params");
	return i->second->Convert(args);
}

PStatement ParamConverter::ConvertStatement(const lua_grammar::stat& stat_) {
	StatementConverter sc(this);
#ifdef LUA_OPTIMIZER_DEBUG
	lua_opt_debug_stream << "Converting statement" << std::endl;
#endif
	return boost::apply_visitor(sc, stat_);
}

PStatement ParamConverter::ConvertChunk(const chunk& chunk_) {
	boost::shared_ptr<StatementList> ret = boost::make_shared<StatementList>();
	for (std::vector<lua_grammar::stat>::const_iterator i = chunk_.stats.begin();
			i != chunk_.stats.end();
			i++)
		ret->AddStatement(ConvertStatement(*i));
	return ret;
}

PExpression ParamConverter::ConvertTerm(const term& term_) {
	TermConverter tc(this);
	return boost::apply_visitor(tc, term_);
}

PExpression ParamConverter::ConvertExpression(const expression& expression_) {
	ExpressionConverter ec(this);
	return ec.ConvertExpression(expression_);
}

PExpression ParamConverter::ConvertPostfixExp(const postfixexp& postfixexp_) {
#ifdef LUA_OPTIMIZER_DEBUG
	lua_opt_debug_stream << "Converting postfix expression" << std::endl;
#endif
	PostfixConverter pc(this);
	return boost::apply_visitor(pc, postfixexp_);
}

void ParamConverter::ConvertParam(const chunk& chunk_, Param* param) {
#ifdef LUA_OPTIMIZER_DEBUG
	lua_opt_debug_stream << "Starting to optimize param" << std::endl;
#endif
	m_param = param;
	InitalizeVars();
	param->m_statement = ConvertChunk(chunk_);
}

void ParamConverter::AddVariable(std::wstring name) {
	size_t s = m_param->m_vars.size();
	m_param->m_vars.push_back(Var(s));
	VarRef i(&m_param->m_vars, s);
	(*m_current_frame)[name] = i;
}

void ParamConverter::InitalizeVars() {
	m_vars_map.clear();
	m_vars_map.resize(1);
	m_current_frame = m_vars_map.begin();
	AddVariable(L"v");
	AddVariable(L"t");
	AddVariable(L"pt");
	AddVariable(L"PT_MIN10");
	AddVariable(L"PT_HOUR");
	AddVariable(L"PT_HOUR8");
	AddVariable(L"PT_DAY");
	AddVariable(L"PT_WEEK");
	AddVariable(L"PT_MONTH");
	AddVariable(L"PT_SEC10");
}

Val ParamRef::Value(const double& time, const double& period) {
	return m_exec_engine->Value(m_param_index, time, period);
}


}

#endif
