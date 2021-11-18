/**
This header library makes it possible to replace implementations of C
functions with C++ callables for unit testing.

It works by using the preprocessor to redefine the functions to be
mocked in the files to be tested by prepending `ut_` to them instead
of calling the "real" implementation. This `ut_` function then
forwards to a `std::function` of the appropriate type that can be
changed at runtime to a C++ callable.

An example of mocking the BSD socket API `send` function would be to
have a header like this:


```c
#ifndef MOCK_NETWORK_H
#define MOCK_NETWORK_H
#    define send ut_send
#endif
```

The build system would then insert this header before any other
`#includes` via an option to the compiler (`-include` for gcc/clang).
This could also be done with `-D` but in the case of multiple
functions it's easier to have all the redefinitions in one header.

Now all calls to `send` are actually to `ut_send`. This will fail to
link since `ut_send` doesn't exist. To implement it, the test binary
should be linked with an object file from compiling this code
(e.g. called `mock_network.cpp`):

```c++
#include "mock_network.hpp"
extern "C" IMPL_MOCK(4, send); // the 4 is the number of parameters "send" takes
```

This will only compile if a header called `mock_network.hpp` exists with the
following contents:

```c++
#include "premock.hpp"
#include <sys/socket.h> // to have access to send, the original function
DECL_MOCK(send); // the declaration for the implementation in the cpp file
```

Now test code can do this:

```c++
#include "mock_network.hpp"

TEST(send, replace) {
    REPLACE(send, [](auto...) { return 7; });
    // any function that calls send from here until the end of scope
    // will call our lambda instead and always return 7
}

TEST(send, mock) {
    auto m = MOCK(send);
    m.returnValue(42);
    // any function that calls send from here until the end of scope
    // will get a return value of 42
    function_that_calls_send();
    // check last call to send only
    m.expectCalled().withValues(3, nullptr, 0, 0); //checks values of last call
    // check last 3 calls:
    m.expectCalled(3).withValues({make_tuple(3, nullptr, 0, 0),
                                  make_tuple(5, nullptr, 0, 0),
                                  make_tuple(7, nullptr, 0, 0)});
}

TEST(send, for_reals) {
    //no MOCK or REPLACE, calling a function that calls send
    //will call the real McCoy
    function_that_calls_send(); //this will probably send packets
}

If neither `REPLACE` nor `MOCK` are used, the original implementation
will be used.

```
 */

#ifndef MOCK_HPP_
#define MOCK_HPP_


#include <functional>
#include <type_traits>
#include <tuple>
#include <deque>
#include <vector>
#include <stdexcept>
#include <iostream>
#include <sstream>
#include <cstring>

/**
 RAII class for setting a mock to a callable until the end of scope
 */
template<typename T>
class MockScope {
public:

    // assumes T is a std::function
    using ReturnType = typename T::result_type;

    /**
     Replace func with scopeFunc until the end of scope.
     */
    template<typename F>
    MockScope(T& func, F scopeFunc):
        _func{func},
        _oldFunc{std::move(func)} {

        _func = std::move(scopeFunc);
    }

    /**
     Restore func to its original value
     */
    ~MockScope() {
        _func = std::move(_oldFunc);
    }

private:

    T& _func;
    T _oldFunc;
};


/**
 Helper function to create a MockScope
*/
template<typename T, typename F>
MockScope<T> mockScope(T& func, F scopeFunc) {
    return {func, scopeFunc};
}

#define CONCATENATE_DETAIL(x, y) x##y
#define CONCATENATE(x, y) CONCATENATE_DETAIL(x, y)
#define MAKE_UNIQUE(x) CONCATENATE(x, __LINE__)
/**
 Temporarily replace func with the passed-in lambda:
 e.g.

 REPLACE(send, [](auto...) { return -1; });

 This causes every call to `send` in the production code to
 return -1 no matter what
 */
#define REPLACE(func, lambda) auto MAKE_UNIQUE(_) = mockScope(mock_##func, lambda)

template<typename T>
struct Slice {
    void* ptr = nullptr;
    size_t length = 0;
};


template<typename>
struct StdFunctionTraits {};

/**
 Pattern match on std::function to get its parameter types
 */
