#include <cmath>
#include <cstdio>
int main(){volatile double a=0.01,b=7.0,c=0.5; double f=std::fma(a,b,c); double n=a*b; n=n+c; std::printf("fused=%.17g unfused=%.17g equal=%d\n",f,n,(int)(f==n));}
