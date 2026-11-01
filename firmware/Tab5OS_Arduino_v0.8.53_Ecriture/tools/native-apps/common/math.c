#include "runtime.h"
#include <math.h>
union bits64{uint64_t bits;double value;};union bits32{uint32_t bits;float value;};
static uint64_t operation(unsigned op,uint64_t a,uint64_t b){return ((const struct tab5_app_api4*)tab5_api)->floating(op,a,b);}
static double binary(unsigned op,double x,double y){union bits64 a={.value=x},b={.value=y},r={.bits=operation(op,a.bits,b.bits)};return r.value;}
double __adddf3(double x,double y){return binary(0,x,y);}double __subdf3(double x,double y){return binary(1,x,y);}double __muldf3(double x,double y){return binary(2,x,y);}double __divdf3(double x,double y){return binary(3,x,y);}
double __floatsidf(int32_t x){union bits64 r={.bits=operation(4,(uint32_t)x,0)};return r.value;}double __floatunsidf(uint32_t x){union bits64 r={.bits=operation(5,x,0)};return r.value;}
double __floatdidf(int64_t x){union bits64 r={.bits=operation(6,x,0)};return r.value;}double __floatundidf(uint64_t x){union bits64 r={.bits=operation(7,x,0)};return r.value;}
double __extendsfdf2(float x){union bits32 a={.value=x};union bits64 r={.bits=operation(8,a.bits,0)};return r.value;}
float __truncdfsf2(double x){union bits64 a={.value=x};union bits32 r={.bits=(uint32_t)operation(9,a.bits,0)};return r.value;}
int32_t __fixdfsi(double x){union bits64 a={.value=x};return operation(10,a.bits,0);}uint32_t __fixunsdfsi(double x){union bits64 a={.value=x};return operation(11,a.bits,0);}
int64_t __fixdfdi(double x){union bits64 a={.value=x};return operation(12,a.bits,0);}uint64_t __fixunsdfdi(double x){union bits64 a={.value=x};return operation(13,a.bits,0);}
double __negdf2(double x){union bits64 a={.value=x};a.bits^=UINT64_C(1)<<63;return a.value;}
double sin(double x){return binary(20,x,0);}double cos(double x){return binary(21,x,0);}double tan(double x){return binary(22,x,0);}double atan(double x){return binary(23,x,0);}double atan2(double x,double y){return binary(24,x,y);}double acos(double x){return binary(25,x,0);}double asin(double x){return binary(26,x,0);}double sqrt(double x){return binary(27,x,0);}double floor(double x){return binary(28,x,0);}double ceil(double x){return binary(29,x,0);}double fmod(double x,double y){return binary(30,x,y);}double pow(double x,double y){return binary(31,x,y);}double exp(double x){return binary(32,x,0);}double log(double x){return binary(33,x,0);}double ldexp(double x,int n){union bits64 a={.value=x},r={.bits=operation(34,a.bits,(uint32_t)n)};return r.value;}

float __addsf3(float a,float b){return a+b;}float __subsf3(float a,float b){return a-b;}float __mulsf3(float a,float b){return a*b;}float __divsf3(float a,float b){return a/b;}
float sinf(float x){return sin(x);}float cosf(float x){return cos(x);}float tanf(float x){return tan(x);}float atanf(float x){return atan(x);}float atan2f(float x,float y){return atan2(x,y);}float acosf(float x){return acos(x);}float asinf(float x){return asin(x);}float sqrtf(float x){return sqrt(x);}float floorf(float x){return floor(x);}float ceilf(float x){return ceil(x);}float fmodf(float x,float y){return fmod(x,y);}float powf(float x,float y){return pow(x,y);}float expf(float x){return exp(x);}float logf(float x){return log(x);}float ldexpf(float x,int n){return ldexp(x,n);}
float fabsf(float x){union bits32 v={.value=x};v.bits&=0x7fffffff;return v.value;}
double fabs(double x){union bits64 v={.value=x};v.bits&=UINT64_C(0x7fffffffffffffff);return v.value;}