template<typename R, typename... A>
struct StdFunctionTraits<std::function<R(A...)>> {
    using TupleType = std::tuple<std::remove_reference_t<A>...>;
    using OutputTupleType = std::tuple<Slice<std::remove_reference_t<A>> ...>;
};


/**
 An exception class to throw when a mock expectation isn't met
 */
class MockException: public std::exception {
public:
    MockException(std::string s):_what{std::move(s)} {}
    const char* what() const noexcept override { return _what.c_str(); }
    std::string _what;
};

//coming in C++17
namespace premock {
template<typename... Ts> struct make_void { typedef void type;};
template<typename... Ts> using void_t = typename make_void<Ts...>::type;
}

// primary template, default to false for every type
template<typename, typename = premock::void_t<>>
struct CanBeStreamed: std::false_type {};

// detects when ostream operator<< works on a type
template<typename T>
struct CanBeStreamed<T, premock::void_t<decltype(std::cout << std::declval<T&>())> >: std::true_type {};

// default implementation for types that don't define operator<<
template<typename T>
std::string toString(const T&, typename std::enable_if<!CanBeStreamed<T>::value>::type* = nullptr) {
    return "<cannot print>";
}

// implementation for types that can be streamed (and therefore printed)
template<typename T>
std::string toString(const T& value, typename std::enable_if<CanBeStreamed<T>::value>::type* = nullptr) {
    std::stringstream stream;
    stream << value;
    return stream.str();
}

// helper class to print std::tuple. A class is used instead of a function to
// enable partial template specialization
template<int I>
struct TuplePrinter {
    template<typename... A>
    static std::string toString(const std::tuple<A...>& values) {
        // since C++ doesn't allow forward loops in compile-time,
        // we recurse from N down, and print from 0 up
        return ::toString(std::get<sizeof...(A) - I>(values)) +
            ", " + TuplePrinter<I - 1>::toString(values);
    }
};

// recursion terminator. This ends at 1 because the last value to be printed
// is the number of elements in the tuple - 1. It's "backwards" recursion
template<>
struct TuplePrinter<1> {
    template<typename... A>
    static std::string toString(const std::tuple<A...>& values) {
        return ::toString(std::get<sizeof...(A) - 1>(values));
    }
};


// template specialization for std::tuple
template<typename... A>
std::string toString(const std::tuple<A...>& values) {
    return std::string{"("} + TuplePrinter<sizeof...(A)>::toString(values) + ")";
}


// primary template, default to false for every type
template<typename, typename = premock::void_t<>>
struct CanBeOverwritten: std::false_type {};

// detects when ostream operator<< works on a type
template<typename T>
struct CanBeOverwritten<T, premock::void_t<decltype(memcpy(std::declval<T>(), std::declval<T>(), 0))>>: std::true_type {};



/**
 A mock class to verify expectations of how the mock was called.
 Supports verification of the number of times called, setting
 return values and checking the values passed to it.
 */
template<typename T>
class Mock {
public:

    using ReturnType = typename MockScope<T>::ReturnType;
    using ParamTupleType = typename StdFunctionTraits<T>::TupleType;
    using OutputTupleType = typename StdFunctionTraits<T>::OutputTupleType;

    /**
     Enables checks on parameter values passed to function invocations
     */
    class ParamChecker {
    public:

        ParamChecker(std::deque<ParamTupleType> v):_values{v} {}

        /**
         Verifies the parameter values passed in the last invocation
         */
        template<typename... A>
        void withValues(A&&... args) {
            withValues({std::make_tuple(std::forward<A>(args)...)}, _values.size() - 1, _values.size());
        }

        /**
         Verifies the parameter values passed in all invocations since the last
         call to `expectCalled`, optionally between the start-th and end-th
         invocations
         */
        void withValues(std::initializer_list<ParamTupleType> args,
                        size_t start = 0, size_t end = 0) {

            if(end == 0) end = _values.size();
            std::deque<ParamTupleType> expected{std::move(args)};

            const auto expectedArgsSize = end - start;
            if(expected.size() != expectedArgsSize)
                throw std::logic_error("ParamChecker::withValues called with " +
                                       capitalize(expected.size(), "value") + ", expected " +
                                       std::to_string(expectedArgsSize));

            for(size_t i = start; i < end; ++i) {
                const auto& expValues = expected.at(i - start);
                const auto& actValues = _values.at(i);
                if(expValues != actValues)
                    throw MockException(std::string{"Invocation values do not match\n"} +
                                        "Expected: " + toString(expValues) + "\n" +
                                        "Actual:   " + toString(actValues) + "\n");
            }
        }

