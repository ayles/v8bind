# v8bind

Header-only library for binding `C++` `class`es and `function`s
to [V8](https://v8.dev) `JS` environment. 

It was inspired by [v8pp](https://github.com/pmed/v8pp) and adds support 
for binding overloaded functions & constructors 
(relies on `template` metaprogramming with recursive instantiation
and requires compiler with **C++17** support).

Can work without **RTTI**.

#### Tested on:

- Microsoft Visual C++ 2019 (Windows 10)
- GCC 7.4 and 8.3 (Ubuntu 18.04)

## Example

```c++
v8::Isolate *isolate = ...;
auto context = isolate->GetCurrentContext();

struct Test {
    int foo;
    const double bar;

    Test(int foo, double bar) : foo(foo), bar(bar) {}
    Test() : Test(0, 0.0) {}

    std::string ToString() {
        return "(" + std::to_string(foo) + ", " 
                   + std::to_string(bar) + ")";
    }

    int Sum() {
        return foo + bar;
    }

    int Sum(long third) {
        return foo + bar + third;
    }
};

v8b::Class<Test> my_class(isolate);
my_class

    // Use tuple with arguments to add constructor signature
    .Constructor<std::tuple<>, std::tuple<int, double>>()

    // Regular variable binding
    .Var("foo", &Test::foo)

    // Const will be readonly in JS
    .Var("bar", &Test::bar)
    
    // Bind function without overloads just by passing it
    .Function("toString", &Test::ToString)
    
    // Binding overloaded function requires type hints 
    // (through template parameters or via static_cast)
    //
    // Binding order is not important until you use
    // different functions that can match same arguments set
    // (for example (double, double) and (int, int))
    // or void (const v8::FunctionCallbackInfo<v8::Value> &)
    // that will match any arguments set
    // In such case place functions in preffered lookup order
    .Function<int (Test::*)(), int (Test::*)(long)>
        ("sum", &Test::Sum, &Test::Sum)
;

v8b::Module my_module(isolate);
my_module.Class("Test", my_class);

// Set to global object
context->Global()->Set(
    context, v8b::ToV8(isolate, "myModule"), my_module.NewInstance());

```

Use it in `JS`:

```js
let test = new myModule.Test();
let test2 = new myModule.Test(1, 2);

console.log(test);
console.log(test2);

console.log(test.sum());
console.log(test2.sum(1));

test.foo = 123;

// Cannot assign to read only property 'bar' of object '[object Test]'
test.bar = 345;
```