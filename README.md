# Squirrel

Squirrel is a simple embeddable scripting language.

## The language
```
fn compute() {
    print(50);
    return 10;
}
```

## Embedding into C++
```
#include <squirrel.h>

double compile_and_run(const std::string& src) {
    sq::parser par(src);
    sq::program prog("module", par);

    sq::runtime rt;
    rt.add_function("print", ...);
    rt.load_program(prog);

    return rt.call<double>("compute");
}
```