    private:

        std::deque<ParamTupleType> _values;
        std::string capitalize(int val, const std::string& word) {
            return val == 1 ? "1 " + word : std::to_string(val) + " " + word + "s";
        }
    };

    /**
     Constructor. Pass in the mock_ std::function to replace until
     the end of scope.
     */
    Mock(T& func):
        _mockScope{func,
            [this](auto... args) {

                _values.emplace_back(args...);

                this->setOutputParameters<sizeof...(args)>(args...);

                auto ret = _returns.at(0);
                if(1 < _returns.size()) _returns.pop_front();

                // it may seem odd to cast to the return type here, but the only
                // reason it's needed is when the mocked function's return type
                // is void. This makes it work
                return static_cast<ReturnType>(ret);
        }},
    _returns(1) {

    }

    /**
     Set the next N return values
     */
    template<typename... A>
    void returnValue(A&&... args) {
        _returns.clear();
        returnValueImpl(std::forward<A>(args)...);
    }

    /**
     Set the output parameter at position I
    */
    template<int I, typename A>
    void outputParam(A valuePtr) {
        outputArray<I>(valuePtr, 1);
    }

    template<size_t I, typename A>
    void outputArray(A ptr, size_t length) {
        std::get<I>(_outputs) = Slice<A>{ptr, length * sizeof(*ptr)};
    }

    /**
     Verify the mock was called n times. Returns a ParamChecker so that
     assertions can be made on the passed in parameter values
     */
    ParamChecker expectCalled(size_t n = 1) {

        if(_values.size() != n)
            throw MockException(std::string{"Was not called enough times\n"} +
                                "Expected: " + std::to_string(n) + "\n" +
                                "Actual:   " + std::to_string(_values.size()) + "\n");
        auto ret = _values;
        _values.clear();
        return ret;
    }

    template<int N, typename A, typename... As>
    std::enable_if_t<std::is_pointer<std::remove_reference_t<A>>::value && CanBeOverwritten<A>::value>
    setOutputParameters(A outputParam, As&&... args) {
        setOutputParametersImpl<N>(std::forward<A>(outputParam));
        setOutputParameters<N - 1>(std::forward<As>(args)...);
    }

private:

    MockScope<T> _mockScope;
    // the _returns would be static if'ed out for void return type if it were allowed in C++
    // since it isn't, we change the return type to void* in that case
    std::deque<std::conditional_t<std::is_void<ReturnType>::value, void*, ReturnType>> _returns;
    std::deque<ParamTupleType> _values;
    OutputTupleType _outputs{};

    template<typename A, typename... As>
    void returnValueImpl(A&& arg, As&&... args) {
        _returns.emplace_back(arg);
        returnValueImpl(std::forward<As>(args)...);
    }

    void returnValueImpl() {}

    template<int N, typename A>
    std::enable_if_t<std::is_pointer<std::remove_reference_t<A>>::value && CanBeOverwritten<A>::value>
    setOutputParameters(A&& outputParam) {
        setOutputParametersImpl<N>(std::forward<A>(outputParam));
    }

    template<int N, typename A>
    void setOutputParametersImpl(A outputParam) {
        constexpr auto paramIndex = std::tuple_size<decltype(_outputs)>::value - N;
        const auto& data = std::get<paramIndex>(_outputs);
        if(data.ptr && data.length) {
            memcpy(outputParam, data.ptr, data.length);
        }
    }


    template<int N, typename A, typename... As>
    std::enable_if_t<!std::is_pointer<std::remove_reference_t<A>>::value || !CanBeOverwritten<A>::value>
    setOutputParameters(A, As&&... args) {
        setOutputParameters<N - 1>(std::forward<As>(args)...);
    }

