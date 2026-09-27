#pragma once

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Operation system

#if !defined( PIPELINE_OS_WINDOWS ) && ( defined( _WIN32 ) || defined( __WIN32__ ) || defined( _WIN64 ) || defined( __WIN64__ ) )
	#define PIPELINE_OS_WINDOWS 1
	#define PIPELINE_OS_LINUX   0
#elif !defined( PIPELINE_OS_LINUX ) && defined( __linux__ )
	#define PIPELINE_OS_LINUX   1
	#define PIPELINE_OS_WINDOWS 0
#else
	static_assert( false, "Unsupported Operation system" );
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Toolchain Compiler

#if !defined( PIPELINE_TOOLCHAIN_MSVC ) && defined( _MSC_VER )
    #define PIPELINE_TOOLCHAIN_MSVC  1
	#define PIPELINE_TOOLCHAIN_GCC   0
	#define PIPELINE_TOOLCHAIN_CLANG 0
#elif !defined( PIPELINE_TOOLCHAIN_CLANG ) && defined( __clang__ )
	#define PIPELINE_TOOLCHAIN_CLANG 1
	#define PIPELINE_TOOLCHAIN_MSVC  0
	#define PIPELINE_TOOLCHAIN_GCC   0
#elif !defined( PIPELINE_TOOLCHAIN_GCC ) && defined( __GNUC__ )
	#define PIPELINE_TOOLCHAIN_GCC   1
	#define PIPELINE_TOOLCHAIN_MSVC  0
	#define PIPELINE_TOOLCHAIN_CLANG 0
#else
	static_assert( false, "Unsupported Toolchain Compiler" );
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Architecture

#if !defined( PIPELINE_ARCH_X64 ) && ( defined( _M_X64 ) || defined( __x86_64__ ) || defined( __amd64__ ) )
	#define PIPELINE_ARCH_X64   1
	#define PIPELINE_ARCH_X86   0
	#define PIPELINE_ARCH_ARM64 0
	#define PIPELINE_ARCH_ARM32 0
#elif !defined( PIPELINE_ARCH_X86 ) && ( defined( _M_IX86 ) || defined( __i386__ ) )
	#define PIPELINE_ARCH_X86   1
	#define PIPELINE_ARCH_X64   0
	#define PIPELINE_ARCH_ARM64 0
	#define PIPELINE_ARCH_ARM32 0
#elif !defined( PIPELINE_ARCH_ARM64 ) && ( defined( _M_ARM64 ) || defined( __aarch64__ ) )
	#define PIPELINE_ARCH_ARM64 1
	#define PIPELINE_ARCH_X64   0
	#define PIPELINE_ARCH_X86   0
	#define PIPELINE_ARCH_ARM32 0
#elif !defined( PIPELINE_ARCH_ARM32 ) && ( defined( _M_ARM ) || defined( __arm__ ) )
	#define PIPELINE_ARCH_ARM32 1
	#define PIPELINE_ARCH_X64   0
	#define PIPELINE_ARCH_X86   0
	#define PIPELINE_ARCH_ARM64 0
#else
	static_assert( false, "Unsupported Architecture" );
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// DLL Export / Import

#if defined( PIPELINE_OS_WINDOWS ) && PIPELINE_OS_WINDOWS
    #if defined( PIPELINE_BUILD_SHARED )
        #define PIPELINE_API __declspec( dllexport )
    #elif defined( PIPELINE_USE_SHARED )
        #define PIPELINE_API __declspec( dllimport )
    #else
        #define PIPELINE_API
    #endif
#elif defined( __GNUC__ ) || defined( __clang__ )
    #if defined( PIPELINE_BUILD_SHARED )
        #define PIPELINE_API __attribute__( (visibility( "default" )) )
    #else
        #define PIPELINE_API
    #endif
#else
    #define PIPELINE_API
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////