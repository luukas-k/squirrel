#include "squirrel.h"

#include <exception>
#include <stdexcept>
#include <cmath>

namespace sq {

	parser::parser(const std::string &src) 
		:
		_src(src),
		_offset(0)
	{
		_program_scope = parse_module();
	}

	parser::~parser() {
	}

	parser::token parser::peek() {
		intptr_t current_offset = _offset;
		while (std::isspace(_src[current_offset])) {
			current_offset++;
		}

		if (std::isalpha(_src[current_offset])) {
			std::string value;
			do {
				value.push_back(_src[current_offset++]);
			} while (std::isalnum(_src[current_offset]) || _src[current_offset] == '_');

			std::unordered_map<std::string, token_type> keywords{
				{"fn", token_type::kw_fn},
				{"return", token_type::kw_return},
				{"let", token_type::kw_let},
			};
			return token{
				.type = keywords.contains(value) ? keywords.at(value) : token_type::symbol,
				.value = value,
				.end_offset = current_offset
			};
		}
		else if (std::isdigit(_src[current_offset])) {
			std::string value;
			do {
				value.push_back(_src[current_offset++]);
			} while (std::isdigit(_src[current_offset]) || _src[current_offset] == '.');

			return token{
				.type = token_type::number,
				.value = value,
				.end_offset = current_offset
			};
		}

		std::unordered_map<char, token_type> character_tokens{
			{'+', token_type::plus},
			{'-', token_type::minus},
			{'*', token_type::star},
			{'/', token_type::slash},
			{'^', token_type::caret},
			{'%', token_type::percent},
			{'=', token_type::equals},
			{';', token_type::semicolon},
			{'(', token_type::paren_open},
			{')', token_type::paren_close},
			{'{', token_type::curly_open},
			{'}', token_type::curly_close},
			{',', token_type::comma},
		};
		if (character_tokens.contains(_src[current_offset])) {
			return token{
				.type = character_tokens.at(_src[current_offset]),
				.value = _src.substr(current_offset, 1),
				.end_offset = current_offset + 1
			};
		}

		return token{ .type = token_type::unknown };
	}
	
	std::string parser::consume(const token &v) {
		_offset = v.end_offset;
		return v.value;
	}

	size_t parser::parse_module() {
		std::vector<size_t> elems;
		while (peek().type != token_type::unknown) {
			elems.push_back(parse_statement());
		}
		return create(ast_scope{ .elements = elems });
	}

	size_t parser::parse_scope() {
		std::vector<size_t> elems;
		consume(peek()); // {
		while (peek().type != token_type::curly_close) {
			elems.push_back(parse_statement());
		}
		consume(peek()); // }
		return create(ast_scope{ .elements = elems });
	}

	size_t parser::parse_statement() {
		auto t = peek();
		switch (t.type) {
			case token_type::curly_open:
			{
				return parse_scope();
			}
			case token_type::kw_return:
			{
				return parse_return();
			}
			case token_type::kw_let:
			{
				return parse_let();
			}
			case token_type::kw_fn:
			{
				return parse_fn();
			}
			default:
			{
				size_t e = parse_expression(0);
				consume(peek()); // ;
				return e;
			}
		}
	}

	size_t parser::parse_fn() {
		consume(peek()); // fn
		auto name = consume(peek()); // name
		consume(peek()); // (
		consume(peek()); // )
		auto scope = parse_scope();
		return create(ast_function{ .name = name, .scope = scope });
	}

	size_t parser::parse_return() {
		consume(peek()); // return
		auto e = parse_expression(0);
		consume(peek()); // ;
		return create(ast_return{ .expr = e });
	}

	size_t parser::parse_let() {
		consume(peek()); // let
		std::string sym = consume(peek()); // var_name
		consume(peek()); // =
		auto e = parse_expression(0);
		consume(peek()); // ;
		return create(ast_let{ .variable = sym, .expr = e });
	}

	parser::op_type parser::peek_op(token t) {
		switch (t.type) {
			case token_type::plus: return op_type::add;
			case token_type::minus: return op_type::sub;
			case token_type::star: return op_type::mul;
			case token_type::slash: return op_type::div;
			case token_type::caret: return op_type::pow;
			case token_type::percent: return op_type::mod;
			case token_type::paren_open: return op_type::call;
			default: return op_type::unknown;
		}
	}

