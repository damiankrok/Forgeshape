#include <cstdio>
int main(){ volatile size_t k=7; double a=0.5+0.01*(double)k; double b=0.5+0.01*7.0; printf("%.17g %.17g %d\n",a,b,a==b);}
