/**
*	This will do a IIR or recursive filter based on an input list of coefficients.
*	version 2004-09-11
*	
*	Copyright 2004 Reid A. Woodbury Jr.
*	
*	Licensed under the Apache License, Version 2.0 (the "License");
*	you may not use this file except in compliance with the License.
*	You may obtain a copy of the License at
*	
*	   http://www.apache.org/licenses/LICENSE-2.0
*	
*	Unless required by applicable law or agreed to in writing, software
*	distributed under the License is distributed on an "AS IS" BASIS,
*	WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*	See the License for the specific language governing permissions and
*	limitations under the License.
*/

#include "ext.h"
#include "ext_obex.h"
#include "ext_strings.h"
#include "z_dsp.h"
#include <math.h>

void *iir_class;

#define IIR_MAX_POLES		64
#define IIR_COEF_MEM_SIZE	( IIR_MAX_POLES * sizeof(double) )

typedef struct
{
	t_pxobject l_obj;
	unsigned char poles;		//	number of poles
	unsigned char inputOrder;	//	order of coefficients
	double a0, *a, *b;			//	coefficients from input list
	double *x, *y;				//	delayed input and output values
} t_iir;

void *iir_new(t_symbol *o, short argc, const t_atom *argv);
void iir_free(t_iir *iir);
void iir_assist(t_iir *iir, void *b, long m, long a, char *s);

void iir_aabab(t_iir *iir);
void iir_aaabb(t_iir *iir);
void iir_print(t_iir *iir);
// void iir_dsp(t_iir *iir, t_signal **sp, short *count);
void iir_dsp64(t_iir *iir, t_object *dsp64, short *count, double samplerate, long maxvectorsize, long flags);
t_int *iir_perform(t_int *w);
void iir_perform64(t_iir *iir, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam);
inline t_double iir_dsp_loop(t_iir *iir, t_double x0);
void iir_clear(t_iir *x);
void iir_coeffs(t_iir *x, t_symbol *, short argc, t_atom *argv);

int C74_EXPORT main(void)
{
// 	t_class *c;
	iir_class = class_new("iir~", (method)iir_new, (method)iir_free, sizeof(t_iir), 0L, A_DEFSYM, 0);
	class_addmethod(iir_class, (method)iir_assist, "assist", A_CANT, 0);

// 	class_addmethod(iir_class, (method)iir_dsp, "dsp", A_CANT, 0);
	class_addmethod(iir_class, (method)iir_dsp64, "dsp64", A_CANT, 0);
	class_addmethod(iir_class, (method)iir_clear, "clear", 0);
	class_addmethod(iir_class, (method)iir_aabab, "aabab", 0);
	class_addmethod(iir_class, (method)iir_aaabb, "aaabb", 0);
	class_addmethod(iir_class, (method)iir_print, "print", 0);
	class_addmethod(iir_class, (method)iir_coeffs, "list", A_GIMME, 0);
	
	class_dspinit(iir_class);
	class_register(CLASS_BOX, iir_class);
	
// 	iir_class = c;
	
	return 0;
}

