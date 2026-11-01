#include "Tab5Calculator.h"
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <cctype>
namespace tab5calc {
namespace {
constexpr double pi=3.14159265358979323846;
struct Parser {
 const char *begin,*p; bool deg;double ans,mem;const char* error=nullptr;unsigned depth=0,steps=0;
 void space(){while(*p==' '||*p=='\t')++p;}
 double fail(const char* message){if(!error)error=message;return 0;}
 double expression(){double v=product();space();while(!error&&(*p=='+'||*p=='-')){char op=*p++;double r=product();v=op=='+'?v+r:v-r;space();}return v;}
 double product(){double v=unary();space();while(!error&&(*p=='*'||*p=='/'||*p=='%')){char op=*p++;double r=unary();if((op=='/'||op=='%')&&r==0)return fail("Division par zéro");v=op=='*'?v*r:op=='/'?v/r:std::fmod(v,r);space();}return v;}
 double unary(){space();if(++steps>2048||++depth>48){--depth;return fail("Expression trop complexe");}double v;if(*p=='+'||*p=='-'){char s=*p++;v=unary();if(s=='-')v=-v;}else v=power();--depth;return v;}
 double power(){double v=atom();space();if(!error&&*p=='^'){++p;v=std::pow(v,unary());}return v;}
 double atom(){space();if(*p=='('){++p;double v=expression();space();if(*p!=')')return fail("Parenthèse manquante");++p;return v;}
  if(std::isdigit(static_cast<unsigned char>(*p))||*p=='.'){char* end=nullptr;double v=std::strtod(p,&end);if(end==p)return fail("Nombre invalide");p=end;return v;}
  char name[24]{};unsigned n=0;
  while(std::isalpha(static_cast<unsigned char>(*p))||*p=='_'){if(n==23)return fail("Fonction inconnue");name[n++]=std::tolower(static_cast<unsigned char>(*p++));}
  if(!n)return fail("Expression invalide");
  if(!std::strcmp(name,"pi"))return pi;if(!std::strcmp(name,"e"))return std::exp(1.0);if(!std::strcmp(name,"ans"))return ans;if(!std::strcmp(name,"mem"))return mem;
  space();if(*p!='(')return fail("Fonction : parenthèses requises");++p;double x=expression();space();if(*p!=')')return fail("Parenthèse manquante");++p;
  double a=deg?x*pi/180:x;
  if(!strcmp(name,"sin"))return sin(a);if(!strcmp(name,"cos"))return cos(a);
  if(!strcmp(name,"tan")){if(fabs(cos(a))<1e-14)return fail("Valeur hors domaine");return tan(a);}
  if(!strcmp(name,"asin"))return asin(x)*(deg?180/pi:1);if(!strcmp(name,"acos"))return acos(x)*(deg?180/pi:1);if(!strcmp(name,"atan"))return atan(x)*(deg?180/pi:1);
  if(!strcmp(name,"sqrt"))return sqrt(x);if(!strcmp(name,"ln"))return log(x);if(!strcmp(name,"log"))return log10(x);if(!strcmp(name,"exp"))return exp(x);
  if(!strcmp(name,"abs"))return fabs(x);if(!strcmp(name,"floor"))return floor(x);if(!strcmp(name,"ceil"))return ceil(x);
  return fail("Fonction inconnue");
 }
};
const Unit allUnits[]={
 {"mm",0,0.001,0},{"cm",0,0.01,0},{"m",0,1,0},{"km",0,1000,0},{"in",0,0.0254,0},{"ft",0,0.3048,0},{"mi",0,1609.344,0},
 {"mg",1,0.000001,0},{"g",1,0.001,0},{"kg",1,1,0},{"lb",1,0.45359237,0},{"oz",1,0.028349523125,0},
 {"°C",2,1,273.15},{"°F",2,5.0/9.0,255.37222222222222},{"K",2,1,0},
 {"m²",3,1,0},{"ha",3,10000,0},{"km²",3,1000000,0},
 {"mL",4,0.001,0},{"L",4,1,0},{"m³",4,1000,0},
 {"m/s",5,1,0},{"km/h",5,1.0/3.6,0},{"mph",5,0.44704,0},
 {"s",6,1,0},{"min",6,60,0},{"h",6,3600,0},
 {"deg",7,pi/180,0},{"rad",7,1,0},
 {"J",8,1,0},{"kJ",8,1000,0},{"Wh",8,3600,0},{"kWh",8,3600000,0}
};
}
Result evaluate(const char* expression,bool degrees,double answer,double memory){
 if(!expression||std::strlen(expression)>2048)return {0,"Expression trop longue",0};
 Parser p{expression,expression,degrees,answer,memory};double v=p.expression();p.space();
 if(!p.error&&*p.p)p.error="Expression invalide";
 if(!p.error&&!std::isfinite(v))p.error="Valeur hors domaine";
 return {p.error?0:v,p.error,size_t(p.p-p.begin)};
}
const Unit* units(size_t& count){count=sizeof(allUnits)/sizeof(allUnits[0]);return allUnits;}
Result convert(double value,unsigned from,unsigned to){size_t count;auto* u=units(count);if(from>=count||to>=count||u[from].family!=u[to].family)return {0,"Unités incompatibles",0};
 double base=value*u[from].scale+u[from].offset;if(u[from].family==2&&base<0)return{0,"Température sous le zéro absolu",0};
 double result=(base-u[to].offset)/u[to].scale;if(!std::isfinite(result))return{0,"Valeur hors domaine",0};return{result,nullptr,0};}
}
