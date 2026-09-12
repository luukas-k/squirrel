#pragma once

#include <string>
#include <unordered_map>
#include <functional>
#include <variant>
#include <optional>
#include <string>

template<class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };

namespace sq {

	class parser {
		friend class program;
		friend class runtime;
	public:
		parser(const std::string &src);
		~parser();

	private: // TOKENS
		enum struct token_type {
			unknown = 0,
			number,
			symbol,
			kw_fn,
			kw_return,
			kw_let,
			paren_open,
			paren_close,
			curly_open,
			curly_close,
			semicolon,
			plus,
			minus,
			star,
			slash,
			caret,
			percent,
			equals,
			comma,
		};
		struct token {
			token_type type{};
			std::string value;
			intptr_t end_offset{};
		};
		token peek();
		std::string consume(const token& t);
		
	private: // AST
		struct ast_number {
			double value{};
		};
		struct ast_symbol {
			std::string variable;
		};
		enum struct op_type {
			unknown = 0,
			// binary ops
			add, sub, mul, div, pow, mod,
			// unary
			call
		};
		struct ast_bin_op {
			op_type op{};
			size_t lhs{}, rhs{};
		};
		struct ast_let {
			std::string variable;
			size_t expr;
		};
		struct ast_function {
			std::string name;
			size_t scope;
		};
		struct ast_return {
			size_t expr;
		};
		struct ast_scope {
			std::vector<size_t> elements;
		};
		struct ast_call {
			size_t callable;
			std::vector<size_t> args;
		};
		using ast_node = std::variant<
			ast_symbol,
			ast_number, 
			ast_bin_op, 
			ast_let, 
			ast_function, 
			ast_return,
			ast_scope,
			ast_call
		>;

		size_t parse_module();
		size_t parse_scope();
		size_t parse_statement();
		size_t parse_fn();
		size_t parse_atom();
		size_t parse_expression(int min_bp);
		size_t parse_return();
		size_t parse_let();

		op_type peek_op(token t);

	private: // NODES
		template<typename node>
		size_t create(node &&v) {
			_nodes.push_back(v);
			return _nodes.size() - 1;
		}
	private:
		std::string _src;
		intptr_t _offset;

		std::vector<ast_node> _nodes;
		size_t _program_scope;
	};

	class program {
		friend class runtime;
	public:
		program(const std::string& name, parser& p);
		~program();

		inline std::string name() const { return _name; }
	private:
		struct symbol {
			std::string name;
		};
		struct temporary {
			size_t identity{};
		};
		struct constant {
			double value{};
		};
		using reg = std::variant<symbol, temporary, constant>;

		struct ir_bin_op {
			parser::op_type op{};
			reg dst{}, lhs{}, rhs{};
		};
		struct ir_set_local {
			std::string name;
			reg src{};
		};
		struct ir_ret {
			reg src{};
		};
		struct ir_call {
			reg dst{};
			reg callable{};
			std::vector<reg> args;
		};
		using ir_instr = std::variant<ir_bin_op, ir_set_local, ir_ret, ir_call>;

	private:
		std::optional<reg> generate(parser& p, size_t e);
		void emit(ir_instr &&instr);

		reg new_temporary();
	private:
		std::string _name;
		struct function {
			std::vector<ir_instr> instrs;
		};
		std::unordered_map<std::string, function> _functions;
		std::vector<std::string> _active_function;
		size_t _temporaries{};
	};

	struct function_ref {
		std::string name;
	};
	struct intrinsic_fn {
		size_t index{};
	};
	using value = std::variant<double, function_ref, intrinsic_fn>;

	class runtime {
	public:
		runtime();
		~runtime();

		void load_program(program &p);

		void add_function(const std::string &name, std::function<value(std::vector<value>)> cb) {
			_intrinsics.push_back(cb);
			_globals[name] = intrinsic_fn{ _intrinsics.size() - 1 };
		}

		template<typename RV, typename...Args>
		RV call(const std::string &name, Args&&...args) {
			value rv = evaluate(find_function(name), {}, false);
			return std::get<RV>(rv);
		}
	private:
		program::function *find_function(const std::string &name);

		value evaluate(program::function *fn, const std::vector<value> &args, bool global_scope);

		value load(const std::unordered_map<std::string, value> &locals, std::unordered_map<int, value> &temps, program::reg r);
		void store(std::unordered_map<std::string, value> &locals, std::unordered_map<int, value> &temps, program::reg dst, value val, bool is_global);
	private:
		std::vector<program*> _programs;
		std::unordered_map<std::string, value> _globals;
		std::vector<std::function<value(std::vector<value>)>> _intrinsics;
	};

}