void *iir_new(t_symbol *o, short argc, const t_atom *argv)
{
	t_iir *x = NULL;
	long c;
	
	if( (x = (t_iir *)object_alloc(iir_class)) )
	{
		dsp_setup((t_pxobject *)x, 1);
		
		// one signal outlet
		outlet_new((t_object *)x, "signal");
		
		//	post message
		object_post((t_object *)x, "iir~ [aabab|aaabb]");
		
		//	output order
		if( argc==1 && !strcmp(atom_getsym(argv)->s_name, "aaabb") )
			x->inputOrder = 1;
		else
			x->inputOrder = 0;
		
		//	now we need pointers for our new data
		x->a = (double *)sysmem_newptr(IIR_COEF_MEM_SIZE);	//	input coefs
		x->b = (double *)sysmem_newptr(IIR_COEF_MEM_SIZE);	//	output coefs
		x->x = (double *)sysmem_newptr(IIR_COEF_MEM_SIZE);	//	delayed input
		x->y = (double *)sysmem_newptr(IIR_COEF_MEM_SIZE);	//	delayed output
		
		for ( c=0; c<=IIR_MAX_POLES; c++ )
			x->x[c] = x->y[c] = 0.0;
		
		if (!x->a || !x->b || !x->x || !x->y)
			object_error((t_object *)x, "BAD INIT POINTER");
		
		x->poles = 0;
	}

	return (x);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void iir_free(t_iir *iir)
{
	if(iir->a) sysmem_freeptr(iir->a);
	if(iir->b) sysmem_freeptr(iir->b);
	if(iir->x) sysmem_freeptr(iir->x);
	if(iir->y) sysmem_freeptr(iir->y);
	
	dsp_free((t_pxobject *)iir);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void iir_assist(t_iir *iir, void *b, long m, long a, char *s)
{
	if (m == 2)
		sprintf(s,"(signal) Output");
	else
		sprintf(s,"(signal) Input, List Input");
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void iir_aabab(t_iir *iir)
{
	iir->inputOrder = 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void iir_aaabb(t_iir *iir)
{
	iir->inputOrder = 1;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void iir_print(t_iir *iir)
{
	long p;
	if (iir->a)
	{
		object_post((t_object *)iir, "a[00] = % .15e", iir->a0);
		for ( p=0; p < iir->poles; p++ )
			object_post((t_object *)iir, "a[%02d] = % .15e   b[%02d] = % .15e", p+1, iir->a[p], p+1, iir->b[p]);
	}
	else
		object_post((t_object *)iir, "a[00] = 0.0");
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// void iir_dsp(t_iir *iir, t_signal **sp, short *count)
// {
// 	iir_clear(iir);
// 	dsp_add(iir_perform, 6, sp[0]->s_vec, sp[3]->s_vec, iir, sp[1]->s_vec, sp[2]->s_vec, sp[0]->s_n);
// }

void iir_dsp64(t_iir *iir, t_object *dsp64, short *count, double samplerate, long maxvectorsize, long flags)
{
	iir_clear(iir);
	dsp_add64(dsp64, (t_object*)iir, (t_perfroutine64)iir_perform64, 0, NULL);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
t_int *iir_perform(t_int *w)
{
	// assign from parameters
	t_float *in = (t_float *) w[1];
	t_float *out = (t_float *) w[2];
	t_iir *iir = (t_iir *) w[3];
	long n = (long) w[4];
	
	if (iir->l_obj.z_disabled)
		return (w+5);
	
	// DSP loops
	if (iir->a && iir->b && iir->x && iir->y) {
		while (n--) {
			*out++ = (t_float)iir_dsp_loop(iir, (t_double)*in++);
		}
	}
	else {	//	if pointers are no good...
		while (n--)
			*out++ = *in++; //	...just copy input to output
	}
	
	return (w+5);
}


void iir_perform64(t_iir *iir, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam)
{
	t_double *in = ins[0];
	t_double *out = outs[0];
	long n = sampleframes;
	
	if (iir->l_obj.z_disabled)
		return;
	
	// DSP loops
	if (iir->a && iir->b && iir->x && iir->y) {
		while (n--) {
			*out++ = iir_dsp_loop(iir, *in++);
		}
	}
	else {	//	if pointers are no good...
		while (n--)
			*out++ = *in++; //	...just copy input to output
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////
t_double iir_dsp_loop(t_iir *iir, t_double x0)
{
	double y0 = (double)x0 * iir->a0;
// 	register long p;
// 	
// 	for (p=0; p<iir->poles; p++) {
// 		y0 += iir->x[p] * iir->a[p];
// 		y0 += iir->y[p] * iir->b[p];
// 	}
	double* xEnd = iir->x + iir->poles;
	double* xp = iir->x;
	double* ap = iir->a;
	double* yp = iir->y;
	double* bp = iir->b;
	while ( xp < xEnd ) {
		y0 += *xp++ * *ap++;
		y0 += *yp++ * *bp++;
	}
	
	//	delay values one sample
// 	for (p=iir->poles-1; p>0; p--) {
// 		iir->x[p] = iir->x[p-1];
// 		iir->y[p] = iir->y[p-1];
// 	}
	if ( iir->poles >= 2 ) {
		xp = iir->x + (iir->poles-1);
		double* xp1 = xp - 1;
		yp = iir->y + (iir->poles-1);
		double* yp1 = yp - 1;
		while ( xp > iir->x ) {
			*xp-- = *xp1--;
			*yp-- = *yp1--;
		}
	}
	
	iir->x[0] = x0;
	iir->y[0] = y0;
	
	return y0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void iir_clear(t_iir *x)
{
	long c;
	for ( c=0; c<=IIR_MAX_POLES; c++ )
		x->y[c] = 0.0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void iir_coeffs (t_iir *x, t_symbol *s, short argc, t_atom *argv)
{
	long i, p, poles;
	
	for(i=0; i<argc; i++) {
		if (argv[i].a_type != A_FLOAT) {
			object_post((t_object *)x, "WARNING: All list members must be of type float or double.");
			x->a0 = 1.0;
			x->poles = 0.0;
			return;
		}
	}
	
	poles = argc/2; //	integer division
	
	x->a0 = argv[0].a_w.w_float;	//	the first is always the same no matter the order
	if (x->inputOrder) {	//	aaabb
		for (p=1; p<=poles && p<=IIR_MAX_POLES; p++) {
			x->a[p-1] = (double)argv[p].a_w.w_float;
			x->b[p-1] = (double)argv[poles+p].a_w.w_float;
		}
	}
	else {					//	aabab
		for (p=1; p<=poles && p<=IIR_MAX_POLES; p++) {
			x->a[p-1] = (double)argv[p*2-1].a_w.w_float;
			x->b[p-1] = (double)argv[p*2].a_w.w_float;
		}
	}
	
	if ( poles > IIR_MAX_POLES )
		x->poles = IIR_MAX_POLES;
	else {
		x->poles = poles;
		for (p=poles; p<IIR_MAX_POLES; p++) {
			x->a[p] = x->b[p] = 0.0;
		}
	}
}
