#ifndef __SKY_MACROS__
#define __SKY_MACROS__

#define i8 char
#define i08 char
#define u8 unsigned char
#define u08 unsigned char
#define i16 short
#define u16 unsigned short
#define i32 int
#define u32 unsigned int
#define i64 long long
#define u64 unsigned long long
#define f32 float
#define f64 double

#define MBError(text, type) (MessageBoxW(NULL, text, L"Error", MB_ICONERROR | type))
#define LOG (wprintf)

#endif