    template<int N, typename A>
    std::enable_if_t<!std::is_pointer<std::remove_reference_t<A>>::value || !CanBeOverwritten<A>::value>
    setOutputParameters(A) { }

    template<int N>
    void setOutputParameters() { }
};



/**
 Helper function to create a Mock<T>
 */
template<typename T>
Mock<T> mock(T& func) {
    return {func};
}

/**
 Helper macro to mock a particular "real" function
 */
#define MOCK(func) mock(mock_##func)

/**
 Traits class for function pointers
 */
template<typename>
struct FunctionTraits;

template<typename R, typename... A>
struct FunctionTraits<R(*)(A...)> {
    /**
     The corresponding type of a std::function that the function could be assigned to
     */
    using StdFunctionType = std::function<R(A...)>;

    /**
     The return type of the function
     */
    using ReturnType = R;

    /**
     Helper struct to get the type of the Nth argument to the function
     */
    template<int N>
    struct Arg {
        using Type = typename std::tuple_element<N, std::tuple<A...>>::type;
    };
};


/**
 Declares a mock function for "real" function func. This is simply the
 declaration, the implementation is done with IMPL_C_MOCK. If foo has signature:

 int foo(int, float);

 Then DECL_MOCK(foo) is:

 extern std::function<int(int, float)> mock_foo;
 */
#define DECL_MOCK(func) extern "C" FunctionTraits<decltype(&func)>::StdFunctionType mock_##func

/**
 Definition of the std::function that will store the implementation. e.g. given:

 int foo(int, float);

 Then MOCK_STORAGE(foo) is:

 std::function<int(int, float)> mock_foo;
 */
#define MOCK_STORAGE(func) decltype(mock_##func) mock_##func


/**
 Definition of the std::function that will store the implementation.
 Defaults to the "real" function. e.g. given:

 int foo(int, float);

 Then MOCK_STORAGE_DEFAULT(foo) is:

 std::function<int(int, float)> mock_foo = foo;
 */
#define MOCK_STORAGE_DEFAULT(func) decltype(mock_##func) mock_##func = func

/**
 A name for the ut_ function argument at position index
 */
#define UT_FUNC_ARG(index) arg_##index

/**
 Type and name for ut_ function argument at position index. If foo has signature:

 int foo(int, float);

 Then:

 UT_FUNC_TYPE_AND_ARG(foo, 0) = int arg_0
 UT_FUNC_TYPE_AND_ARG(foo, 1) = float arg_1
 */
#define UT_FUNC_TYPE_AND_ARG(func, index) FunctionTraits<decltype(&func)>::Arg<index>::Type UT_FUNC_ARG(index)


/**
 Helper macros to generate the code for the ut_ functions.

 UT_FUNC_ARGS_N generates the parameter list in the function declaration,
 with types and parameter names, e.g. (int arg0, float arg1, ...)

 UT_FUNC_FWD_N generates just the parameter names so that the ut_
 function can forward the call to the equivalent mock_, e.g.
 (arg0, arg1, ...)

 */
#define UT_FUNC_ARGS_0(func)
#define UT_FUNC_FWD_0

#define UT_FUNC_ARGS_1(func) UT_FUNC_ARGS_0(func) UT_FUNC_TYPE_AND_ARG(func, 0)
#define UT_FUNC_FWD_1 UT_FUNC_FWD_0 UT_FUNC_ARG(0)

#define UT_FUNC_ARGS_2(func) UT_FUNC_ARGS_1(func), UT_FUNC_TYPE_AND_ARG(func, 1)
#define UT_FUNC_FWD_2 UT_FUNC_FWD_1, UT_FUNC_ARG(1)

#define UT_FUNC_ARGS_3(func) UT_FUNC_ARGS_2(func), UT_FUNC_TYPE_AND_ARG(func, 2)
#define UT_FUNC_FWD_3 UT_FUNC_FWD_2, UT_FUNC_ARG(2)

#define UT_FUNC_ARGS_4(func) UT_FUNC_ARGS_3(func), UT_FUNC_TYPE_AND_ARG(func, 3)
#define UT_FUNC_FWD_4 UT_FUNC_FWD_3, UT_FUNC_ARG(3)

