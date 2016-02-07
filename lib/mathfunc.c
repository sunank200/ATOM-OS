#include "mklib.h"
#include "olib.h"

double round(double v);
double ceil(double v);
double floor(double v);
double ylog2x(double x, double y);
double p2minus1(double x);
double p2xstary(double x, double y);
double p2xminus1(double x);
double ceil(double x);
double floor(double x);
double ylog2x(double x, double y);

double modf(double x, double *iptr)
{
	double res;

	res = floor(x);
	*iptr = res;
	res = x - res;
	return res;
}

double floor(double x)
{
	double res;

	res = ceil(x);
	if (res > x) res = res - 1.0;
	return res;
}
static double pow_internal(double x, double y)
{
	double res;
	double t1, t2, t3;

	t1 = ylog2x(x,y);
	t2 = floor(t1);
	t1 = t1 - t2;
	t3 = p2xminus1(t1) + 1.0;
	res = p2xstary(t2, t3);
	return res;
}
double pow(double x, double y)
{
	double res = 0;

	if (x < 0)
	{
		if (y != round(y))
		{
			printf("pow: domain error\n");
			exit(0);
		}	
		else
		{
			res = pow_internal(-x, y);
			if (((int)y % 2) == 0) return res;
			else return -res;
		}
	}
	else res = pow_internal(x, y);

	return res;
}

//======== exp  =====

extern int	errno;
static double	log2e	= 1.4426950408889634073599247;
static double	sqrt2	= 1.4142135623730950488016887;
//static double	maxf	= 3.40282347e+38F;
//static double	minf	= -3.40282347e-38F;
static double	maxfd	= 1.7e+308;
static double	minfd	= -1.7e-308;
static double sq2p1	 =2.414213562373095048802e0;
static double sq2m1	 = .414213562373095048802e0;
static double pio2	 =1.570796326794896619231e0;
static double pio4	 = .785398163397448309615e0;
static double q4	 = .5895697050844462222791e2;
static double twoopi	= 0.63661977236758134308;
static double p0	=  .1357884097877375669092680e8;
static double p1	= -.4942908100902844161158627e7;
static double p2	=  .4401030535375266501944918e6;
static double p3	= -.1384727249982452873054457e5;
static double p4	=  .1459688406665768722226959e3;
static double q0	=  .8644558652922534429915149e7;
static double q1	=  .4081792252343299749395779e6;
static double q2	=  .9463096101538208180571257e4;
static double q3	=  .1326534908786136358911494e3;



static int powi(int x,int y)
{
	int i,result=1;
	for(i=0; i<y; i++)
		result = result * x;
	return result;
}

double ylog2x(double x, double y);
double log(double x)
{
	return ylog2x(x, (1/log2e));
}

double ldexp(double y, int exp)
{
	double t = exp;
	return p2xstary(t, y);
}

double frexp(double x, int *exp); // Fully in asm

double exp(double arg)
{
	double fract;
	double temp1, temp2, xsq;
	int ent;
	double result;

	if(arg == 0.)
		return(1.);
	if(arg < minfd)
		return(0.0);
	if(arg > maxfd) 
	{
		printf("Too large");
		return(maxfd);
	}
	arg *= log2e;
	ent = (int)arg;
	fract = (arg-ent) - 0.5;
	xsq = fract*fract;
	temp1 = ((p2*xsq+p1)*xsq+p0)*fract;
	temp2 = ((1.0*xsq+q2)*xsq+q1)*xsq + q0;

	result = (sqrt2*(temp2+temp1)/(temp2-temp1))*powi(2,ent);
	return result;
}

//========= sqrt ======

float sqrt(float number)
{
	long i;
	float x, y;
	const float f = 1.5F;
	x = number * 0.5F;
	y  = number;
	i  = * ( long * ) &y;
	i  = 0x5f3759df - ( i >> 1 );
	y  = * ( float * ) &i;
	y  = y * ( f - ( x * y * y ) );
	y  = y * ( f - ( x * y * y ) );
	return number * y;
}

