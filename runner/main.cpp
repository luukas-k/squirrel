#include <squirrel.h>
#include <print>
#include <cassert>

int main() {
	std::string src = R"SRC(

let x = 5 + 6;

fn get_x() {
	print(x);
	return x;
}

fn get_x_times_10() {
	return get_x() * 10;
}

fn get_x_pow_10() {
	return x ^ 10;
}

)SRC";

	sq::parser p(src);
	sq::program prog("prog", p);

	sq::runtime rt;
	rt.add_function("print", [](std::vector<sq::value> vals) -> sq::value {
		for (auto &v : vals) {
			std::visit(overloaded{
				[](const double& t) { std::print("{}", t); },
				[](const auto &t) { std::print("?"); }
			}, v);
		}
		std::println("\n");
		return {};
	});
	rt.load_program(prog);

	std::println("get_x_times_10() = {}", rt.call<double>("get_x_times_10"));
	std::println("get_x_pow_10() = {}", rt.call<double>("get_x_pow_10"));

	return 0;
}