	size_t parser::parse_atom() {
		auto t = peek();
		switch (t.type) {
			case token_type::number:
			{
				auto v = consume(t);
				return create(ast_number{ .value = ::atof(v.c_str()) });
			}
			case token_type::symbol:
			{
				auto v = consume(t);
				return create(ast_symbol{ .variable = v });
			}
			default:
			{
				throw std::runtime_error("parse_atom()");
			}
		}
	}

	size_t parser::parse_expression(int min_bp) {
		auto lhs = parse_atom();

		std::unordered_map<op_type, std::pair<int, int>> infix_binding_power{
			{op_type::add, {1,2}},
			{op_type::sub, {1,2}},
			{op_type::mul, {3,4}},
			{op_type::div, {3,4}},
			{op_type::mod, {3,4}},
			{op_type::pow, {5,6}},
		};
		std::unordered_map<op_type, int> postfix_binding_power{
			{op_type::call, 7}
		};

		while (peek_op(peek()) != op_type::unknown) {
			auto op = peek_op(peek());

			if (postfix_binding_power.contains(op)) {
				auto pf_l_bp = postfix_binding_power.at(op);
				if (pf_l_bp < min_bp) {
					break;
				}
				consume(peek()); // (
				std::vector<size_t> args;
				while (peek().type != token_type::paren_close) {
					args.push_back(parse_expression(0));
					if (peek().type == token_type::comma) {
						consume(peek());
					}
				}
				consume(peek()); // )
				lhs = create(ast_call{ .callable = lhs, .args = args });
				continue;
			}

			auto[l_bp, r_bp] = infix_binding_power.at(op);
			if (l_bp < min_bp) {
				break;
			}

			consume(peek()); // consume op
			auto rhs = parse_expression(r_bp);

			lhs = create(ast_bin_op{ .op = op, .lhs = lhs, .rhs = rhs });
		}

		return lhs;
	}

	program::program(const std::string& name, parser& p) 
		:
		_name(name)
	{
		_active_function.push_back("__" + name);
		generate(p, p._program_scope);
	}

	program::~program() 
	{}

	std::optional<program::reg> program::generate(parser &p, size_t e) {
		return std::visit(overloaded{
			[&](const parser::ast_bin_op& bop) -> std::optional<program::reg> {
				auto lhs = generate(p, bop.lhs).value();
				auto rhs = generate(p, bop.rhs).value();
				auto tmp = new_temporary();
				emit(ir_bin_op{ .op = bop.op, .dst = tmp, .lhs = lhs, .rhs = rhs });
				return tmp;
			},
			[&](const parser::ast_let &l) -> std::optional<program::reg> {
				auto e = generate(p, l.expr).value();
				emit(ir_set_local{.name = l.variable, .src = e });
				return {};
			},
			[&](const parser::ast_function &l) -> std::optional<program::reg> {
				_active_function.push_back(l.name);
				generate(p, l.scope);
				_active_function.pop_back();
				return {};
			},
			[&](const parser::ast_scope &l) -> std::optional<program::reg> {
				for (auto &e : l.elements) {
					generate(p, e);
				}
				return {};
			},
			[&](const parser::ast_return &l) -> std::optional<program::reg> {
				auto rv = generate(p, l.expr).value();
				emit(ir_ret{ .src = rv });
				return {};
			},
			[&](const parser::ast_number &l) -> std::optional<program::reg> {
				return constant{ .value = l.value };
			},
			[&](const parser::ast_symbol &l) -> std::optional<program::reg> {
				return symbol{ .name = l.variable };
			},
			[&](const parser::ast_call &c) -> std::optional<program::reg> {
				auto callable = generate(p, c.callable).value();
				std::vector<reg> args;
				for (auto &t : c.args) {
					args.push_back(generate(p, t).value());
				}
				auto dst = new_temporary();
				emit(ir_call{ .dst = dst, .callable = callable, .args = args });
				return dst;
			},
			[&](const auto &e) -> std::optional<program::reg> { 
				throw std::runtime_error("generate() unexpected type"); 
			}
		}, p._nodes.at(e));
	}

	void program::emit(ir_instr &&instr) {
		_functions[_active_function.back()].instrs.push_back(instr);
	}
	
	program::reg program::new_temporary() {
		return temporary{ ++_temporaries };
	}

	runtime::runtime() {
	}

	runtime::~runtime() {
	}