//========= sin, cos, tan , asin , acos, atan

double atan2(double arg1,double arg2);
double atan(double arg);
static double satan(double arg);
static double xatan(double arg);

/*
	atan2 discovers what quadrant the angle
	is in and calls atan.
*/

double atan2(double arg1,double arg2)
{
	if((arg1+arg2)==arg1)
		if(arg1 >= 0.) 
			return(pio2);
		else 
			return(-pio2);
	else 
		if(arg2 <0.)
			if(arg1 >= 0.)
				return(pio2+pio2 - satan(-arg1/arg2));
			else
				return(-pio2-pio2 + satan(arg1/arg2));
		else 
			if(arg1>0)
				return(satan(arg1/arg2));
			else
				return(-satan(-arg1/arg2));
}

/*
	satan reduces its argument (known to be positive)
	to the range [0,0.414...] and calls xatan.
*/

static double satan(double arg)
{
	if(arg < sq2m1)
		return(xatan(arg));
	else 
		if(arg > sq2p1)
			return(pio2 - xatan(1.0/arg));
		else
			return(pio4 + xatan((arg-1.0)/(arg+1.0)));
}

/*
	xatan evaluates a series valid in the
	range [-0.414...,+0.414...].
*/

static double xatan(double arg)
{
	double argsq;
	double value;

	argsq = arg*arg;
	value = ((((p4*argsq + p3)*argsq + p2)*argsq + p1)*argsq + p0);
	value = value/(((((argsq + q4)*argsq + q3)*argsq + q2)*argsq + q1)*argsq + q0);
	return(value*arg);
}


/*
	atan makes its argument positive and
	calls the inner routine satan.
*/

double atan(double arg)
{
	if(arg>0)
		return(satan(arg));
	else
		return(-satan(-arg));
}

//////////////////////////////////


/*
	asin(arg) and acos(arg) return the arcsin, arccos,
	respectively of their arguments.

	Arctan is called after appropriate range reduction.
*/

double asin(double arg) 
{
	double sign, temp;
	sign = 1.;
	if(arg <0)
	{
		arg = -arg;
		sign = -1.;
	}

	if(arg > 1.)
	{
		return(0.);
	}

	temp = sqrt(1. - arg*arg);
	if(arg > 0.7)
		temp = pio2 - atan(temp/arg);
	else
		temp = atan(arg/temp);

	return(sign*temp);
}

double acos(double arg) 
{
	if((arg > 1.) || (arg < -1.)){
	return(0.);
}

return(pio2 - asin(arg));
}



static double sinus(double arg, int quad);
double cos(double arg)
{
	if(arg<0)
		arg = -arg;
	return(sinus(arg, 1));
}

double sin(double arg)
{
	return(sinus(arg, 0));
}

static double sinus(double arg, int quad)
{
	double e, f;
	double ysq;
	double x,y;
	int k;
	double temp1, temp2;

	x = arg;
	if(x<0) {
		x = -x;
		quad = quad + 2;
	}
	x = x*twoopi;	/*underflow?*/
	if(x>32764){
		y = modf(x,&e);
		e = e + quad;
		modf(0.25*e,&f);
		quad = e - 4*f;
	}else{
		k = x;
		y = x - k;
		quad = (quad + k) & 03;
	}
	if (quad & 01)
		y = 1-y;
	if(quad > 1)
		y = -y;

	ysq = y*y;
	temp1 = ((((p4*ysq+p3)*ysq+p2)*ysq+p1)*ysq+p0)*y;
	temp2 = ((((ysq+q3)*ysq+q2)*ysq+q1)*ysq+q0);
	return(temp1/temp2);
}

double tan(double arg)
{
	return (sin(arg)/cos(arg));
}

double fabs(double x)
{
	if (x < 0.0) return -x;
	else return x;
}