#define UT_FUNC_ARGS_5(func) UT_FUNC_ARGS_4(func), UT_FUNC_TYPE_AND_ARG(func, 4)
#define UT_FUNC_FWD_5 UT_FUNC_FWD_4, UT_FUNC_ARG(4)

#define UT_FUNC_ARGS_6(func) UT_FUNC_ARGS_5(func), UT_FUNC_TYPE_AND_ARG(func, 5)
#define UT_FUNC_FWD_6 UT_FUNC_FWD_5, UT_FUNC_ARG(5)

#define UT_FUNC_ARGS_7(func) UT_FUNC_ARGS_6(func), UT_FUNC_TYPE_AND_ARG(func, 6)
#define UT_FUNC_FWD_7 UT_FUNC_FWD_6, UT_FUNC_ARG(6)

#define UT_FUNC_ARGS_8(func) UT_FUNC_ARGS_7(func), UT_FUNC_TYPE_AND_ARG(func, 7)
#define UT_FUNC_FWD_8 UT_FUNC_FWD_7, UT_FUNC_ARG(7)

#define UT_FUNC_ARGS_9(func) UT_FUNC_ARGS_8(func), UT_FUNC_TYPE_AND_ARG(func, 8)
#define UT_FUNC_FWD_9 UT_FUNC_FWD_8, UT_FUNC_ARG(8)

#define UT_FUNC_ARGS_10(func) UT_FUNC_ARGS_9(func), UT_FUNC_TYPE_AND_ARG(func, 9)
#define UT_FUNC_FWD_10 UT_FUNC_FWD_9, UT_FUNC_ARG(9)

/**
 The implementation of the C++ mock for function func. This version makes the mock
 function default to the real implementation.

 This writes code to:

 1. Define the global mock_func std::function variable to hold the current implementation
 2. Assign this global to a pointer to the "real" implementation
 3. Writes the ut_ function called by production code to automatically forward to the mock

 The number of arguments that the function takes must be specified. This could be deduced
 with template metaprogramming but there is no way to feed that information back to
 the preprocessor. Since the production calling code thinks it's calling a function
 whose name begins with ut_, that function must exist or there'll be a linker error.
 The only way to not have to write the function by hand is to use the preprocessor.

 An example of a call to IMPL_MOCK(4, send) (where send is the BSD socket function)
 assuming the macro is used in an extern "C" block:

 extern "C" ssize_t ut_send(int arg0, const void* arg1, size_t arg2, int arg3) {
     return mock_send(arg0, arg1, arg2, arg3);
 }
 std::function<ssize_t(int, const void*, size_t, int)> mock_send = send


 */
#define IMPL_MOCK_DEFAULT(num_args, func) \
    FunctionTraits<decltype(&func)>::ReturnType ut_##func(UT_FUNC_ARGS_##num_args(func)) { \
        return mock_##func(UT_FUNC_FWD_##num_args); \
    } \
    MOCK_STORAGE_DEFAULT(func)

/**
 The implementation of the C++ mock for function func. This version makes the mock
 function default to nullptr.

 This writes code to:

 1. Define the global mock_func std::function variable to hold the current implementation
 2. Assign this global to a pointer to the "real" implementation
 3. Writes the ut_ function called by production code to automatically forward to the mock

 The number of arguments that the function takes must be specified. This could be deduced
 with template metaprogramming but there is no way to feed that information back to
 the preprocessor. Since the production calling code thinks it's calling a function
 whose name begins with ut_, that function must exist or there'll be a linker error.
 The only way to not have to write the function by hand is to use the preprocessor.

 An example of a call to IMPL_MOCK(4, send) (where send is the BSD socket function)
 assuming the macro is used in an extern "C" block:

 extern "C" ssize_t ut_send(int arg0, const void* arg1, size_t arg2, int arg3) {
     return mock_send(arg0, arg1, arg2, arg3);
 }
 std::function<ssize_t(int, const void*, size_t, int)> mock_send = send


 */
#define IMPL_MOCK(num_args, func) \
    FunctionTraits<decltype(&func)>::ReturnType ut_##func(UT_FUNC_ARGS_##num_args(func)) { \
        return mock_##func(UT_FUNC_FWD_##num_args); \
    } \
    MOCK_STORAGE(func)


#endif // MOCK_HPP_
