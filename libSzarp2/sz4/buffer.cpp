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

#include "szarp_config.h"
#include "szarp_base_common/lua_param_optimizer.h"
#include "sz4/buffer.h"
#include "sz4/base.h"
#include "sz4/real_param_entry.h"
#include "sz4/lua_optimized_param_entry.h"

namespace sz4 {

void buffer::remove_param(TParam* param) {
	if (m_param_ents.size() <= param->GetParamId())
		return;

	generic_param_entry* entry = m_param_ents[param->GetParamId()];
	if (!entry)
		return;

	entry->deregister_from_monitor(m_param_monitor);
	delete entry;

	m_param_ents[param->GetParamId()] = NULL;
}

template<template<typename DT, typename TT> class param_entry_type, class data_type, class time_type> generic_param_entry* param_entry_build_t_3(base* _base, TParam* param, const boost::filesystem::wpath &buffer_directory) {
	return new param_entry_in_buffer<param_entry_type, data_type, time_type>(_base, param, buffer_directory);
}

template<template <typename DT, typename TT> class param_entry_type, class data_type> generic_param_entry* param_entry_build_t_2(base* _base, TParam* param, const boost::filesystem::wpath &buffer_directory) {
	switch (param->GetTimeType()) {
		case TParam::SECOND:
			return param_entry_build_t_3<param_entry_type, data_type, second_time_t>(_base, param, buffer_directory);
		case TParam::NANOSECOND:
			return param_entry_build_t_3<param_entry_type, data_type, nanosecond_time_t>(_base, param, buffer_directory);
	}
	return NULL;
}

template<template <typename DT, typename TT> class param_entry_type> generic_param_entry* param_entry_build_t_1(base* _base, TParam* param, const boost::filesystem::wpath &buffer_directory) {
	switch (param->GetDataType()) {
		case TParam::SHORT:
			return param_entry_build_t_2<param_entry_type, short>(_base, param, buffer_directory);
		case TParam::INT:
			return param_entry_build_t_2<param_entry_type, int>(_base, param, buffer_directory);
		case TParam::FLOAT:
			return param_entry_build_t_2<param_entry_type, float>(_base, param, buffer_directory);
		case TParam::DOUBLE:
			return param_entry_build_t_2<param_entry_type, double>(_base, param, buffer_directory);
	}
	return NULL;
}

generic_param_entry* param_entry_build(base *_base, TParam* param, const boost::filesystem::wpath &buffer_directory) {
	switch (param->GetSz4Type()) {
		case TParam::SZ4_REAL:
			return param_entry_build_t_1<real_param_entry_in_buffer>(_base, param, buffer_directory);
		case TParam::SZ4_LUA_OPTIMIZED:
			return param_entry_build_t_1<lua_optimized_param_entry_in_buffer>(_base, param, buffer_directory);
		default:
		case TParam::SZ4_NONE:
			assert(false);
	}
}

generic_param_entry* buffer::create_param_entry(TParam* param) {
	prepare_param(param);

	return param_entry_build(m_base, param, m_buffer_directory);
}

void buffer::prepare_param(TParam* param) {
	if (param->GetSz4Type() != TParam::SZ4_NONE)
		return;

	if (param->GetType() == TParam::P_REAL) {
		param->SetSz4Type(TParam::SZ4_REAL);
		return;
	}

	param->PrepareDefinable();
	if (param->GetType() == TParam::P_COMBINED) {
		param->SetSz4Type(TParam::SZ4_COMBINED);
		return;
	}

	if (param->GetType() == TParam::P_DEFINABLE) {
		param->SetSz4Type(TParam::SZ4_DEFINABLE);
		return;
	}

	if (param->GetType() == TParam::P_LUA) {
		LuaExec::Param* exec_param = new LuaExec::Param();
		param->SetLuaExecParam(exec_param);

		if (LuaExec::optimize_lua_param(param, IPKContainer::GetObject())) 
			param->SetSz4Type(TParam::SZ4_LUA);
		else
			param->SetSz4Type(TParam::SZ4_LUA_OPTIMIZED);
		return;
	}
}

}
