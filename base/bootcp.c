
/////////////////////////////////////////////////////////////////////
// Copyright (C) 2008 Department of Computer SCience & Engineering
// National Institute of Technology - Warangal
// Andhra Pradesh 
// INDIA
// http://www.nitw.ac.in
//
// This software is developed for software based DSM system Project
// This is an experimental platform, freely useable and modifiable
// 
//
// Team Members:
// Prof. T. Ramesh 			Chapram Sudhakar
// A. Sundara Ramesh  			T. Santhosi
// K. Suresh 				G. Konda Reddy
// Vivek Kumar 				T. Vijaya Laxmi
// Angad 				Jhahnavi
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
int main(int argc, char **argv)
{
	FILE *in, *out;
	char buf[513];

	in = fopen("boot", "r");
	out = fopen("fimage", "r+");
	if (in == NULL || out == NULL)
	{
		printf("Error: could not open one of boot ot fimage files\n");
		exit(0);
	}

	fread(buf, 1, 512, in);
	fwrite(buf, 1, 512, out);
	fclose(in);
	fclose(out);
	return 0;
}
