# Introduction

This is a "hello world" Vulkan application, used as a test bed for functional-style monadic
programming in C++. 

We develop a framework for structuring an application using the IO, ReaderIO and StateIO monads.

The included demo application is currently in the process of transitioning to use this framework.

There are tests that illustrate how to use the framework in the context of a Vulkan application.

Lambdas are eschewed in favour of named structs with operator(). This helps make compiler warnings
more bearable.

As much function composition is done at compile time as possible, which is the reason for the complex 
compiler warnings, since we end up with deeply nested types. I.e. we eschew type erasure.

## Development environment

The toolchain is configured as a nix flake dev shell.

Run all commands prefixed with `nix develop ./build_env --command`


## Building and running tests

```shell
nix develop ./build_env --command cmake --build build/Debug --target vulkandemo && \
  nix develop ./build_env --command ctest --test-dir build/Debug -V
```