	void runtime::load_program(program &p) {
		_programs.push_back(&p);
		for (auto &[name, fn] : p._functions) {
			_globals[name] = function_ref{ .name = name };
		}
		
		auto find_fn = [&](const std::string &name) -> program::function * {
			for (auto &f : _programs) {
				for (auto &[fname, fn] : f->_functions) {
					if (fname == name) {
						return &fn;
					}
				}
			}
			return nullptr;
		};

		program::function* fn = find_fn("__" + p.name());
		
		evaluate(fn, {}, true);
	}

	program::function *runtime::find_function(const std::string &name) {
		for (auto &f : _programs) {
			for (auto &[fname, fn] : f->_functions) {
				if (fname == name) {
					return &fn;
				}
			}
		}
		return nullptr;
	}

	value runtime::evaluate(program::function *fn, const std::vector<value> &args, bool global_scope) {
		std::unordered_map<std::string, value> locals;
		std::unordered_map<int, value> temporaries;
		intptr_t instruction_pointer = 0;

		std::optional<value> return_value;
		while (!return_value.has_value() && instruction_pointer < fn->instrs.size()) {
			auto &i = fn->instrs[instruction_pointer++];
			std::visit(overloaded{
				[&](const program::ir_bin_op &bop) {
					auto lhs = load(locals, temporaries, bop.lhs);
					auto rhs = load(locals, temporaries, bop.rhs);
					value res = std::visit(overloaded{
						[&](double l, double r) -> value {
							switch (bop.op) {
								case parser::op_type::add:
								{
									return l + r;
								}
								case parser::op_type::sub:
								{
									return l - r;
								}
								case parser::op_type::mul:
								{
									return l * r;
								}
								case parser::op_type::div:
								{
									return l / r;
								}
								case parser::op_type::pow:
								{
									return pow(l, r);
								}
								case parser::op_type::mod:
								{
									return fmod(l, r);
								}
								default:
								{
									throw std::runtime_error("no op.");
								}
							}
						},
						[](auto,auto) -> value {
							throw std::runtime_error("no op.");
						}
					}, lhs, rhs);
					store(locals, temporaries, bop.dst, res, global_scope);
				},
				[&](const program::ir_set_local &bop) {
					store(locals, temporaries, program::symbol{bop.name}, load(locals, temporaries, bop.src), global_scope);
				},
				[&](const program::ir_ret &bop) {
					return_value = load(locals, temporaries, bop.src);
				},
				[&](const program::ir_call &call) {
					auto callable = load(locals, temporaries, call.callable);
					std::vector<value> args;
					for (auto &a : call.args) {
						args.push_back(load(locals, temporaries, a));
					}
					value rv = value{};
					std::visit(overloaded{
						[&](const function_ref &fref) {
							auto fn = find_function(fref.name);
							rv = evaluate(fn, args, false);
						},
						[&](const intrinsic_fn &fref) {
							auto& intrin = _intrinsics.at(fref.index);
							rv = intrin(args);
						},
						[&](const auto &v) {
							throw std::runtime_error("unknown callable");
						}
					}, callable);
					store(locals, temporaries, call.dst, rv, global_scope);
				},
				[](const auto &) { 
					throw std::runtime_error("unknown instruction."); 
				}
			}, i);
		}
		return return_value.value_or(value{});
	}

	value runtime::load(const std::unordered_map<std::string, value>& locals, std::unordered_map<int, value> &temps, program::reg r) {
		return std::visit(overloaded{
			[&](const program::symbol &v) -> value {
				if (locals.contains(v.name)) {
					return locals.at(v.name);
				}
				return _globals.at(v.name);
			},
			[&](const program::temporary &v) -> value {
				return temps.at(v.identity);
			},
			[](const program::constant &v) -> value {
				return value(v.value);
			},
			[](const auto &v) -> value { 
				throw std::runtime_error("load"); 
			}
		}, r);
	}

	void runtime::store(std::unordered_map<std::string, value> &locals, std::unordered_map<int, value> &temps, program::reg dst, value val, bool is_global) {
		std::visit(overloaded{
			[&](const program::symbol &v) {
				if (is_global) {
					_globals[v.name] = val;
				}
				else {
					locals[v.name] = val;
				}
			},
			[&](const program::temporary &v) {
				temps[v.identity] = val;
			},
			[](const auto &v) {
				throw std::runtime_error("store");
			}
		}, dst);
	}

}