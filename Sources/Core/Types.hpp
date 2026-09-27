#pragma once

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Types

using u8  = unsigned char;
using u16 = unsigned short;
using u32 = unsigned int;
using u64 = unsigned long long;

using i8  = signed char;
using i16 = signed short;
using i32 = signed int;
using i64 = signed long long;

using f32 = float;
using f64 = double;

static_assert( sizeof( u8  ) == 1, "Expected u8 to be 1 byte." );
static_assert( sizeof( u16 ) == 2, "Expected u16 to be 2 bytes." );
static_assert( sizeof( u32 ) == 4, "Expected u32 to be 4 bytes." );
static_assert( sizeof( u64 ) == 8, "Expected u64 to be 8 bytes." );

static_assert( sizeof( i8  ) == 1, "Expected i8 to be 1 byte." );
static_assert( sizeof( i16 ) == 2, "Expected i16 to be 2 bytes." );
static_assert( sizeof( i32 ) == 4, "Expected i32 to be 4 bytes." );
static_assert( sizeof( i64 ) == 8, "Expected i64 to be 8 bytes." );

static_assert( sizeof( f32 ) == 4, "Expected f32 to be 4 bytes." );
static_assert( sizeof( f64 ) == 8, "Expected f64 to be 8 bytes." );

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Limits

#define U8_MAX   0xFF
#define U8_MIN   0x00
#define U16_MAX  0xFFFF
#define U16_MIN  0x0000
#define U32_MAX  0xFFFFFFFF
#define U32_MIN  0x00000000
#define U64_MAX  0xFFFFFFFFFFFFFFFFULL
#define U64_MIN  0x0000000000000000ULL

#define I8_MAX   0x7F
#define I8_MIN   ( -0x80 )
#define I16_MAX  0x7FFF
#define I16_MIN  ( -0x8000 )
#define I32_MAX  0x7FFFFFFF
#define I32_MIN  ( -0x80000000 )
#define I64_MAX  0x7FFFFFFFFFFFFFFFLL
#define I64_MIN  ( -0x8000000000000000LL )

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Bytes Calculations

#define KB( x ) ( ( x )   * 1024ULL )
#define MB( x ) ( KB( x ) * 1024ULL )
#define GB( x ) ( MB( x ) * 1024ULL )
#define TB( x ) ( GB( x ) * 1024ULL )
#define PB( x ) ( TB( x ) * 1024ULL )
#define EB( x ) ( PB( x ) * 1024ULL )

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Optimization Macros

#define INLINE inline
#define STATIC static

#if defined( _MSC_VER )
    #define FOREVER_INLINE    __forceinline
    #define NEVER_INLINE      __declspec( noinline )
#elif defined( __GNUC__ ) || defined( __clang__ )
    #define FOREVER_INLINE    __attribute__( (always_inline) ) inline
    #define NEVER_INLINE      __attribute__( (noinline) )
#else
    #define FOREVER_INLINE    inline
    #define NEVER_INLINE
#endif

#if defined( __GNUC__ ) || defined( __clang__ )
    #define LIKELY( x )      __builtin_expect(!!( x ), 1 )
    #define UNLIKELY( x )    __builtin_expect(!!( x ), 0 )
#else
    #define LIKELY( x )      ( x )
    #define UNLIKELY( x )    ( x )
#endif

#if defined( _MSC_VER )
    #define RESTRICT         __restrict
#elif defined( __GNUC__ ) || defined( __clang__ )
    #define RESTRICT         __restrict__
#elif defined( __STDC_VERSION__ ) && __STDC_VERSION__ >= 199901L
    #define RESTRICT         restrict
#else
    #define RESTRICT
#endif

#if defined( _MSC_VER )
    #define ALIGN_TO( n )    __declspec( align( n ) )
#elif defined(__GNUC__) || defined(__clang__)
    #define ALIGN_TO( n )      __attribute__( (aligned( n )) )
#elif defined(__cplusplus) && __cplusplus >= 201103L
    #define ALIGN_TO( n )      alignas( n )
#elif defined( __STDC_VERSION__ ) && __STDC_VERSION__ >= 201112L
    #define ALIGN_TO( n )      _Alignas( n )
#else
    #define ALIGN_TO( n )
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////