// Copyright (c) 2015-2016, Massachusetts Institute of Technology
// Copyright (c) 2016-2017 Sandia Corporation
// Copyright (c) 2017 NTESS, LLC.

// This file is part of the Compressed Continuous Computation (C3) Library
// Author: Alex A. Gorodetsky 
// Contact: alex@alexgorodetsky.com

// All rights reserved.

// Redistribution and use in source and binary forms, with or without modification, 
// are permitted provided that the following conditions are met:

// 1. Redistributions of source code must retain the above copyright notice, 
//    this list of conditions and the following disclaimer.

// 2. Redistributions in binary form must reproduce the above copyright notice, 
//    this list of conditions and the following disclaimer in the documentation 
//    and/or other materials provided with the distribution.

// 3. Neither the name of the copyright holder nor the names of its contributors 
//    may be used to endorse or promote products derived from this software 
//    without specific prior written permission.

// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE 
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER 
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, 
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

//Code


/** \file polynomials.c
 * Provides routines for manipulating orthogonal polynomials
*/

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <float.h>
#include <assert.h>
#include <complex.h>

#include "futil.h"

#ifndef ZEROTHRESH
#define ZEROTHRESH  1e0 * DBL_EPSILON
#endif

#include "stringmanip.h"
#include "array.h"
#include "fft.h"
#include "lib_quadrature.h"
#include "linalg.h"
#include "legtens.h"
#include "fourier.h"

inline static double fourierortho(size_t n){
    (void) n;
    return 2.0*M_PI;
}

/********************************************************//**
*   Initialize a Fourier Basis
*
*   \return p - polynomial
*************************************************************/
struct OrthPoly * init_fourier_poly(){
    
    struct OrthPoly * p;
    if ( NULL == (p = malloc(sizeof(struct OrthPoly)))){
        fprintf(stderr, "failed to allocate memory for poly exp.\n");
        exit(1);
    }
    p->ptype = FOURIER;
    p->an = NULL;
    p->bn = NULL;
    p->cn = NULL;
    
    p->lower = 0.0;
    p->upper = 2.0 * M_PI;

    p->const_term = 0.0;
    p->lin_coeff = 0.0;
    p->lin_const = 0.0;

    p->norm = fourierortho;

    return p;
}


/********************************************************//**
*   Evaluate a fourier expansion
*
*   \param[in] poly - polynomial expansion
*   \param[in] x    - location at which to evaluate
*
*   \return out - polynomial value
*************************************************************/
double fourier_expansion_eval(const struct OrthPolyExpansion * poly, double x)
{
    assert (poly != NULL);
    assert (poly->kristoffel_eval == 0);


    double x_norm = space_mapping_map(poly->space_transform, x);

    double complex val = cexp((double _Complex)I*x_norm);
    double complex out = 0.0;
    double complex basis = 1.0;
    for (size_t ii = 0; ii < poly->num_poly; ii++){        
        out = out + poly->ccoeff[ii]*basis;
        basis = basis * val;
    }

    basis = 1.0 / val;
    for (size_t ii = 1; ii < poly->num_poly; ii++){
        out = out + conj(poly->ccoeff[ii])*basis;        
        basis = basis / val;
    }
    
    return creal(out);
}

/********************************************************//**
*   Evaluate the derivative of orth normal polynomial expansion
*
*   \param[in] poly - pointer to orth poly expansion
*   \param[in] x    - location at which to evaluate
*
*
*   \return out - value of derivative
*************************************************************/
double fourier_expansion_deriv_eval(const struct OrthPolyExpansion * poly, double x)
{
    assert (poly != NULL);
    assert (poly->kristoffel_eval == 0);

    double x_norm = space_mapping_map(poly->space_transform,x);

    double complex val = cexp((double _Complex)I*x_norm);
    double complex out = 0.0;
    double complex basis = val;
    for (size_t ii = 1; ii < poly->num_poly; ii++){        
        out = out + poly->ccoeff[ii]*basis * ii * (double _Complex)I;
        basis = basis * val;
    }

    basis = 1.0 / val;
    for (size_t ii = 1; ii < poly->num_poly; ii++){
        out = out - conj(poly->ccoeff[ii])*basis * ii * (double _Complex)I;        
        basis = basis / val;
    }

    double rout = creal(out);
    rout *= space_mapping_map_deriv(poly->space_transform,x);
    return rout;
}

/********************************************************//**
   Compute an expansion for the derivtive

   \param[in] p - orthogonal polynomial expansion
   
   \return derivative
*************************************************************/
struct OrthPolyExpansion * fourier_expansion_deriv(const struct OrthPolyExpansion * p)
{
    if (p == NULL) return NULL;
    assert (p->kristoffel_eval == 0);

    double dx = space_mapping_map_deriv(p->space_transform, 0.0);
    struct OrthPolyExpansion * out = orth_poly_expansion_copy(p);
    out->ccoeff[0] = 0.0;
    for (size_t ii = 1; ii < p->num_poly; ii++){
        out->ccoeff[ii] *= (double _Complex)I*ii*dx;
    }
    return out;
}

/********************************************************//**
   Compute an expansion for the second derivative

   \param[in] p - orthogonal polynomial expansion
   
   \return derivative
*************************************************************/
struct OrthPolyExpansion * fourier_expansion_dderiv(const struct OrthPolyExpansion * p)
{
    if (p == NULL) return NULL;
    assert (p->kristoffel_eval == 0);

    double dx = space_mapping_map_deriv(p->space_transform, 0.0);
    struct OrthPolyExpansion * out = orth_poly_expansion_copy(p);
    out->ccoeff[0] = 0.0;
    for (size_t ii = 1; ii < p->num_poly; ii++){
        out->ccoeff[ii] *= -1.0*(double)ii*(double)ii*dx*dx;
    }
    return out;
}

/********************************************************//**
*  Approximating a function that can take a vector of points as
*  input
*
*  \param[in,out] poly - orthogonal polynomial expansion
*  \param[in]     f    - wrapped function
*  \param[in]     opts - approximation options
*
*  \return 0 - no problems, > 0 problem
*
*  \note  Maximum quadrature limited to 200 nodes
*************************************************************/
int
fourier_expansion_approx_vec(struct OrthPolyExpansion * poly,
                             struct Fwrap * f,
                             const struct OpeOpts * opts)
{
    (void) opts;
    int return_val = 1;
    /* printf("I am here!\n"); */


    /* size_t N = poly->num_poly; */
    size_t N = (poly->num_poly-1)*2;
    /* size_t N = 8; */
    size_t nquad = N;
    double frac = 2.0*M_PI/(nquad);


    double * quad_pts = calloc_double(N);
    for (size_t ii = 0; ii < nquad; ii++){
        quad_pts[ii] = frac * ii;
    }
    
    /* printf("what!\n"); */
    double * pts = calloc_double(nquad);
    double * fvals = calloc_double(nquad);
    for (size_t ii = 0; ii < nquad; ii++){
        pts[ii] = space_mapping_map_inverse(poly->space_transform,
                                            quad_pts[ii]);
        /* pts[ii] = -M_PI + ii * frac; */
    }
    

    // Evaluate functions

    return_val = fwrap_eval(nquad,pts,fvals,f);
    if (return_val != 0){
        return return_val;
    }

    /* printf("pts = "); dprint(nquad, pts); */
    /* printf("fvals = "); dprint(nquad, fvals); */

    double complex * coeff = malloc(nquad * sizeof(double complex));
    double complex * fvc = malloc(nquad * sizeof(double complex));
    for (size_t ii = 0; ii < nquad; ii++){
        fvc[ii] = fvals[ii];
    }

    int fft_res = fft(N, fvc, 1, coeff, 1);
    if (fft_res != 0){
        fprintf(stderr, "fft is not successfull\n");
        return 1;
    }

    /* printf("\n"); */
    /* for (size_t ii = 0; ii < N; ii++){ */
    /*     fprintf(stdout, "%3.5G %3.5G\n", creal(coeff[ii]), cimag(coeff[ii])); */
    /* } */


    /* printf("copy coeffs %zu= \n", poly->num_poly); */
    /* poly->num_poly = N/2; */
    poly->ccoeff[0] = coeff[0]/N;
    for (size_t ii = 1; ii < poly->num_poly; ii++){
        poly->ccoeff[ii] = coeff[ii]/N;
        /* printf("ii = %zu %3.5G %3.5G\n", ii, creal(poly->ccoeff[ii]), */
        /*        cimag(poly->ccoeff[ii])); */
    }
    
    /* poly->num_poly = N/2; */
    /* for (size_t ii = 0; ii < poly->num_poly/2+1; ii++){ */
    /*     /\* printf("ii = %zu\n", ii); *\/ */
    /*     poly->ccoeff[ii] = coeff[ii]/N; */
    /* } */
    /* for (size_t ii = 1; ii < poly->num_poly/2; ii++){ */
    /*     poly->ccoeff[ii+poly->num_poly/2] = coeff[poly->num_poly-ii]/N; */
    /* } */

    free(pts); pts = NULL;
    free(quad_pts); quad_pts = NULL;
    free(fvals); fvals = NULL;
    free(fvc); fvc = NULL;
    free(coeff); coeff = NULL;
    return return_val;
}





/* static inline double orth_poly_expansion_deriv_eval_for_approx(double x, void* poly){ */
/*     return orth_poly_expansion_deriv_eval(poly, x); */
/* } */

/* /\********************************************************\//\** */
/* *   Evaluate the derivative of an orthogonal polynomial expansion */
/* * */
/* *   \param[in] poly - pointer to orth poly expansion */
/* *   \param[in] x    - location at which to evaluate */
/* * */
/* * */
/* *   \return out - value of derivative */
/* *************************************************************\/ */
/* double cheb_expansion_deriv_eval(const struct OrthPolyExpansion * poly, double x) */
/* { */
/*     assert (poly != NULL); */
/*     assert (poly->kristoffel_eval == 0); */

/*     double dmult = space_mapping_map_deriv(poly->space_transform,x); */
/*     if (poly->num_poly == 1){ */
/*         return 0.0; */
/*     } */
/*     else if (poly->num_poly == 2){ */
/*         return poly->coeff[1] * dmult; */
/*     } */

/*     double x_norm = space_mapping_map(poly->space_transform,x); */
    
/*     if (poly->num_poly == 3){ */
/*         return (poly->coeff[1] + poly->coeff[2] * 4 * x_norm) * dmult; */
/*     } */

/*     double * cheb_eval = calloc_double(poly->num_poly); */
/*     double * cheb_evald = calloc_double(poly->num_poly); */
/*     cheb_eval[0] = 1.0; */
/*     cheb_eval[1] = x_norm; */
/*     cheb_eval[2] = 2.0*x_norm*cheb_eval[1] - cheb_eval[0]; */
    
/*     cheb_evald[0] = 0.0; */
/*     cheb_evald[1] = 1.0; */
/*     cheb_evald[2] = 4.0*x_norm; */

/*     double out = poly->coeff[1]*cheb_evald[1] + poly->coeff[2]*cheb_evald[2]; */
/*     for (size_t ii = 3; ii < poly->num_poly; ii++){ */
/*         cheb_eval[ii] = 2.0 * x_norm * cheb_eval[ii-1] - cheb_eval[ii-2]; */
/*         cheb_evald[ii] = 2.0 * cheb_eval[ii-1] + 2.0 * x_norm * cheb_evald[ii-1] -  */
/*             cheb_evald[ii-2]; */
/*         out += poly->coeff[ii]*cheb_evald[ii]; */
/*     } */

/*     out *= dmult; */
/*     free(cheb_eval); cheb_eval = NULL; */
/*     free(cheb_evald); cheb_evald = NULL; */
/*     return out; */
/* } */

/* static inline double cheb_expansion_deriv_eval_for_approx(double x, void* poly){ */
/*     return cheb_expansion_deriv_eval(poly, x); */
/* } */

/* /\********************************************************\//\** */
/* *   Evaluate the derivative of an orth poly expansion */
/* * */
/* *   \param[in] p - orthogonal polynomial expansion */
/* *    */
/* *   \return derivative */
/* * */
/* *   \note */
/* *       Could speed this up slightly by using partial sum */
/* *       to keep track of sum of coefficients */
/* *************************************************************\/ */
/* struct OrthPolyExpansion * */
/* orth_poly_expansion_deriv(struct OrthPolyExpansion * p) */
/* { */
/*     if (p == NULL) return NULL; */
/*     assert (p->kristoffel_eval == 0); */

/*     orth_poly_expansion_round(&p); */
            
/*     struct OrthPolyExpansion * out = NULL; */

/*     out = orth_poly_expansion_copy(p); */
/*     for (size_t ii = 0; ii < out->nalloc; ii++){ */
/*         out->coeff[ii] = 0.0; */
/*     } */
/*     if (p->num_poly == 1){ */
/*         orth_poly_expansion_round(&out); */
/*         return out; */
/*     } */

/*     out->num_poly -= 1; */
/*     if (p->p->ptype == LEGENDRE){ */
/*     /\* if (1 == 0){ *\/ */

/*         double dtransform_dx = space_mapping_map_deriv(p->space_transform,0.0); */
/*         for (size_t ii = 0; ii < p->num_poly-1; ii++){ // loop over coefficients */
/*             for (size_t jj = ii+1; jj < p->num_poly; jj+=2){ */
/*                 /\* out->coeff[ii] += p->coeff[jj]; *\/ */
/*                 out->coeff[ii] += p->coeff[jj]*sqrt(2*jj+1); */
/*             } */
/*             /\* out->coeff[ii] *= (double) ( 2 * (ii) + 1) * dtransform_dx; *\/ */
/*             out->coeff[ii] *= sqrt((double) ( 2 * (ii) + 1))* dtransform_dx; */
/*         } */
/*     } */
/*     else if (p->p->ptype == CHEBYSHEV){ */
/*         orth_poly_expansion_approx(cheb_expansion_deriv_eval_for_approx, p, out);       */
/*     } */
/*     else{ */
/*         orth_poly_expansion_approx(orth_poly_expansion_deriv_eval_for_approx, p, out);       */
/*     } */

/*     orth_poly_expansion_round(&out); */
/*     return out; */
/* } */

/* /\********************************************************\//\** */
/* *   Evaluate the second derivative of a chebyshev expansion */
/* * */
/* *   \param[in] poly - pointer to orth poly expansion */
/* *   \param[in] x    - location at which to evaluate */
/* * */
/* * */
/* *   \return out - value of derivative */
/* *************************************************************\/ */
/* double cheb_expansion_dderiv_eval(const struct OrthPolyExpansion * poly, double x) */
/* { */
/*     assert (poly != NULL); */
/*     assert (poly->kristoffel_eval == 0); */

    
/*     double dmult = space_mapping_map_deriv(poly->space_transform,x); */
/*     if (poly->num_poly <= 2){ */
/*         return 0.0; */
/*     } */
/*     else if (poly->num_poly == 3){ */
/*         return poly->coeff[2] * 4.0; */
/*     } */

/*     double x_norm = space_mapping_map(poly->space_transform,x); */
/*     /\* printf("x_norm = %3.15G\n", x_norm); *\/ */
    
/*     if (poly->num_poly == 4){ */
/*         /\* printf("here yo!\n"); *\/ */
/*         return (poly->coeff[2] * 4 + poly->coeff[3]*24.0*x_norm) * dmult * dmult; */
/*     } */

/*     double * cheb_eval   = calloc_double(poly->num_poly); */
/*     double * cheb_evald  = calloc_double(poly->num_poly); */
/*     double * cheb_evaldd = calloc_double(poly->num_poly); */
/*     cheb_eval[0] = 1.0; */
/*     cheb_eval[1] = x_norm; */
/*     cheb_eval[2] = 2.0*x_norm*cheb_eval[1] - cheb_eval[0]; */
/*     cheb_eval[3] = 2.0*x_norm*cheb_eval[2] - cheb_eval[1]; */
    
/*     cheb_evald[0] = 0.0; */
/*     cheb_evald[1] = 1.0; */
/*     cheb_evald[2] = 4.0*x_norm; */
/*     cheb_evald[3] = 2.0 * cheb_eval[2] + 2.0 * x_norm * cheb_evald[2] - cheb_evald[1]; */

/*     cheb_evaldd[0] = 0.0; */
/*     cheb_evaldd[1] = 0.0; */
/*     cheb_evaldd[2] = 4.0; */
/*     cheb_evaldd[3] = 24.0 * x_norm; */

/*     double out = poly->coeff[2]*cheb_evaldd[2] + poly->coeff[3]*cheb_evaldd[3]; */
/*     for (size_t ii = 4; ii < poly->num_poly; ii++){ */
/*         cheb_eval[ii] = 2.0 * x_norm * cheb_eval[ii-1] - cheb_eval[ii-2]; */
/*         cheb_evald[ii] = 2.0 * cheb_eval[ii-1] + 2.0 * x_norm * cheb_evald[ii-1] -  */
/*             cheb_evald[ii-2]; */
/*         cheb_evaldd[ii] = 4.0 * cheb_evald[ii-1] + 2.0 * x_norm * cheb_evaldd[ii-1] - */
/*             cheb_evaldd[ii-2]; */

/*         out += poly->coeff[ii]*cheb_evaldd[ii]; */
/*     } */

/*     out *= dmult*dmult; */
/*     /\* if (fabs(x_norm) > 0.999){ *\/ */
/*     /\*     printf("out = %3.15G\n", out); *\/ */
/*     /\* } *\/ */
/*     free(cheb_eval); cheb_eval = NULL; */
/*     free(cheb_evald); cheb_evald = NULL; */
/*     free(cheb_evaldd); cheb_evaldd = NULL; */
/*     return out; */
/* } */

/* static inline double cheb_expansion_dderiv_eval_for_approx(double x, void* poly){ */
/*     return cheb_expansion_dderiv_eval(poly, x); */
/* } */
/* /\********************************************************\//\** */
/* *   Evaluate the second derivative of an orth poly expansion */
/* * */
/* *   \param[in] p - orthogonal polynomial expansion */
/* *    */
/* *   \return derivative */
/* * */
/* *   \note */
/* *       Could speed this up slightly by using partial sum */
/* *       to keep track of sum of coefficients */
/* *************************************************************\/ */
/* struct OrthPolyExpansion * */
/* orth_poly_expansion_dderiv(struct OrthPolyExpansion * p) */
/* { */
/*     if (p == NULL) return NULL; */
/*     assert (p->kristoffel_eval == 0); */

/*     orth_poly_expansion_round(&p); */
            
/*     struct OrthPolyExpansion * out = NULL; */

/*     out = orth_poly_expansion_copy(p); */
/*     for (size_t ii = 0; ii < out->nalloc; ii++){ */
/*         out->coeff[ii] = 0.0; */
/*     } */
/*     if (p->num_poly < 2){ */
/*         return out; */
/*     } */

/*     /\* printf("lets go!\n"); *\/ */
/*     /\* dprint(p->num_poly, p->coeff); *\/ */
/*     out->num_poly -= 2; */
/*     if (p->p->ptype == CHEBYSHEV){ */
/*         orth_poly_expansion_approx(cheb_expansion_dderiv_eval_for_approx, p, out);               */
/*     } */
/*     else{ */
        
/*         struct OrthPolyExpansion * temp = orth_poly_expansion_deriv(p); */
/*         orth_poly_expansion_free(out); */
/*         out = orth_poly_expansion_deriv(temp); */
/*         orth_poly_expansion_free(temp); temp = NULL; */
/*         /\* fprintf(stderr, "Cannot yet take second derivative for polynomial of type %d\n", *\/ */
/*         /\*         p->p->ptype); *\/ */
/*         /\* exit(1); *\/ */
/*     } */

/*     orth_poly_expansion_round(&out); */
/*     return out; */
/* } */

/* /\********************************************************\//\** */
/*    Take a second derivative and enforce periodic bc */
/* **************************************************************\/ */
/* struct OrthPolyExpansion * orth_poly_expansion_dderiv_periodic(const struct OrthPolyExpansion * f) */
/* { */
/*     (void)(f); */
/*     NOT_IMPLEMENTED_MSG("orth_poly_expansion_dderiv_periodic"); */
/*     exit(1); */
/* } */

/* /\********************************************************\//\** */
/* *   free the memory of an orthonormal polynomial expansion */
/* * */
/* *   \param[in,out] p - orthogonal polynomial expansion */
/* *************************************************************\/ */
/* void orth_poly_expansion_free(struct OrthPolyExpansion * p){ */
/*     if (p != NULL){ */
/*         free_orth_poly(p->p); p->p = NULL; */
/*         space_mapping_free(p->space_transform); p->space_transform = NULL; */
/*         free(p->coeff); p->coeff = NULL; */
/*         free(p); p = NULL; */
/*     } */
/* } */

/* /\********************************************************\//\** */
/* *   Serialize orth_poly_expansion */
/* *    */
/* *   \param[in] ser       - location to which to serialize */
/* *   \param[in] p         - polynomial */
/* *   \param[in] totSizeIn - if not null then only return total size of  */
/* *                          array without serialization! if NULL then serialiaze */
/* * */
/* *   \return ptr : pointer to end of serialization */
/* *************************************************************\/ */
/* unsigned char * */
/* serialize_orth_poly_expansion(unsigned char * ser,  */
/*         struct OrthPolyExpansion * p, */
/*         size_t * totSizeIn) */
/* { */
/*     // order is  ptype->lower_bound->upper_bound->orth_poly->coeff */
    
    
/*     size_t totsize = sizeof(int) + 2*sizeof(double) +  */
/*                        p->num_poly * sizeof(double) + sizeof(size_t); */

/*     size_t size_mapping; */
/*     serialize_space_mapping(NULL,p->space_transform,&size_mapping); */
/*     totsize += size_mapping; */

/*     totsize += sizeof(int); // for kristoffel flag */
/*     if (totSizeIn != NULL){ */
/*         *totSizeIn = totsize; */
/*         return ser; */
/*     } */
/*     unsigned char * ptr = serialize_int(ser, p->p->ptype); */
/*     ptr = serialize_double(ptr, p->lower_bound); */
/*     ptr = serialize_double(ptr, p->upper_bound); */
/*     ptr = serialize_doublep(ptr, p->coeff, p->num_poly); */
/*     ptr = serialize_space_mapping(ptr,p->space_transform,NULL); */
/*     ptr = serialize_int(ptr,p->kristoffel_eval); */
/*     return ptr; */
/* } */

/* /\********************************************************\//\** */
/* *   Deserialize orth_poly_expansion */
/* * */
/* *   \param[in]     ser  - input string */
/* *   \param[in,out] poly - poly expansion */
/* * */
/* *   \return ptr - ser + number of bytes of poly expansion */
/* *************************************************************\/ */
/* unsigned char *  */
/* deserialize_orth_poly_expansion( */
/*     unsigned char * ser,  */
/*     struct OrthPolyExpansion ** poly) */
/* { */
    
/*     size_t num_poly = 0; */
/*     //size_t npoly_check = 0; */
/*     double lower_bound = 0; */
/*     double upper_bound = 0; */
/*     double * coeff = NULL; */
/*     struct OrthPoly * p = NULL; */
/*     struct SpaceMapping * map = NULL; */
/*     // order is  ptype->lower_bound->upper_bound->orth_poly->coeff */
/*     p = deserialize_orth_poly(ser); */
/*     unsigned char * ptr = ser + sizeof(int); */
/*     ptr = deserialize_double(ptr,&lower_bound); */
/*     ptr = deserialize_double(ptr,&upper_bound); */
/*     ptr = deserialize_doublep(ptr, &coeff, &num_poly); */
/*     ptr = deserialize_space_mapping(ptr, &map); */

/*     int kristoffel_eval; */
/*     ptr = deserialize_int(ptr,&kristoffel_eval); */
/*     if ( NULL == (*poly = malloc(sizeof(struct OrthPolyExpansion)))){ */
/*         fprintf(stderr, "failed to allocate memory for poly exp.\n"); */
/*         exit(1); */
/*     } */
/*     (*poly)->num_poly = num_poly; */
/*     (*poly)->lower_bound = lower_bound; */
/*     (*poly)->upper_bound = upper_bound; */
/*     (*poly)->coeff = coeff; */
/*     (*poly)->nalloc = num_poly;//+OPECALLOC; */
/*     (*poly)->p = p; */
/*     (*poly)->space_transform = map; */
/*     (*poly)->kristoffel_eval = kristoffel_eval; */
/*     return ptr; */
/* } */

/* /\********************************************************\//\** */
/*     Save an orthonormal polynomial expansion in text format */

/*     \param[in] f      - function to save */
/*     \param[in] stream - stream to save it to */
/*     \param[in] prec   - precision with which to save it */
/* ************************************************************\/ */
/* void orth_poly_expansion_savetxt(const struct OrthPolyExpansion * f, */
/*                                  FILE * stream, size_t prec) */
/* { */
/*     assert (f != NULL); */
/*     fprintf(stream,"%d ",f->p->ptype); */
/*     fprintf(stream,"%3.*G ",(int)prec,f->lower_bound); */
/*     fprintf(stream,"%3.*G ",(int)prec,f->upper_bound); */
/*     fprintf(stream,"%zu ",f->num_poly); */
/*     for (size_t ii = 0; ii < f->num_poly; ii++){ */
/*         if (prec < 100){ */
/*             fprintf(stream, "%3.*G ",(int)prec,f->coeff[ii]); */
/*         } */
/*     } */
/*     space_mapping_savetxt(f->space_transform,stream,prec); */
/*     fprintf(stream,"%d ",f->kristoffel_eval); */
/* } */

/* /\********************************************************\//\** */
/*     Load an orthonormal polynomial expansion in text format */

/*     \param[in] stream - stream to save it to */

/*     \return Orthonormal polynomial expansion */
/* ************************************************************\/ */
/* struct OrthPolyExpansion * */
/* orth_poly_expansion_loadtxt(FILE * stream)//l, size_t prec) */
/* { */

/*     enum poly_type ptype; */
/*     double lower_bound = 0; */
/*     double upper_bound = 0; */
/*     size_t num_poly; */

/*     int ptypeint; */
/*     int num = fscanf(stream,"%d ",&ptypeint); */
/*     ptype = (enum poly_type)ptypeint; */
/*     assert (num == 1); */
/*     num = fscanf(stream,"%lG ",&lower_bound); */
/*     assert (num == 1); */
/*     num = fscanf(stream,"%lG ",&upper_bound); */
/*     assert (num == 1); */
/*     num = fscanf(stream,"%zu ",&num_poly); */
/*     assert (num == 1); */

/*     struct OrthPolyExpansion * ope =  */
/*         orth_poly_expansion_init(ptype,num_poly,lower_bound,upper_bound); */

/*     for (size_t ii = 0; ii < ope->num_poly; ii++){ */
/*         num = fscanf(stream, "%lG ",ope->coeff+ii); */
/*         assert (num == 1); */
/*     } */

/*     space_mapping_free(ope->space_transform); ope->space_transform = NULL; */
/*     ope->space_transform = space_mapping_loadtxt(stream); */

/*     int kristoffel_eval; */
/*     num = fscanf(stream,"%d ",&kristoffel_eval); */
/*     assert (num == 1); */
/*     ope->kristoffel_eval = kristoffel_eval; */
    
/*     return ope; */
/* } */

/* /\********************************************************\//\** */
/* *   Convert an orthogonal polynomial expansion to a standard_polynomial */
/* * */
/* *   \param[in] p - polynomial */
/* * */
/* *   \return sp - standard polynomial */
/* *************************************************************\/ */
/* struct StandardPoly *  */
/* orth_poly_expansion_to_standard_poly(struct OrthPolyExpansion * p) */
/* { */
/*     struct StandardPoly * sp =  */
/*         standard_poly_init(p->num_poly,p->lower_bound,p->upper_bound); */
    
/*     double m = (p->p->upper - p->p->lower) / (p->upper_bound - p->lower_bound); */
/*     double off = p->p->upper - m * p->upper_bound; */

/*     size_t ii, jj; */
/*     size_t n = p->num_poly-1; */

/*     sp->coeff[0] = p->coeff[0]*p->p->const_term; */

/*     if (n > 0){ */
/*         sp->coeff[0]+=p->coeff[1] * (p->p->lin_const + p->p->lin_coeff * off); */
/*         sp->coeff[1]+=p->coeff[1] * p->p->lin_coeff * m; */
/*     } */
/*     if (n > 1){ */
        
/*         double * a = calloc_double(n+1); //n-2 poly */
/*         a[0] = p->p->const_term; */
/*         double * b = calloc_double(n+1); // n- 1poly */
/*         double * c = calloc_double(n+1); // n- 1poly */
/*         b[0] = p->p->lin_const + p->p->lin_coeff * off; */
/*         b[1] = p->p->lin_coeff * m; */
/*         for (ii = 2; ii < n+1; ii++){ // starting at the order 2 polynomial */
/*             c[0] = (p->p->bn(ii) + p->p->an(ii)*off) * b[0] +  */
/*                                                         p->p->cn(ii) * a[0]; */
/*             sp->coeff[0] += p->coeff[ii] * c[0]; */
/*             for (jj = 1; jj < ii-1; jj++){ */
/*                 c[jj] = (p->p->an(ii) * m) * b[jj-1] +  */
/*                         (p->p->bn(ii) + p->p->an(ii) * off) * b[jj] +  */
/*                         p->p->cn(ii) * a[jj]; */
/*                 sp->coeff[jj] += p->coeff[ii] * c[jj]; */
/*             } */
/*             c[ii-1] = (p->p->an(ii) * m) * b[ii-2] +  */
/*                             (p->p->bn(ii) + p->p->an(ii) * off) * b[ii-1]; */
/*             c[ii] = (p->p->an(ii) * m) * b[ii-1]; */
            
/*             sp->coeff[ii-1] += p->coeff[ii] * c[ii-1]; */
/*             sp->coeff[ii] += p->coeff[ii] * c[ii]; */

/*             memcpy(a, b, ii * sizeof(double)); */
/*             memcpy(b, c, (ii+1) * sizeof(double)); */
/*         } */
        
/*         free(a); */
/*         free(b); */
/*         free(c); */
/*     } */

/*     // Need to do something with lower and upper bounds!! */
/*     return sp; */
/* } */

/* /\********************************************************\//\** */
/* *   Evaluate each orthonormal polynomial expansion that is in an  */
/* *   array of generic functions  */
/* * */
/* *   \param[in]     n       - number of polynomials */
/* *   \param[in]     parr    - polynomial expansions */
/* *   \param[in]     x       - location at which to evaluate */
/* *   \param[in,out] out     - evaluations */
/* * */
/* *   \return 0 - successful */
/* * */
/* *   \note */
/* *   Assumes all functions have the same bounds */
/* *************************************************************\/ */
/* int orth_poly_expansion_arr_eval(size_t n, */
/*                                  struct OrthPolyExpansion ** parr,  */
/*                                  double x, double * out) */
/* { */


/*     if (parr[0]->kristoffel_eval == 1){ */
/*         for (size_t ii = 0; ii < n; ii++){ */
/*             out[ii] = orth_poly_expansion_eval(parr[ii],x); */
/*         } */
/*         return 0; */
/*     } */

/*     int all_same = 1; */
/*     enum poly_type ptype = parr[0]->p->ptype; */
/*     for (size_t ii = 1; ii < n; ii++){ */
/*         if (parr[ii]->p->ptype != ptype){ */
/*             all_same = 0; */
/*             break; */
/*         } */
/*     } */

/*     if ((all_same == 0) || (ptype == CHEBYSHEV)){ */
/*         for (size_t ii = 0; ii < n; ii++){ */
/*             out[ii] = orth_poly_expansion_eval(parr[ii],x); */
/*         } */
/*         return 0; */
/*     } */

/*     // all the polynomials are of the same type */
/*     size_t maxpoly = 0; */
/*     for (size_t ii = 0; ii < n; ii++){ */
/*         if (parr[ii]->num_poly > maxpoly){ */
/*             maxpoly = parr[ii]->num_poly; */
/*         } */
/*         out[ii] = 0.0; */
/*     } */


/*     double x_norm = space_mapping_map(parr[0]->space_transform,x); */

/*     // double out = 0.0; */
/*     double p[2]; */
/*     double pnew; */
/*     p[0] = parr[0]->p->const_term; */
/*     size_t iter = 0; */
/*     for (size_t ii = 0; ii < n; ii++){ */
/*         out[ii] += p[0] * parr[ii]->coeff[iter]; */
/*     } */
/*     iter++; */
/*     p[1] = parr[0]->p->lin_const + parr[0]->p->lin_coeff * x_norm; */
/*     for (size_t ii = 0; ii < n; ii++){ */
/*         if (parr[ii]->num_poly > iter){ */
/*             out[ii] += p[1] * parr[ii]->coeff[iter]; */
/*         } */
/*     } */
/*     iter++; */
/*     for (iter = 2; iter < maxpoly; iter++){ */
/*         pnew = eval_orth_poly_wp(parr[0]->p, p[0], p[1], iter, x_norm); */
/*         for (size_t ii = 0; ii < n; ii++){ */
/*             if (parr[ii]->num_poly > iter){ */
/*                 out[ii] += parr[ii]->coeff[iter] * pnew; */
/*             } */
/*         } */
/*         p[0] = p[1]; */
/*         p[1] = pnew; */
/*     } */

/*     return 0; */
/* } */

/* /\********************************************************\//\** */
/* *   Evaluate each orthonormal polynomial expansion that is in an  */
/* *   array of generic functions at an array of points */
/* * */
/* *   \param[in]     n          - number of polynomials */
/* *   \param[in]     parr       - polynomial expansions (all have the same bounds) */
/* *   \param[in]     N          - number of evaluations */
/* *   \param[in]     x          - locations at which to evaluate */
/* *   \param[in]     incx       - increment between locations */
/* *   \param[in,out] y          - evaluations */
/* *   \param[in]     incy       - increment between evaluations of array (at least n) */
/* * */
/* *   \return 0 - successful */
/* * */
/* *   \note */
/* *   Assumes all functions have the same bounds */
/* *************************************************************\/ */
/* int orth_poly_expansion_arr_evalN(size_t n, */
/*                                   struct OrthPolyExpansion ** parr, */
/*                                   size_t N, */
/*                                   const double * x, size_t incx, */
/*                                   double * y, size_t incy) */
/* { */
/*     if (parr[0]->kristoffel_eval == 1){ */
/*         for (size_t jj = 0; jj < N; jj++){ */
/*             for (size_t ii = 0; ii < n; ii++){ */
/*                 y[ii + jj * incy] = orth_poly_expansion_eval(parr[ii],x[jj*incx]); */
/*                 /\* printf("y = %G\n",y[ii+jj*incy]); *\/ */
/*             } */
/*         } */
/*         return 0; */
/*     } */
    

/*     for (size_t jj = 0; jj < N; jj++){ */
/*         for (size_t ii = 0; ii < n; ii++){ */
/*             y[ii + jj * incy] = 0.0; */
/*         } */
/*     } */

/*     int res; */
/*     for (size_t jj = 0; jj < N; jj++){ */
/*         res = orth_poly_expansion_arr_eval(n, parr,  */
/*                                            x[jj*incx], y + jj*incy); */
/*         if (res != 0){ */
/*             return res; */
/*         } */
/*     } */


/*     for (size_t jj = 0; jj < N; jj++){ */
/*         for (size_t ii = 0; ii < n; ii++){ */
/*             if (isnan(y[ii + jj* incy]) || y[ii+jj * incy] > 1e100){ */
/*                 fprintf(stderr,"Warning, evaluation in legendre_array_eval is nan\n"); */
/*                 fprintf(stderr,"Polynomial %zu, evaluation %zu\n",ii,jj); */
/*                 print_orth_poly_expansion(parr[ii],0,NULL,stderr); */
/*                 exit(1); */
/*             } */
/*             else if (isinf(y[ii + jj * incy])){ */
/*                 fprintf(stderr,"Warning, evaluation in legendre_array_eval inf\n"); */
/*                 exit(1); */
/*             }         */
/*         } */
/*     } */
    
    
/*     return 0; */
/* } */



/* /\********************************************************\//\** */
/* *   Get the kristoffel weight of an orthonormal polynomial expansion */
/* * */
/* *   \param[in] poly - polynomial expansion */
/* *   \param[in] x    - location at which to evaluate */
/* * */
/* *   \return out - weight */
/* *************************************************************\/ */
/* double orth_poly_expansion_get_kristoffel_weight(const struct OrthPolyExpansion * poly, double x) */
/* { */
/*     size_t iter = 0; */
/*     double p [2]; */
/*     double pnew; */
        
/*     double x_normalized = space_mapping_map(poly->space_transform,x); */
/*     double den = 0.0; */
        
/*     p[0] = poly->p->const_term; */


/*     den += p[0]*p[0]; */
        
/*     iter++; */
/*     if (poly->num_poly > 1){ */
/*         p[1] = poly->p->lin_const + poly->p->lin_coeff * x_normalized; */

/*         den += p[1]*p[1]; */
/*         iter++; */
/*     } */
/*     for (iter = 2; iter < poly->num_poly; iter++){ */
/*         pnew = eval_orth_poly_wp(poly->p, p[0], p[1], iter, x_normalized); */

/*         p[0] = p[1]; */
/*         p[1] = pnew; */

/*         den += pnew*pnew; */
/*     } */

/*     // Normalize by number of functions */
/*     return sqrt(den / ( (double) poly->num_poly ) ); */
/*     //return sqrt(den); */
/* } */

/* /\********************************************************\//\** */
/* *   Evaluate a polynomial expansion consisting of sequentially increasing  */
/* *   order polynomials from the same family. */
/* * */
/* *   \param[in]     poly - function */
/* *   \param[in]     N    - number of evaluations */
/* *   \param[in]     x    - location at which to evaluate */
/* *   \param[in]     incx - increment of x */
/* *   \param[in,out] y    - allocated space for evaluations */
/* *   \param[in]     incy - increment of y* */
/* * */
/* *   \note Currently just calls the single evaluation code */
/* *         Note sure if this is optimal, cache-wise */
/* *************************************************************\/ */
/* void orth_poly_expansion_evalN(const struct OrthPolyExpansion * poly, size_t N, */
/*                                const double * x, size_t incx, double * y, size_t incy) */
/* { */
/*     for (size_t ii = 0; ii < N; ii++){ */
/*         y[ii*incy] = orth_poly_expansion_eval(poly,x[ii*incx]); */
/*     } */
/* } */

/* /\********************************************************\//\* */
/* *   Evaluate the gradient of an orthonormal polynomial expansion  */
/* *   with respect to the parameters */
/* * */
/* *   \param[in]     poly - polynomial expansion */
/* *   \param[in]     nx   - number of x points */
/* *   \param[in]     x    - location at which to evaluate */
/* *   \param[in,out] grad - gradient values (N,nx) */
/* * */
/* *   \return 0 success, 1 otherwise */
/* *************************************************************\/ */
/* int orth_poly_expansion_param_grad_eval( */
/*     const struct OrthPolyExpansion * poly, size_t nx, const double * x, double * grad) */
/* { */
/*     size_t nparam = orth_poly_expansion_get_num_params(poly); */
/*     for (size_t ii = 0; ii < nx; ii++){ */

/*         double p[2]; */
/*         double pnew; */

/*         double x_norm = space_mapping_map(poly->space_transform,x[ii]); */
    
/*         size_t iter = 0; */
/*         p[0] = poly->p->const_term; */
/*         double den = p[0]*p[0]; */
        
/*         grad[ii*nparam] = p[0]; */
/*         iter++; */
/*         if (poly->num_poly > 1){ */
/*             p[1] = poly->p->lin_const + poly->p->lin_coeff * x_norm; */
/*             grad[ii*nparam + iter] = p[1];  */
/*             iter++; */
/*             den += p[1]*p[1]; */

/*             for (iter = 2; iter < poly->num_poly; iter++){ */
/*                 pnew = (poly->p->an(iter)*x_norm + poly->p->bn(iter)) * p[1] + poly->p->cn(iter) * p[0]; */
/*                 grad[ii*nparam + iter] = pnew; */
/*                 den += pnew * pnew; */
/*                 p[0] = p[1]; */
/*                 p[1] = pnew; */
/*             } */
/*         } */

/*         if (poly->kristoffel_eval == 1){ */
/*             /\* printf("gradient normalized by kristoffel %G\n",den); *\/ */
/*             for (size_t jj = 0; jj < poly->num_poly; jj++){ */
/*                 // Normalize by number of functions */
/*                 grad[ii*nparam+jj] /= sqrt(den / ( (double) poly->num_poly )); */
/*                 //grad[ii*nparam+jj] /= sqrt(den); */
/*             } */
/*         } */
/*     } */
/*     return 0;     */
/* } */


/* /\********************************************************\//\* */
/* *   Evaluate the gradient of an orthonormal polynomial expansion  */
/* *   with respect to the parameters */
/* * */
/* *   \param[in]     poly - polynomial expansion */
/* *   \param[in]     x    - location at which to evaluate */
/* *   \param[in,out] grad - gradient values (N,nx) */
/* * */
/* *   \return evaluation */
/* *************************************************************\/ */
/* double orth_poly_expansion_param_grad_eval2( */
/*     const struct OrthPolyExpansion * poly, double x, double * grad) */
/* { */
/*     double out = 0.0; */

/*     double p[2]; */
/*     double pnew; */

/*     double x_norm = space_mapping_map(poly->space_transform,x); */

/*     double den = 0.0; */
/*     size_t iter = 0; */
/*     p[0] = poly->p->const_term; */
/*     grad[0] = p[0]; */
/*     den += p[0]*p[0]; */
    
/*     out += p[0]*poly->coeff[0]; */
/*     iter++; */
/*     if (poly->num_poly > 1){ */
/*         p[1] = poly->p->lin_const + poly->p->lin_coeff * x_norm; */
/*         grad[iter] = p[1]; */
/*         den += p[1]*p[1]; */
/*         out += p[1]*poly->coeff[1]; */
/*         iter++; */

/*         for (iter = 2; iter < poly->num_poly; iter++){ */
/*             pnew = (poly->p->an(iter)*x_norm + poly->p->bn(iter)) * p[1] + poly->p->cn(iter) * p[0]; */
/*             grad[iter] = pnew; */
/*             out += pnew*poly->coeff[iter]; */
/*             p[0] = p[1]; */
/*             p[1] = pnew; */

/*             den += pnew * pnew; */
/*         } */
/*     } */
/*     if (poly->kristoffel_eval == 1){ */
/*         /\* printf("gradient normalized by kristoffel %G\n",den); *\/ */
/*         for (size_t jj = 0; jj < poly->num_poly; jj++){ */
/*             // Normalize by number of functions */
/*             grad[jj] /= sqrt(den / ( (double) poly->num_poly )); */
/*             /\* grad[jj] /= sqrt(den); *\/ */
/*         } */
/*     } */
/*     return out;     */
/* } */

/* /\********************************************************\//\** */
/*     Take a gradient of the squared norm  */
/*     with respect to its parameters, and add a scaled version */
/*     of this gradient to *grad* */

/*     \param[in]     poly  - polynomial */
/*     \param[in]     scale - scaling for additional gradient */
/*     \param[in,out] grad  - gradient, on output adds scale * new_grad */

/*     \return  0 - success, 1 -failure */

/* ************************************************************\/ */
/* int */
/* orth_poly_expansion_squared_norm_param_grad(const struct OrthPolyExpansion * poly, */
/*                                             double scale, double * grad) */
/* { */

/*     assert (poly->kristoffel_eval == 0); */
/*     int res = 1; */

    
/*     // assuming linear transformation */
/*     double dtransform_dx = space_mapping_map_deriv(poly->space_transform,0.0); */
/*     if (poly->p->ptype == LEGENDRE){ */
/*         for (size_t ii = 0; ii < poly->num_poly; ii++){ */
/*             //the extra 2 is for the weight */
/*             grad[ii] += 2.0*scale * poly->coeff[ii] * poly->p->norm(ii) * /\* *dtransform_dx * *\/ */
/*                          (poly->upper_bound-poly->lower_bound);  */
/*         } */
/*         res = 0; */
/*     } */
/*     else if (poly->p->ptype == HERMITE){ */
/*         for (size_t ii = 0; ii < poly->num_poly; ii++){ */
/*             grad[ii] += 2.0*scale * poly->coeff[ii] * poly->p->norm(ii) * dtransform_dx; */
/*         } */
/*         res = 0; */
/*     } */
/*     else if (poly->p->ptype == CHEBYSHEV){ */
/*         double * temp = calloc_double(poly->num_poly); */
/*         for (size_t ii = 0; ii < poly->num_poly; ii++){ */
/*             temp[ii] = 2.0/(1.0 - (double)2*ii*2*ii); */
/*         } */
/*         for (size_t ii = 0; ii < poly->num_poly; ii++){ */
/*             for (size_t jj = 0; jj < poly->num_poly; jj++){ */
/*                 size_t n1 = ii+jj; */
/*                 size_t n2; */
/*                 if (ii > jj){ */
/*                     n2 = ii-jj; */
/*                 } */
/*                 else{ */
/*                     n2 = jj-ii; */
/*                 } */
/*                 if (n1 % 2 == 0){ */
/*                     grad[ii] += 2.0*scale*temp[n1/2] * dtransform_dx; */
/*                 } */
/*                 if (n2 % 2 == 0){ */
/*                     grad[ii] += 2.0*scale*temp[n2/2] * dtransform_dx; */
/*                 } */
/*             } */
/*         } */
/*         free(temp); temp = NULL; */
/*         res = 0; */
/*     } */
/*     else{ */
/*         fprintf(stderr, */
/*                 "Cannot evaluate derivative with respect to parameters for polynomial type %d\n", */
/*                 poly->p->ptype); */
/*         exit(1); */
/*     } */
/*     return res; */
/* } */

/* /\********************************************************\//\** */
/*     Squared norm of a function in RKHS  */

/*     \param[in]     poly        - polynomial */
/*     \param[in]     decay_type  - type of decay */
/*     \param[in]     decay_param - parameter of decay */

/*     \return  0 - success, 1 -failure */
/* ************************************************************\/ */
/* double */
/* orth_poly_expansion_rkhs_squared_norm(const struct OrthPolyExpansion * poly, */
/*                                       enum coeff_decay_type decay_type, */
/*                                       double decay_param) */
/* { */

/*     assert (poly->kristoffel_eval == 0); */
    
/*     // assuming linear transformation */
/*     double m = space_mapping_map_deriv(poly->space_transform,0.0); */
/*     /\* double m = (poly->upper_bound-poly->lower_bound) /(poly->p->upper - poly->p->lower); *\/ */
/*     double out = 0.0; */
/*     if (poly->p->ptype == LEGENDRE){ */
/*         if (decay_type == ALGEBRAIC){ */
/*             for (size_t ii = 0; ii < poly->num_poly; ii++){ */
/*                 out += poly->coeff[ii] * poly->coeff[ii]*pow(decay_param,ii) * poly->p->norm(ii)*2.0 * m; */
/*             }    */
/*         } */
/*         else if (decay_type == EXPONENTIAL){ */
/*             for (size_t ii = 0; ii < poly->num_poly; ii++){ */
/*                 out += poly->coeff[ii] * poly->coeff[ii]*pow((double)ii+1.0,-decay_param)*m*poly->p->norm(ii)*2.0; */
/*             }    */
/*         } */
/*         else{ */
/*             for (size_t ii = 0; ii < poly->num_poly; ii++){ */
/*                 out += poly->coeff[ii] * poly->coeff[ii] * poly->p->norm(ii)*2.0*m; */
/*             } */
/*         } */
/*     } */
/*     else if (poly->p->ptype == HERMITE){ */
/*         if (decay_type == ALGEBRAIC){ */
/*             for (size_t ii = 0; ii < poly->num_poly; ii++){ */
/*                 out += poly->coeff[ii] * poly->coeff[ii]*pow(decay_param,ii) * poly->p->norm(ii); */
/*             }    */
/*         } */
/*         else if (decay_type == EXPONENTIAL){ */
/*             for (size_t ii = 0; ii < poly->num_poly; ii++){ */
/*                 out += poly->coeff[ii] * poly->coeff[ii]*pow((double)ii+1.0,-decay_param) * poly->p->norm(ii); */
/*             }    */
/*         } */
/*         else{ */
/*             for (size_t ii = 0; ii < poly->num_poly; ii++){ */
/*                 out += poly->coeff[ii] * poly->coeff[ii] * poly->p->norm(ii); */
/*             } */
/*         } */
/*     } */
/*     else if (poly->p->ptype == CHEBYSHEV){ */
/*         double * temp = calloc_double(poly->num_poly); */
/*         for (size_t ii = 0; ii < poly->num_poly; ii++){ */
/*             temp[ii] = 2.0/(1.0 - (double)2*ii*2*ii); */
/*         } */
/*         for (size_t ii = 0; ii < poly->num_poly; ii++){ */
/*             double temp_sum = 0.0; */
/*             for (size_t jj = 0; jj < poly->num_poly; jj++){ */
/*                 size_t n1 = ii+jj; */
/*                 size_t n2; */
/*                 if (ii > jj){ */
/*                     n2 = ii-jj; */
/*                 } */
/*                 else{ */
/*                     n2 = jj-ii; */
/*                 } */
/*                 if (n1 % 2 == 0){ */
/*                     temp_sum += poly->coeff[jj]*temp[n1/2]; */
/*                 } */
/*                 if (n2 % 2 == 0){ */
/*                     temp_sum += poly->coeff[jj]*temp[n2/2]; */
/*                 } */
/*             } */
/*             if (decay_type == ALGEBRAIC){ */
/*                 out += temp_sum*temp_sum * pow(decay_param,ii)*m; */
/*             } */
/*             else if (decay_type == EXPONENTIAL){ */
/*                 out += temp_sum*temp_sum * pow((double)ii+1.0,-decay_param)*m; */
/*             } */
/*             else{ */
/*                 out += temp_sum * temp_sum*m; */
/*             } */
/*         } */
/*         free(temp); temp = NULL; */
/*     } */
/*     else{ */
/*         fprintf(stderr, "Cannot evaluate derivative with respect to parameters for polynomial type %d\n",poly->p->ptype); */
/*         exit(1); */
/*     } */
/*     return out; */
/* } */

/* /\********************************************************\//\** */
/*     Take a gradient of the squared norm  */
/*     with respect to its parameters, and add a scaled version */
/*     of this gradient to *grad* */

/*     \param[in]     poly        - polynomial */
/*     \param[in]     scale       - scaling for additional gradient */
/*     \param[in]     decay_type  - type of decay */
/*     \param[in]     decay_param - parameter of decay */
/*     \param[in,out] grad        - gradient, on output adds scale * new_grad */

/*     \return  0 - success, 1 -failure */

/*     \note  */
/*     NEED TO DO SOME TESTS FOR CHEBYSHEV (dont use for now) */
/* ************************************************************\/ */
/* int */
/* orth_poly_expansion_rkhs_squared_norm_param_grad(const struct OrthPolyExpansion * poly, */
/*                                                  double scale, enum coeff_decay_type decay_type, */
/*                                                  double decay_param, double * grad) */
/* { */
/*     assert (poly->kristoffel_eval == 0); */
/*     int res = 1; */
/*     if ((poly->p->ptype == LEGENDRE) || (poly->p->ptype == HERMITE)){ */
/*         if (decay_type == ALGEBRAIC){ */
/*             for (size_t ii = 0; ii < poly->num_poly; ii++){ */
/*                 grad[ii] += 2.0*scale * poly->coeff[ii] * pow(decay_param,ii); */
/*             }    */
/*         } */
/*         else if (decay_type == EXPONENTIAL){ */
/*             for (size_t ii = 0; ii < poly->num_poly; ii++){ */
/*                 grad[ii] += 2.0*scale * poly->coeff[ii] * pow((double)ii+1.0,-decay_param); */
/*             }    */
/*         } */
/*         else{ */
/*             for (size_t ii = 0; ii < poly->num_poly; ii++){ */
/*                 grad[ii] += 2.0*scale * poly->coeff[ii]; */
/*             } */
/*         } */
/*         res = 0; */
/*     } */
/*     else if (poly->p->ptype == CHEBYSHEV){ */
/*         // THIS COULD BE WRONG!! */
/*         double * temp = calloc_double(poly->num_poly); */
/*         for (size_t ii = 0; ii < poly->num_poly; ii++){ */
/*             temp[ii] = 2.0/(1.0 - (double)2*ii*2*ii); */
/*         } */
/*         for (size_t ii = 0; ii < poly->num_poly; ii++){ */
/*             for (size_t jj = 0; jj < poly->num_poly; jj++){ */
/*                 size_t n1 = ii+jj; */
/*                 size_t n2; */
/*                 if (ii > jj){ */
/*                     n2 = ii-jj; */
/*                 } */
/*                 else{ */
/*                     n2 = jj-ii; */
/*                 } */
/*                 if (decay_type == ALGEBRAIC){ */
/*                     if (n1 % 2 == 0){ */
/*                         grad[ii] += 2.0*scale*temp[n1/2]* pow(decay_param,ii); */
/*                     } */
/*                     if (n2 % 2 == 0){ */
/*                         grad[ii] += 2.0*scale*temp[n2/2]* pow(decay_param,ii); */
/*                     } */
/*                 } */
/*                 else if (decay_type == EXPONENTIAL){ */
/*                     if (n1 % 2 == 0){ */
/*                         grad[ii] += 2.0*scale*temp[n1/2]*pow((double)ii+1.0,-decay_param); */
/*                     } */
/*                     if (n2 % 2 == 0){ */
/*                         grad[ii] += 2.0*scale*temp[n2/2]*pow((double)ii+1.0,-decay_param); */
/*                     } */
/*                 } */
/*                 else { */
/*                     if (n1 % 2 == 0){ */
/*                         grad[ii] += 2.0*scale*temp[n1/2]; */
/*                     } */
/*                     if (n2 % 2 == 0){ */
/*                         grad[ii] += 2.0*scale*temp[n2/2]; */
/*                     } */
/*                 } */
/*             } */
/*         } */
/*         free(temp); temp = NULL; */
/*         res = 0; */
/*     } */
/*     else{ */
/*         fprintf(stderr, "Cannot evaluate derivative with respect to parameters for polynomial type %d\n",poly->p->ptype); */
/*         exit(1); */
/*     } */
/*     return res; */
/* } */

/* /\********************************************************\//\** */
/* *  Round an orthogonal polynomial expansion */
/* * */
/* *  \param[in,out] p - orthogonal polynomial expansion */
/* * */
/* *  \note */
/* *      (UNTESTED, use with care!!!!  */
/* *************************************************************\/ */
/* void orth_poly_expansion_round(struct OrthPolyExpansion ** p) */
/* {    */
/*     if (0 == 0){ */
/*         /\* double thresh = 1e-3*ZEROTHRESH; *\/ */
/*         double thresh = ZEROTHRESH; */
/*         /\* double thresh = 1e-30; *\/ */
/*         /\* double thresh = 10.0*DBL_EPSILON; *\/ */
/*         /\* printf("thresh = %G\n",thresh); *\/ */
/*         size_t jj = 0; */
/*         // */
/*         int allzero = 1; */
/*         double maxcoeff = fabs((*p)->coeff[0]); */
/*         for (size_t ii = 1; ii < (*p)->num_poly; ii++){ */
/*             double val = fabs((*p)->coeff[ii]); */
/*             if (val > maxcoeff){ */
/*                 maxcoeff = val; */
/*             } */
/*         } */
/*         maxcoeff = maxcoeff * (*p)->num_poly; */
/*         /\* printf("maxcoeff = %3.15G\n", maxcoeff); *\/ */
/* 	    for (jj = 0; jj < (*p)->num_poly;jj++){ */
/*             if (fabs((*p)->coeff[jj]) < thresh){ */
/*                 (*p)->coeff[jj] = 0.0; */
/*             } */
/*             if (fabs((*p)->coeff[jj])/maxcoeff < thresh){ */
/*                 (*p)->coeff[jj] = 0.0; */
/*             } */
/*             else{ */
/*                 allzero = 0; */
/*             } */
           
/* 	    } */
/*         if (allzero == 1){ */
/*             (*p)->num_poly = 1; */
/*         } */
/*         else { */
/*             jj = 0; */
/*             size_t end = (*p)->num_poly; */
/*             if ((*p)->num_poly > 2){ */
/*                 while (fabs((*p)->coeff[end-1]) < thresh){ */
/*                     end-=1; */
/*                     if (end == 0){ */
/*                         break; */
/*                     } */
/*                 } */
                
/*                 if (end > 0){ */
/*                     //printf("SHOULD NOT BE HERE\n"); */
/*                     size_t num_poly = end; */
/*                     // */
/*                     //double * new_coeff = calloc_double(num_poly); */
/*                     //for (jj = 0; jj < num_poly; jj++){ */
/*                     //    new_coeff[jj] = (*p)->coeff[jj]; */
/*                    // } */
/*                     //free((*p)->coeff); (*p)->coeff=NULL; */
/*                     //(*p)->coeff = new_coeff; */
/*                     (*p)->num_poly = num_poly; */
/*                 } */
/*             } */
/*         } */

/*         /\* printf("rounded coeffs = "); dprint((*p)->num_poly, (*p)->coeff); *\/ */

/*         /\* orth_poly_expansion_roundt(p,thresh); *\/ */

/*     } */
/* } */

/* /\********************************************************\//\** */
/* *  Round an orthogonal polynomial expansion to a threshold */
/* * */
/* *  \param[in,out] p      - orthogonal polynomial expansion */
/* *  \param[in]     thresh - threshold (relative) to round to */
/* * */
/* *  \note */
/* *      (UNTESTED, use with care!!!!  */
/* *************************************************************\/ */
/* void orth_poly_expansion_roundt(struct OrthPolyExpansion ** p, double thresh) */
/* {    */
    
/*     size_t jj = 0; */
/*     double sum = 0.0; */
/*     /\* double maxval = fabs((*p)->coeff[0]); *\/ */
/* 	/\* for (jj = 1; jj < (*p)->num_poly;jj++){ *\/ */
/*     /\*     sum += pow((*p)->coeff[jj],2); *\/ */
/*     /\*     if (fabs((*p)->coeff[jj]) > maxval){ *\/ */
/*     /\*         maxval = fabs((*p)->coeff[jj]); *\/ */
/*     /\*     } *\/ */
/* 	/\* } *\/ */
/*     size_t keep = (*p)->num_poly; */
/*     if (sum <= thresh){ */
/*         keep = 1; */
/*     } */
/*     else{ */
/*         double sumrun = 0.0; */
/*         for (jj = 0; jj < (*p)->num_poly; jj++){ */
/*             /\* if ((fabs((*p)->coeff[jj]) / maxval) < thresh){ *\/ */
/*             /\*     (*p)->coeff[jj] = 0.0; *\/ */
/*             /\* } *\/ */
/*             sumrun += pow((*p)->coeff[jj],2); */
/*             if ( (sumrun / sum) > (1.0-thresh)){ */
/*                 keep = jj+1; */
/*                 break; */
/*             } */
/*         } */
/*     } */
/*     /\* dprint((*p)->num_poly, (*p)->coeff); *\/ */
/*     /\* printf("number keep = %zu\n",keep); *\/ */
/*     //printf("tolerance = %G\n",thresh); */
/*     double * new_coeff = calloc_double(keep + OPECALLOC); */
/*     memmove(new_coeff,(*p)->coeff, keep * sizeof(double)); */
/*     free((*p)->coeff); */
/*     (*p)->num_poly = keep; */
/*     (*p)->nalloc = (*p)->num_poly + OPECALLOC; */
/*     (*p)->coeff = new_coeff; */
/* } */



/* /\********************************************************\//\** */
/* *  Approximate a function with an orthogonal polynomial */
/* *  series with a fixed number of basis */
/* * */
/* *  \param[in] A    - function to approximate */
/* *  \param[in] args - arguments to function */
/* *  \param[in] poly - orthogonal polynomial expansion */
/* * */
/* *  \note */
/* *       Wont work for polynomial expansion with only the constant  */
/* *       term. */
/* *************************************************************\/ */
/* void */
/* orth_poly_expansion_approx(double (*A)(double,void *), void *args,  */
/*                            struct OrthPolyExpansion * poly) */
/* { */
/*     size_t ii, jj; */
/*     double p[2]; */
/*     double pnew; */

/*     /\* double m = 1.0; *\/ */
/*     /\* double off = 0.0; *\/ */

/*     double * fvals = NULL; */
/*     double * pt_un = NULL; // unormalized point */
/*     double * pt = NULL; */
/*     double * wt = NULL;  */

/*     size_t nquad = poly->num_poly+1; */

/*     switch (poly->p->ptype) { */
/*         case CHEBYSHEV: */
/*             /\* m = (poly->upper_bound - poly->lower_bound) /  *\/ */
/*             /\*     (poly->p->upper - poly->p->lower); *\/ */
/*             /\* off = poly->upper_bound - m * poly->p->upper; *\/ */
/*             pt = calloc_double(nquad); */
/*             wt = calloc_double(nquad); */
/*             cheb_gauss(poly->num_poly,pt,wt); */

/*             /\* clenshaw_curtis(nquad,pt,wt); *\/ */
/*             /\* for (ii = 0; ii < nquad; ii++){wt[ii] *= 0.5;} *\/ */
            
/*             break; */
/*         case LEGENDRE: */
/*             /\* m = (poly->upper_bound - poly->lower_bound) /  *\/ */
/*             /\*     (poly->p->upper - poly->p->lower); *\/ */
/*             /\* off = poly->upper_bound - m * poly->p->upper; *\/ */
/* //            nquad = poly->num_poly*2.0-1.0;//\*2.0; */
/*             pt = calloc_double(nquad); */
/*             wt = calloc_double(nquad); */
            
/*             // uncomment next two for cc */
/*             // clenshaw_curtis(nquad,pt,wt); */
/* //            for (ii = 0; ii < nquad; ii++){wt[ii] *= 0.5;} */

/*             gauss_legendre(nquad,pt,wt); */
/*             break; */
/*         case HERMITE: */
/*             pt = calloc_double(nquad); */
/*             wt = calloc_double(nquad); */
/*             gauss_hermite(nquad,pt,wt); */
/* //            printf("point = "); */
/* //            dprint(nquad,pt); */
/*             break; */
/*         case STANDARD: */
/*             fprintf(stderr, "Cannot call orth_poly_expansion_approx for STANDARD type\n"); */
/*             break; */
/*         //default: */
/*         //    fprintf(stderr, "Polynomial type does not exist: %d\n ",  */
/*         //            poly->p->ptype); */
/*     } */
    
/*     fvals = calloc_double(nquad); */
/*     pt_un = calloc_double(nquad); */
/*     for (ii = 0; ii < nquad; ii++){ */
/*         /\* pt_un[ii] =  m * pt[ii] + off; *\/ */
/*         pt_un[ii] = space_mapping_map_inverse(poly->space_transform,pt[ii]); */
/*         fvals[ii] = A(pt_un[ii],args)  * wt[ii]; */
/*     } */
    
/*     if (poly->num_poly > 1){ */
/*         for (ii = 0; ii < nquad; ii++){ // loop over all points */
/*             p[0] = poly->p->const_term; */
/*             poly->coeff[0] += fvals[ii] * poly->p->const_term; */
            
/*             p[1] = poly->p->lin_const + poly->p->lin_coeff * pt[ii]; */
/*             poly->coeff[1] += fvals[ii] * p[1] ; */
/*             // loop over all coefficients */
/*             for (jj = 2; jj < poly->num_poly; jj++){  */
/*                 pnew = eval_orth_poly_wp(poly->p, p[0], p[1], jj, pt[ii]); */
/*                 poly->coeff[jj] += fvals[ii] * pnew; */
/*                 p[0] = p[1]; */
/*                 p[1] = pnew; */
/*             } */
/*         } */

/*         for (ii = 0; ii < poly->num_poly; ii++){ */
/*             poly->coeff[ii] /= poly->p->norm(ii); */
/*         } */

/*     } */
/*     else{ */
/*         for (ii = 0; ii < nquad; ii++){ */

/*             poly->coeff[0] += fvals[ii] *poly->p->const_term; */
/*         } */
/*         poly->coeff[0] /= poly->p->norm(0); */
/*     } */
/*     free(fvals); fvals = NULL; */
/*     free(wt);    wt    = NULL; */
/*     free(pt);    pt    = NULL; */
/*     free(pt_un); pt_un = NULL; */
    
/* } */

/* /\********************************************************\//\** */
/* *  Construct an orthonormal polynomial expansion from (weighted) function  */
/* *  evaluations and quadrature nodes */
/* *   */
/* *  \param[in,out] poly      - orthogonal polynomial expansion */
/* *  \param[in]     num_nodes - number of nodes  */
/* *  \param[in]     fvals     - function values (multiplied by a weight if necessary) */
/* *  \param[in]     nodes     - locations of evaluations */
/* *************************************************************\/ */
/* static void */
/* orth_poly_expansion_construct(struct OrthPolyExpansion * poly, */
/*                               size_t num_nodes, double * fvals, */
/*                               double * nodes) */

/* { */
/*     double p[2]; */
/*     double pnew; */
/*     size_t ii,jj; */
/*     if (poly->num_poly > 1){ */
/*         for (ii = 0; ii < num_nodes; ii++){ // loop over all points */
/*             p[0] = poly->p->const_term; */
/*             poly->coeff[0] += fvals[ii] * poly->p->const_term; */
/*             p[1] = poly->p->lin_const + poly->p->lin_coeff * nodes[ii]; */
/*             poly->coeff[1] += fvals[ii] * p[1] ; */
/*             // loop over all coefficients */
/*             for (jj = 2; jj < poly->num_poly; jj++){ */
/*                 pnew = eval_orth_poly_wp(poly->p, p[0], p[1], jj, nodes[ii]); */
/*                 poly->coeff[jj] += fvals[ii] * pnew; */
/*                 p[0] = p[1]; */
/*                 p[1] = pnew; */
/*             } */
/*         } */

/*         for (ii = 0; ii < poly->num_poly; ii++){ */
/*             poly->coeff[ii] /= poly->p->norm(ii); */
/*         } */

/*     } */
/*     else{ */
/*         for (ii = 0; ii < num_nodes; ii++){ */
/*             poly->coeff[0] += fvals[ii] * poly->p->const_term; */
/*         } */
/*         poly->coeff[0] /= poly->p->norm(0); */
/*     } */
/* } */



/* /\********************************************************\//\** */
/* *   Create an approximation adaptively */
/* * */
/* *   \param[in] aopts - approximation options */
/* *   \param[in] fw    - wrapped function */
/* *    */
/* *   \return poly */
/* * */
/* *   \note  */
/* *       Follows general scheme that trefethan outlines about  */
/* *       Chebfun in his book Approximation Theory and practice */
/* *************************************************************\/ */
/* struct OrthPolyExpansion * */
/* orth_poly_expansion_approx_adapt(const struct OpeOpts * aopts, */
/*                                  struct Fwrap * fw) */
/* { */
/*     assert (aopts != NULL); */
/*     assert (fw != NULL); */

/*     size_t ii; */
/*     size_t N = aopts->start_num; */
/*     struct OrthPolyExpansion * poly = NULL; */
/*     poly = orth_poly_expansion_init_from_opts(aopts,N); */
/*     orth_poly_expansion_approx_vec(poly,fw,aopts); */

/*     size_t coeffs_too_big = 0; */
/*     for (ii = 0; ii < aopts->coeffs_check; ii++){ */
/*         if (fabs(poly->coeff[N-1-ii]) > aopts->tol){ */
/*             coeffs_too_big = 1; */
/*             break; */
/*         } */
/*     } */
    


/*     size_t maxnum = ope_opts_get_maxnum(aopts); */
/*     /\* printf("TOL SPECIFIED IS %G\n",aopts->tol); *\/ */
/*     /\* printf("Ncoeffs check=%zu \n",aopts->coeffs_check); *\/ */
/*     /\* printf("maxnum = %zu\n", maxnum); *\/ */
/*     while ((coeffs_too_big == 1) && (N < maxnum)){ */
/*         /\* printf("N = %zu\n",N); *\/ */
/*         coeffs_too_big = 0; */
	
/*         free(poly->coeff); poly->coeff = NULL; */
/*         if (aopts->qrule == C3_CC_QUAD){ */
/*             N = N * 2 - 1; // for nested cc */
/*         } */
/*         else{ */
/*             N = N + 7; */
/*         } */
/*         poly->num_poly = N; */
/*         poly->nalloc = N + OPECALLOC; */
/*         poly->coeff = calloc_double(poly->nalloc); */
/* //        printf("Number of coefficients to check = %zu\n",aopts->coeffs_check); */
/*         orth_poly_expansion_approx_vec(poly,fw,aopts); */
/* 	    double sum_coeff_squared = 0.0; */
/*         for (ii = 0; ii < N; ii++){  */
/*             sum_coeff_squared += pow(poly->coeff[ii],2);  */
/*         } */
/*         sum_coeff_squared = fmax(sum_coeff_squared,ZEROTHRESH); */
/*         /\* sum_coeff_squared = 1.0; *\/ */
/*         for (ii = 0; ii < aopts->coeffs_check; ii++){ */
/*             /\* printf("aopts->tol=%3.15G last coefficients %3.15G\n", *\/ */
/*             /\*        aopts->tol * sum_coeff_squared, *\/ */
/*            	/\* 	  poly->coeff[N-1-ii]); *\/ */
/*             if (fabs(poly->coeff[N-1-ii]) > (aopts->tol * sum_coeff_squared) ){ */
/*                 coeffs_too_big = 1; */
/*                 break; */
/*             } */
/*         } */
/*         if (N > 100){ */
/*             //printf("Warning: num of poly is %zu: last coeff = %G \n",N,poly->coeff[N-1]); */
/*             //printf("tolerance is %3.15G\n", aopts->tol * sum_coeff_squared); */
/*             //printf("Considering using piecewise polynomials\n"); */
/*             /\* */
/*             printf("first 5 coeffs\n"); */

/*             size_t ll; */
/*             for (ll = 0; ll<5;ll++){ */
/*                 printf("%3.10G ",poly->coeff[ll]); */
/*             } */
/*             printf("\n"); */

/*             printf("Last 10 coeffs\n"); */
/*             for (ll = 0; ll<10;ll++){ */
/*                 printf("%3.10G ",poly->coeff[N-10+ll]); */
/*             } */
/*             printf("\n"); */
/*             *\/ */
/*             coeffs_too_big = 0; */
/*         } */

/*     } */
    
/*     orth_poly_expansion_round(&poly); */

/*     // verify */
/*     /\* double pt = (upper - lower)*randu() + lower; *\/ */
/*     /\* double val_true = A(pt,args); *\/ */
/*     /\* double val_test = orth_poly_expansion_eval(poly,pt); *\/ */
/*     /\* double diff = val_true-val_test; *\/ */
/*     /\* double err = fabs(diff); *\/ */
/*     /\* if (fabs(val_true) > 1.0){ *\/ */
/*     /\* //if (fabs(val_true) > ZEROTHRESH){ *\/ */
/*     /\*     err /= fabs(val_true); *\/ */
/*     /\* } *\/ */
/*     /\* if (err > 100.0*aopts->tol){ *\/ */
/*     /\*     //fprintf(stderr, "Approximating at point %G in (%3.15G,%3.15G)\n",pt,lower,upper); *\/ */
/*     /\*     //fprintf(stderr, "leads to error %G, while tol is %G \n",err,aopts->tol); *\/ */
/*     /\*     //fprintf(stderr, "actual value is %G \n",val_true); *\/ */
/*     /\*     //fprintf(stderr, "predicted value is %3.15G \n",val_test); *\/ */
/*     /\*     //fprintf(stderr, "%zu N coeffs, last coeffs are %3.15G,%3.15G \n",N,poly->coeff[N-2],poly->coeff[N-1]); *\/ */
/*     /\*     //exit(1); *\/ */
/*     /\* } *\/ */

/*     /\* if (default_opts == 1){ *\/ */
/*     /\*     ope_opts_free(aopts); *\/ */
/*     /\* } *\/ */
/*     return poly; */
/* } */

/* /\********************************************************\//\** */
/* *   Generate an orthonormal polynomial with pseudorandom coefficients */
/* *   between [-1,1] */
/* * */
/* *   \param[in] ptype    - polynomial type */
/* *   \param[in] maxorder - maximum order of the polynomial */
/* *   \param[in] lower    - lower bound of input */
/* *   \param[in] upper    - upper bound of input */
/* * */
/* *   \return poly */
/* *************************************************************\/ */
/* struct OrthPolyExpansion *  */
/* orth_poly_expansion_randu(enum poly_type ptype, size_t maxorder,  */
/*                           double lower, double upper) */
/* { */
/*     struct OrthPolyExpansion * poly = */
/*         orth_poly_expansion_init(ptype,maxorder+1, lower, upper); */
                                        
/*     size_t ii; */
/*     for (ii = 0; ii < poly->num_poly; ii++){ */
/*         poly->coeff[ii] = randu()*2.0-1.0; */
/*     } */
/*     return poly; */
/* } */

/* /\********************************************************\//\** */
/* *   Integrate a Chebyshev approximation */
/* * */
/* *   \param[in] poly - polynomial to integrate */
/* * */
/* *   \return out - integral of approximation */
/* *************************************************************\/ */
/* double */
/* cheb_integrate2(const struct OrthPolyExpansion * poly) */
/* { */
/*     size_t ii; */
/*     double out = 0.0; */
    
/*     double m = space_mapping_map_inverse_deriv(poly->space_transform,0.0); */
/*     /\* double m =  *\/ */
/*     for (ii = 0; ii < poly->num_poly; ii+=2){ */
/*         out += poly->coeff[ii] * 2.0 / (1.0 - (double) (ii*ii)); */
/*     } */
/*     out = out * m; */
/*     return out; */
/* } */

/* /\********************************************************\//\** */
/* *   Integrate a Legendre approximation */
/* * */
/* *   \param[in] poly - polynomial to integrate */
/* * */
/* *   \return out - integral of approximation */
/* *************************************************************\/ */
/* double */
/* legendre_integrate(const struct OrthPolyExpansion * poly) */
/* { */
/*     double out = 0.0; */

/*     double m = space_mapping_map_inverse_deriv(poly->space_transform,0.0); */
/*     out = poly->coeff[0] * 2.0; */
/*     out = out * m; */
/*     return out; */
/* } */

/* /\********************************************************\//\** */
/* *   Compute the product of two polynomial expansion */
/* * */
/* *   \param[in] a - first polynomial */
/* *   \param[in] b - second polynomial */
/* * */
/* *   \return c - polynomial expansion */
/* * */
/* *   \note  */
/* *   Computes c(x) = a(x)b(x) where c is same form as a */
/* *   Lower and upper bounds of both polynomials must be the same */
/* *************************************************************\/ */
/* struct OrthPolyExpansion * */
/* orth_poly_expansion_prod(const struct OrthPolyExpansion * a, */
/*                          const struct OrthPolyExpansion * b) */
/* { */
    
/*     struct OrthPolyExpansion * c = NULL; */
/*     double lb = a->lower_bound; */
/*     double ub = a->upper_bound; */

/*     enum poly_type p = a->p->ptype; */
/*     if ( (p == LEGENDRE) && (a->num_poly < 100) && (b->num_poly < 100)){ */
/*         //printf("in special prod\n"); */
/*         //double lb = a->lower_bound; */
/*         //double ub = b->upper_bound; */
            
/*         size_t ii,jj; */
/*         c = orth_poly_expansion_init(p, a->num_poly + b->num_poly+1, lb, ub); */
/*         double * allprods = calloc_double(a->num_poly * b->num_poly); */
/*         for (ii = 0; ii < a->num_poly; ii++){ */
/*             for (jj = 0; jj < b->num_poly; jj++){ */
/*                 allprods[jj + ii * b->num_poly] = a->coeff[ii] * b->coeff[jj]; */
/*             } */
/*         } */
        
/*         //printf("A = \n"); */
/*         //print_orth_poly_expansion(a,1,NULL); */

/*         //printf("B = \n"); */
/*         //print_orth_poly_expansion(b,1,NULL); */

/*         //dprint2d_col(b->num_poly, a->num_poly, allprods); */

/*         size_t kk; */
/*         for (kk = 0; kk < c->num_poly; kk++){ */
/*             for (ii = 0; ii < a->num_poly; ii++){ */
/*                 for (jj = 0; jj < b->num_poly; jj++){ */
/*                     c->coeff[kk] +=  lpolycoeffs[ii+jj*100+kk*10000] *  */
/*                                         allprods[jj+ii*b->num_poly]; */
/*                 } */
/*             } */
/*             //printf("c coeff[%zu]=%G\n",kk,c->coeff[kk]); */
/*         } */
/*         orth_poly_expansion_round(&c); */
/*         free(allprods); allprods=NULL; */
/*     } */
/*     /\* else if (p == CHEBYSHEV){ *\/ */
/*     /\*     c = orth_poly_expansion_init(p,a->num_poly+b->num_poly+1,lb,ub); *\/ */
/*     /\*     for (size_t ii = 0; ii) *\/ */
/*     /\* } *\/ */
/*     else{ */
/* //        printf("OrthPolyExpansion product greater than order 100 is slow\n"); */
/*         const struct OrthPolyExpansion * comb[2]; */
/*         comb[0] = a; */
/*         comb[1] = b; */
        
/*         double norma = 0.0, normb = 0.0; */
/*         size_t ii; */
/*         for (ii = 0; ii < a->num_poly; ii++){ */
/*             norma += pow(a->coeff[ii],2); */
/*         } */
/*         for (ii = 0; ii < b->num_poly; ii++){ */
/*             normb += pow(b->coeff[ii],2); */
/*         } */
        
/*         if ( (norma < ZEROTHRESH) || (normb < ZEROTHRESH) ){ */
/*             //printf("in here \n"); */
/* //            c = orth_poly_expansion_constant(0.0,a->p->ptype,lb,ub); */
/*             c = orth_poly_expansion_init(p,1, lb, ub); */
/*             space_mapping_free(c->space_transform); c->space_transform = NULL; */
/*             c->space_transform = space_mapping_copy(a->space_transform); */
/*             c->coeff[0] = 0.0; */
/*         } */
/*         else{ */
/*             //printf(" total order of product = %zu\n",a->num_poly+b->num_poly); */
/*             c = orth_poly_expansion_init(p, a->num_poly + b->num_poly+5, lb, ub); */
/*             space_mapping_free(c->space_transform); c->space_transform = NULL; */
/*             c->space_transform = space_mapping_copy(a->space_transform); */
/*             orth_poly_expansion_approx(&orth_poly_expansion_eval3,comb,c); */
/*             /\* printf("num_coeff pre_round = %zu\n", c->num_poly); *\/ */
/*             /\* orth_poly_expansion_approx(&orth_poly_expansion_eval3,comb,c); *\/ */
/*             orth_poly_expansion_round(&c); */
/*             /\* printf("num_coeff post_round = %zu\n", c->num_poly); *\/ */
/*         } */
/*     } */
    
/*     //\* */
/*     //printf("compute product\n"); */
/*     //struct OpeOpts ao; */
/*     //ao.start_num = 3; */
/*     //ao.coeffs_check = 2; */
/*     //ao.tol = 1e-13; */
/*     //c = orth_poly_expansion_approx_adapt(&orth_poly_expansion_eval3,comb,  */
/*     //                    p, lb, ub, &ao); */
    
/*     //orth_poly_expansion_round(&c); */
/*     //printf("done\n"); */
/*     //\*\/ */
/*     return c; */
/* } */

/* /\********************************************************\//\** */
/* *   Compute the sum of the product between the functions in two arraysarrays */
/* * */
/* *   \param[in] n   - number of functions */
/* *   \param[in] lda - stride of first array */
/* *   \param[in] a   - array of orthonormal polynomial expansions */
/* *   \param[in] ldb - stride of second array */
/* *   \param[in] b   - array of orthonormal polynomial expansions */
/* * */
/* *   \return c - polynomial expansion */
/* * */
/* *   \note  */
/* *       All the functions need to have the same lower  */
/* *       and upper bounds and be of the same type */
/* * */
/* *       If the maximum order of the polynomials is greater than 25 then this is */
/* *       inefficient because I haven't precomputed triple product rules */
/* *************************************************************\/ */
/* struct OrthPolyExpansion * */
/* orth_poly_expansion_sum_prod(size_t n, size_t lda,  */
/*                              struct OrthPolyExpansion ** a, size_t ldb, */
/*                              struct OrthPolyExpansion ** b) */
/* { */

/*     enum poly_type ptype; */
/*     ptype = a[0]->p->ptype; */
/*     struct OrthPolyExpansion * c = NULL; */
/*     double lb = a[0]->lower_bound; */
/*     double ub = a[0]->upper_bound; */

/*     size_t ii; */
/*     size_t maxordera = 0; */
/*     size_t maxorderb = 0; */
/*     size_t maxorder = 0; */
/*     //int legen = 1; */
/*     for (ii = 0; ii < n; ii++){ */

/*         if (a[ii*lda]->p->ptype !=  b[ii*ldb]->p->ptype){ */
/*             return c; // cant do it */
/*         } */
/*         else if (a[ii*lda]->p->ptype != ptype){ */
/*             return c; */
/*         } */
/*         size_t neworder = a[ii*lda]->num_poly + b[ii*ldb]->num_poly; */
/*         if (neworder > maxorder){ */
/*             maxorder = neworder; */
/*         } */
/*         if (a[ii*lda]->num_poly > maxordera){ */
/*             maxordera = a[ii*lda]->num_poly; */
/*         } */
/*         if (b[ii*ldb]->num_poly > maxorderb){ */
/*             maxorderb = b[ii*ldb]->num_poly; */
/*         } */
/*     } */
/*     if ( (maxordera > 99) || (maxorderb > 99) || (ptype != LEGENDRE)){ */
/*         printf("OrthPolyExpansion sum_product greater than order 100 is slow\n"); */
/*         c = orth_poly_expansion_prod(a[0],b[0]); */
/*         for (ii = 1; ii< n; ii++){ */
/*             struct OrthPolyExpansion * temp =  */
/*                 orth_poly_expansion_prod(a[ii*lda],b[ii*ldb]); */
/*             orth_poly_expansion_axpy(1.0,temp,c); */
/*             orth_poly_expansion_free(temp);  */
/*             temp = NULL; */
/*         } */
/*     } */
/*     else{ */
/*         enum poly_type p = LEGENDRE; */
/*         c = orth_poly_expansion_init(p, maxorder, lb, ub); */
/*         size_t kk,jj,ll; */
/*         double * allprods = calloc_double( maxorderb * maxordera); */
/*         for (kk = 0; kk < n; kk++){ */
/*             for (ii = 0; ii < a[kk*lda]->num_poly; ii++){ */
/*                 for (jj = 0; jj < b[kk*ldb]->num_poly; jj++){ */
/*                     allprods[jj + ii * maxorderb] +=  */
/*                             a[kk*lda]->coeff[ii] * b[kk*ldb]->coeff[jj]; */
/*                 } */
/*             } */
/*         } */

/*         for (ll = 0; ll < c->num_poly; ll++){ */
/*             for (ii = 0; ii < maxordera; ii++){ */
/*                 for (jj = 0; jj < maxorderb; jj++){ */
/*                     c->coeff[ll] +=  lpolycoeffs[ii+jj*100+ll*10000] *  */
/*                                         allprods[jj+ii*maxorderb]; */
/*                 } */
/*             } */
/*         } */
/*         free(allprods); allprods=NULL; */
/*         orth_poly_expansion_round(&c); */
/*     } */
/*     return c; */
/* } */

/* /\********************************************************\//\** */
/* *   Compute a linear combination of generic functions */
/* * */
/* *   \param[in] n   - number of functions */
/* *   \param[in] ldx - stride of first array */
/* *   \param[in] x   - functions */
/* *   \param[in] ldc - stride of coefficients */
/* *   \param[in] c   - scaling coefficients */
/* * */
/* *   \return  out  = \f$ \sum_{i=1}^n coeff[ldc[i]] * gfa[ldgf[i]] \f$ */
/* * */
/* *   \note  */
/* *       If both arrays do not consist of only LEGENDRE polynomials */
/* *       return NULL. All the functions need to have the same lower  */
/* *       and upper bounds */
/* *************************************************************\/ */
/* struct OrthPolyExpansion * */
/* orth_poly_expansion_lin_comb(size_t n, size_t ldx,  */
/*                              struct OrthPolyExpansion ** x, size_t ldc, */
/*                              const double * c ) */
/* { */

/*     struct OrthPolyExpansion * out = NULL; */
/*     double lb = x[0]->lower_bound; */
/*     double ub = x[0]->upper_bound; */
/*     enum poly_type ptype = x[0]->p->ptype; */
/*     size_t ii; */
/*     size_t maxorder = 0; */
/*     //int legen = 1; */
/*     for (ii = 0; ii < n; ii++){ */
/*         if (x[ii*ldx]->p->ptype != ptype){ */
/*             //legen = 0; */
/*             return out; // cant do it */
/*         } */
/*         size_t neworder = x[ii*ldx]->num_poly; */
/*         if (neworder > maxorder){ */
/*             maxorder = neworder; */
/*         } */
/*     } */
    

/*     out = orth_poly_expansion_init(ptype, maxorder, lb, ub); */
/*     space_mapping_free(out->space_transform); */
/*     out->space_transform = space_mapping_copy(x[0]->space_transform); */
/*     size_t kk; */
/*     for (kk = 0; kk < n; kk++){ */
/*         for (ii = 0; ii < x[kk*ldx]->num_poly; ii++){ */
/*             out->coeff[ii] +=  c[kk*ldc]*x[kk*ldx]->coeff[ii]; */
/*         } */
/*     } */
/*     orth_poly_expansion_round(&out); */
/*     return out; */
/* } */

/* /\********************************************************\//\** */
/* *   Integrate an orthogonal polynomial expansion  */
/* * */
/* *   \param[in] poly - polynomial to integrate */
/* * */
/* *   \return out - Integral of approximation */
/* * */
/* *   \note  */
/* *       Need to an 'else' or default behavior to switch case */
/* *       int_{lb}^ub  f(x) dx */
/* *    For Hermite polynomials this integrates with respec to */
/* *    the Gaussian weight */
/* *************************************************************\/ */
/* double */
/* orth_poly_expansion_integrate(const struct OrthPolyExpansion * poly) */
/* { */
/*     double out = 0.0; */
/*     switch (poly->p->ptype){ */
/*     case LEGENDRE:  out = legendre_integrate(poly); break; */
/*     case HERMITE:   out = hermite_integrate(poly);  break; */
/*     case CHEBYSHEV: out = cheb_integrate2(poly);    break; */
/*     case STANDARD:  fprintf(stderr, "Cannot integrate STANDARD type\n"); break; */
/*     } */
/*     return out; */
/* } */

/* /\********************************************************\//\** */
/* *   Integrate an orthogonal polynomial expansion  */
/* * */
/* *   \param[in] poly - polynomial to integrate */
/* * */
/* *   \return out - Integral of approximation */
/* * */
/*     \note Computes  \f$ \int f(x) w(x) dx \f$ for every univariate function */
/*     in the qmarray */
    
/*     w(x) depends on underlying parameterization */
/*     for example, it is 1/2 for legendre (and default for others), */
/*     gauss for hermite,etc */
/* *************************************************************\/ */
/* double */
/* orth_poly_expansion_integrate_weighted(const struct OrthPolyExpansion * poly) */
/* { */
/*     double out = 0.0; */
/*     switch (poly->p->ptype){ */
/*     case LEGENDRE:  out = poly->coeff[0];  break; */
/*     case HERMITE:   out = poly->coeff[0];  break; */
/*     case CHEBYSHEV: out = poly->coeff[0];  break; */
/*     case STANDARD:  fprintf(stderr, "Cannot integrate STANDARD type\n"); break; */
/*     } */

/*     return out; */
/* } */


/* /\********************************************************\//\** */
/* *   Weighted inner product between two polynomial  */
/* *   expansions of the same type */
/* * */
/* *   \param[in] a - first polynomial */
/* *   \param[in] b - second polynomai */
/* * */
/* *   \return inner product */
/* * */
/* *   \note */
/* *       Computes \f[ \int_{lb}^ub  a(x)b(x) w(x) dx \f] */
/* * */
/* *************************************************************\/ */
/* double */
/* orth_poly_expansion_inner_w(const struct OrthPolyExpansion * a, */
/*                             const struct OrthPolyExpansion * b) */
/* { */
/*     assert(a->p->ptype == b->p->ptype); */
    
/*     double out = 0.0; */
/*     size_t N = a->num_poly < b->num_poly ? a->num_poly : b->num_poly; */
/*     size_t ii; */
/*     for (ii = 0; ii < N; ii++){ */
/*         out += a->coeff[ii] * b->coeff[ii] * a->p->norm(ii);  */
/*     } */

/*     return out; */
/* } */

/* /\********************************************************\//\** */
/* *   Inner product between two polynomial expansions of the same type */
/* * */
/* *   \param[in] a - first polynomial */
/* *   \param[in] b - second polynomai */
/* * */
/* *   \return  inner product */
/* * */
/* *   \note */
/* *   If the polynomial is NOT HERMITE then */
/* *   Computes  \f$ \int_{lb}^ub  a(x)b(x) dx \f$ by first */
/* *   converting each polynomial to a Legendre polynomial */
/* *   Otherwise it computes the error with respect to gaussia weight */
/* *************************************************************\/ */
/* double */
/* orth_poly_expansion_inner(const struct OrthPolyExpansion * a, */
/*                           const struct OrthPolyExpansion * b) */
/* {    */
/*     struct OrthPolyExpansion * t1 = NULL; */
/*     struct OrthPolyExpansion * t2 = NULL; */
    
/* //    assert (a->ptype == b->ptype); */
/* //    enum poly_type ptype = a->ptype; */
/* //    switch (ptype) */
/*     if ((a->p->ptype == HERMITE) && (b->p->ptype == HERMITE)){ */
/*         return orth_poly_expansion_inner_w(a,b); */
/*     } */
/*     else if ((a->p->ptype == CHEBYSHEV) && (b->p->ptype == CHEBYSHEV)){ */

/*         struct OrthPolyExpansion * prod = orth_poly_expansion_prod(a, b); */
/*         double int_val = orth_poly_expansion_integrate(prod); */
/*         orth_poly_expansion_free(prod); prod = NULL; */
/*         return int_val; */
/*         /\* // can possibly make this more efficient *\/ */
/*         /\* double out = 0.0; *\/ */
/*         /\* size_t N = a->num_poly < b->num_poly ? a->num_poly : b->num_poly; *\/ */
/*         /\* for (size_t ii = 0; ii < N; ii++){ *\/ */
/*         /\*     for (size_t jj = 0; jj < ii; jj++){ *\/ */
/*         /\*         if ( ((ii+jj) % 2) == 0){ *\/ */
/*         /\*             out += (a->coeff[ii]*b->coeff[jj] *  *\/ */
/*         /\*                      (1.0 / (1.0 - (double) (ii-jj)*(ii-jj)) *\/ */
/*         /\*                       + 1.0 / (1.0 - (double) (ii+jj)*(ii+jj)))); *\/ */
/*         /\*         } *\/ */
/*         /\*     } *\/ */
/*         /\*     for (size_t jj = ii; jj < N; jj++){ *\/ */
/*         /\*         if ( ((ii+jj) % 2) == 0){ *\/ */
/*         /\*             out += (a->coeff[ii]*b->coeff[jj] *  *\/ */
/*         /\*                      (1.0 / (1.0 - (double) (jj-ii)*(jj-ii)) *\/ */
/*         /\*                       + 1.0 / (1.0 - (double) (ii+jj)*(ii+jj)))); *\/ */
/*         /\*         } *\/ */
/*         /\*     } *\/ */
/*         /\* } *\/ */
/*         /\* double m = (a->upper_bound - a->lower_bound) / (a->p->upper - a->p->lower); *\/ */
/*         /\* out *=  m; *\/ */
/*         /\* return out *\/; */
/*     } */
/*     else{ */
/*         if (a->p->ptype == CHEBYSHEV){ */
/*             t1 = orth_poly_expansion_init(LEGENDRE, a->num_poly, */
/*                                           a->lower_bound, a->upper_bound); */
/*             orth_poly_expansion_approx(&orth_poly_expansion_eval2, (void*)a, t1); */
/*             orth_poly_expansion_round(&t1); */
/*         } */
/*         else if (a->p->ptype != LEGENDRE){ */
/*             fprintf(stderr, "Don't know how to take inner product using polynomial type. \n"); */
/*             fprintf(stderr, "type1 = %d, and type2= %d\n",a->p->ptype,b->p->ptype); */
/*             exit(1); */
/*         } */

/*         if (b->p->ptype == CHEBYSHEV){ */
/*             t2 = orth_poly_expansion_init(LEGENDRE, b->num_poly, */
/*                                           b->lower_bound, b->upper_bound); */
/*             orth_poly_expansion_approx(&orth_poly_expansion_eval2, (void*)b, t2); */
/*             orth_poly_expansion_round(&t2); */
/*         } */
/*         else if (b->p->ptype != LEGENDRE){ */
/*             fprintf(stderr, "Don't know how to take inner product using polynomial type. \n"); */
/*             fprintf(stderr, "type1 = %d, and type2= %d\n",a->p->ptype,b->p->ptype); */
/*             exit(1); */
/*         } */

/*         double out; */
/*         if ((t1 == NULL) && (t2 == NULL)){ */
/*             out = orth_poly_expansion_inner_w(a,b) * (a->upper_bound - a->lower_bound); */
/*         } */
/*         else if ((t1 == NULL) && (t2 != NULL)){ */
/*             out = orth_poly_expansion_inner_w(a,t2) * (a->upper_bound - a->lower_bound); */
/*             orth_poly_expansion_free(t2); t2 = NULL; */
/*         } */
/*         else if ((t2 == NULL) && (t1 != NULL)){ */
/*             out = orth_poly_expansion_inner_w(t1,b) * (b->upper_bound - b->lower_bound); */
/*             orth_poly_expansion_free(t1); t1 = NULL; */
/*         } */
/*         else{ */
/*             out = orth_poly_expansion_inner_w(t1,t2) * (t1->upper_bound - t1->lower_bound); */
/*             orth_poly_expansion_free(t1); t1 = NULL; */
/*             orth_poly_expansion_free(t2); t2 = NULL; */
/*         } */
/*         return out; */
/*     } */
/* } */

/* /\********************************************************\//\** */
/* *   Compute the norm of an orthogonal polynomial */
/* *   expansion with respect to family weighting  */
/* *   function */
/* * */
/* *   \param[in] p - polynomial to integrate */
/* * */
/* *   \return out - norm of function */
/* * */
/* *   \note */
/* *        Computes  \f$ \sqrt(\int f(x)^2 w(x) dx) \f$ */
/* *************************************************************\/ */
/* double orth_poly_expansion_norm_w(const struct OrthPolyExpansion * p){ */

/*     double out = sqrt(orth_poly_expansion_inner_w(p,p)); */
/*     return sqrt(out); */
/* } */

/* /\********************************************************\//\** */
/* *   Compute the norm of an orthogonal polynomial */
/* *   expansion with respect to uniform weighting  */
/* *   (except in case of HERMITE, then do gaussian weighting) */
/* * */
/* *   \param[in] p - polynomial of which to obtain norm */
/* * */
/* *   \return out - norm of function */
/* * */
/* *   \note */
/* *        Computes \f$ \sqrt(\int_a^b f(x)^2 dx) \f$ */
/* *************************************************************\/ */
/* double orth_poly_expansion_norm(const struct OrthPolyExpansion * p){ */

/*     double out = 0.0; */
/*     out = sqrt(orth_poly_expansion_inner(p,p)); */
/*     return out; */
/* } */

/* /\********************************************************\//\** */
/* *   Multiply polynomial expansion by -1 */
/* * */
/* *   \param[in,out] p - polynomial multiply by -1 */
/* *************************************************************\/ */
/* void  */
/* orth_poly_expansion_flip_sign(struct OrthPolyExpansion * p) */
/* {    */
/*     size_t ii; */
/*     for (ii = 0; ii < p->num_poly; ii++){ */
/*         p->coeff[ii] *= -1.0; */
/*     } */
/* } */

/* /\********************************************************\//\** */
/* *   Multiply by scalar and overwrite expansion */
/* * */
/* *   \param[in] a - scaling factor for first polynomial */
/* *   \param[in] x - polynomial to scale */
/* *************************************************************\/ */
/* void orth_poly_expansion_scale(double a, struct OrthPolyExpansion * x) */
/* { */
    
/*     size_t ii; */
/*     for (ii = 0; ii < x->num_poly; ii++){ */
/*         x->coeff[ii] *= a; */
/*     } */
/*     orth_poly_expansion_round(&x); */
/* } */

/* /\********************************************************\//\** */
/* *   Multiply and add 3 expansions \f$ z \leftarrow ax + by + cz \f$ */
/* * */
/* *   \param[in] a  - scaling factor for first polynomial */
/* *   \param[in] x  - first polynomial */
/* *   \param[in] b  - scaling factor for second polynomial */
/* *   \param[in] y  - second polynomial */
/* *   \param[in] c  - scaling factor for third polynomial */
/* *   \param[in] z  - third polynomial */
/* * */
/* *************************************************************\/ */
/* void */
/* orth_poly_expansion_sum3_up(double a, struct OrthPolyExpansion * x, */
/*                            double b, struct OrthPolyExpansion * y, */
/*                            double c, struct OrthPolyExpansion * z) */
/* { */
/*     assert (x->p->ptype == y->p->ptype); */
/*     assert (y->p->ptype == z->p->ptype); */
    
/*     assert ( x != NULL ); */
/*     assert ( y != NULL ); */
/*     assert ( z != NULL ); */
    
/*     size_t ii; */
/*     if ( (z->num_poly >= x->num_poly) && (z->num_poly >= y->num_poly) ){ */
        
/*         if (x->num_poly > y->num_poly){ */
/*             for (ii = 0; ii < y->num_poly; ii++){ */
/*                 z->coeff[ii] = c*z->coeff[ii] + a*x->coeff[ii] + b*y->coeff[ii]; */
/*             } */
/*             for (ii = y->num_poly; ii < x->num_poly; ii++){ */
/*                 z->coeff[ii] = c*z->coeff[ii] + a*x->coeff[ii]; */
/*             } */
/*             for (ii = x->num_poly; ii < z->num_poly; ii++){ */
/*                 z->coeff[ii] = c*z->coeff[ii]; */
/*             } */
/*         } */
/*         else{ */
/*             for (ii = 0; ii < x->num_poly; ii++){ */
/*                 z->coeff[ii] = c*z->coeff[ii] + a*x->coeff[ii] + b*y->coeff[ii]; */
/*             } */
/*             for (ii = x->num_poly; ii < y->num_poly; ii++){ */
/*                 z->coeff[ii] = c*z->coeff[ii] + b*y->coeff[ii]; */
/*             } */
/*             for (ii = x->num_poly; ii < z->num_poly; ii++){ */
/*                 z->coeff[ii] = c*z->coeff[ii]; */
/*             } */
/*         } */
/*     } */
/*     else if ((z->num_poly >= x->num_poly) && ( z->num_poly < y->num_poly)) { */
/*         double * temp = realloc(z->coeff, (y->num_poly)*sizeof(double)); */
/*         if (temp == NULL){ */
/*             fprintf(stderr,"cannot allocate new size fo z-coeff in sum3_up\n"); */
/*             exit(1); */
/*         } */
/*         else{ */
/*             z->coeff = temp; */
/*         } */
/*         for (ii = 0; ii < x->num_poly; ii++){ */
/*             z->coeff[ii] = c*z->coeff[ii]+a*x->coeff[ii]+b*y->coeff[ii]; */
/*         } */
/*         for (ii = x->num_poly; ii < z->num_poly; ii++){ */
/*             z->coeff[ii] = c*z->coeff[ii] + b*y->coeff[ii]; */
/*         } */
/*         for (ii = z->num_poly; ii < y->num_poly; ii++){ */
/*             z->coeff[ii] = b*y->coeff[ii]; */
/*         } */
/*         z->num_poly = y->num_poly; */
/*     } */
/*     else if ( (z->num_poly < x->num_poly) && ( z->num_poly >= y->num_poly) ){ */
/*         double * temp = realloc(z->coeff, (x->num_poly)*sizeof(double)); */
/*         if (temp == NULL){ */
/*             fprintf(stderr,"cannot allocate new size fo z-coeff in sum3_up\n"); */
/*             exit(1); */
/*         } */
/*         else{ */
/*             z->coeff = temp; */
/*         } */
/*         for (ii = 0; ii < y->num_poly; ii++){ */
/*             z->coeff[ii] = c*z->coeff[ii]+a*x->coeff[ii]+b*y->coeff[ii]; */
/*         } */
/*         for (ii = y->num_poly; ii < z->num_poly; ii++){ */
/*             z->coeff[ii] = c*z->coeff[ii] + a*x->coeff[ii]; */
/*         } */
/*         for (ii = z->num_poly; ii < x->num_poly; ii++){ */
/*             z->coeff[ii] = a*x->coeff[ii]; */
/*         } */
/*         z->num_poly = x->num_poly; */
/*     } */
/*     else if ( x->num_poly <= y->num_poly){ */
/*         double * temp = realloc(z->coeff, (y->num_poly)*sizeof(double)); */
/*         if (temp == NULL){ */
/*             fprintf(stderr,"cannot allocate new size fo z-coeff in sum3_up\n"); */
/*             exit(1); */
/*         } */
/*         for (ii = 0; ii < z->num_poly; ii++){ */
/*             z->coeff[ii] = c*z->coeff[ii]+a*x->coeff[ii]+b*y->coeff[ii]; */
/*         } */
/*         for (ii = z->num_poly; ii < x->num_poly; ii++){ */
/*             z->coeff[ii] = a*x->coeff[ii] + b*y->coeff[ii]; */
/*         } */
/*         for (ii = x->num_poly; ii < y->num_poly; ii++){ */
/*             z->coeff[ii] = b*y->coeff[ii]; */
/*         } */
/*         z->num_poly = y->num_poly; */
/*     } */
/*     else if (y->num_poly <= x->num_poly) { */
/*         double * temp = realloc(z->coeff, (x->num_poly)*sizeof(double)); */
/*         if (temp == NULL){ */
/*             fprintf(stderr,"cannot allocate new size fo z-coeff in sum3_up\n"); */
/*             exit(1); */
/*         } */
/*         for (ii = 0; ii < z->num_poly; ii++){ */
/*             z->coeff[ii] = c*z->coeff[ii]+a*x->coeff[ii]+b*y->coeff[ii]; */
/*         } */
/*         for (ii = z->num_poly; ii < y->num_poly; ii++){ */
/*             z->coeff[ii] = a*x->coeff[ii] + b*y->coeff[ii]; */
/*         } */
/*         for (ii = y->num_poly; ii < x->num_poly; ii++){ */
/*             z->coeff[ii] = a*x->coeff[ii]; */
/*         } */
/*         z->num_poly = x->num_poly; */
/*     } */
/*     else{ */
/*         fprintf(stderr,"Haven't accounted for anything else?! %zu %zu %zu\n",  */
/*                 x->num_poly, y->num_poly, z->num_poly); */
/*         exit(1); */
/*     } */
/*     //   z->nalloc = z->num_poly + OPECALLOC; */
/*     orth_poly_expansion_round(&z); */
/* } */

/* /\********************************************************\//\** */
/* *   Multiply by scalar and add two orthgonal  */
/* *   expansions of the same family together \f[ y \leftarrow ax + y \f] */
/* * */
/* *   \param[in] a  - scaling factor for first polynomial */
/* *   \param[in] x  - first polynomial */
/* *   \param[in] y  - second polynomial */
/* * */
/* *   \return 0 if successfull 1 if error with allocating more space for y */
/* * */
/* *   \note  */
/* *       Computes z=ax+by, where x and y are polynomial expansionx */
/* *       Requires both polynomials to have the same upper  */
/* *       and lower bounds */
/* *        */
/* **************************************************************\/ */
/* int orth_poly_expansion_axpy(double a, struct OrthPolyExpansion * x, */
/*                              struct OrthPolyExpansion * y) */
/* { */
        
/*     assert (y != NULL); */
/*     assert (x != NULL); */
/*     assert (x->p->ptype == y->p->ptype); */
/*     assert ( fabs(x->lower_bound - y->lower_bound) < DBL_EPSILON ); */
/*     assert ( fabs(x->upper_bound - y->upper_bound) < DBL_EPSILON ); */
    
/*     if (x->num_poly < y->num_poly){ */
/*         // shouldnt need rounding here */
/*         size_t ii; */
/*         for (ii = 0; ii < x->num_poly; ii++){ */
/*             y->coeff[ii] += a * x->coeff[ii]; */
/*             if (fabs(y->coeff[ii]) < ZEROTHRESH){ */
/*                 y->coeff[ii] = 0.0; */
/*             } */
/*         } */
/*     } */
/*     else{ */
/*         size_t ii; */
/*         if (x->num_poly > y->nalloc){ */
/*             //printf("hereee\n"); */
/*             y->nalloc = x->num_poly+10; */
/*             double * temp = realloc(y->coeff, (y->nalloc)*sizeof(double)); */
/*             if (temp == NULL){ */
/*                 return 0; */
/*             } */
/*             else{ */
/*                 y->coeff = temp; */
/*                 for (ii = y->num_poly; ii < y->nalloc; ii++){ */
/*                     y->coeff[ii] = 0.0; */
/*                 } */
/*             } */
/*             //printf("finished\n"); */
/*         } */
/*         for (ii = y->num_poly; ii < x->num_poly; ii++){ */
/*             y->coeff[ii] = a * x->coeff[ii]; */
/*             if (fabs(y->coeff[ii]) < ZEROTHRESH){ */
/*                 y->coeff[ii] = 0.0; */
/*             } */
/*         } */
/*         for (ii = 0; ii < y->num_poly; ii++){ */
/*             y->coeff[ii] += a * x->coeff[ii]; */
/*             if (fabs(y->coeff[ii]) < ZEROTHRESH){ */
/*                 y->coeff[ii] = 0.0; */
/*             } */
/*         } */
/*         y->num_poly = x->num_poly; */
/*         size_t nround = y->num_poly; */
/*         for (ii = 0; ii < y->num_poly-1;ii++){ */
/*             if (fabs(y->coeff[y->num_poly-1-ii]) > ZEROTHRESH){ */
/*                 break; */
/*             } */
/*             else{ */
/*                 nround = nround-1; */
/*             } */
/*         } */
/*         y->num_poly = nround; */
/*     } */
    
/*     return 0; */
/* } */


/* /\********************************************************\//\** */
/* *   Multiply by scalar and add two orthgonal  */
/* *   expansions of the same family together */
/* * */
/* *   \param[in] a - scaling factor for first polynomial */
/* *   \param[in] x - first polynomial */
/* *   \param[in] b - scaling factor for second polynomial */
/* *   \param[in] y  second polynomial */
/* * */
/* *   \return p - orthogonal polynomial expansion */
/* * */
/* *   \note  */
/* *       Computes z=ax+by, where x and y are polynomial expansionx */
/* *       Requires both polynomials to have the same upper  */
/* *       and lower bounds */
/* *    */
/* *************************************************************\/ */
/* struct OrthPolyExpansion * */
/* orth_poly_expansion_daxpby(double a, struct OrthPolyExpansion * x, */
/*                            double b, struct OrthPolyExpansion * y) */
/* { */
/*     /\* */
/*     printf("a=%G b=%G \n",a,b); */
/*     printf("x=\n"); */
/*     print_orth_poly_expansion(x,0,NULL); */
/*     printf("y=\n"); */
/*     print_orth_poly_expansion(y,0,NULL); */
/*     *\/ */
    
/*     //double diffa = fabs(a-ZEROTHRESH); */
/*     //double diffb = fabs(b-ZEROTHRESH); */
/*     size_t ii; */
/*     struct OrthPolyExpansion * p ; */
/*     //if ( (x == NULL && y != NULL) || ((diffa <= ZEROTHRESH) && (y != NULL))){ */
/*     if ( (x == NULL && y != NULL)){ */
/*         //printf("b = %G\n",b); */
/*         //if (diffb <= ZEROTHRESH){ */
/*         //    p = orth_poly_expansion_init(y->p->ptype,1,y->lower_bound, y->upper_bound); */
/*        // } */
/*        // else{     */
/*             p = orth_poly_expansion_init(y->p->ptype, */
/*                         y->num_poly, y->lower_bound, y->upper_bound); */
/*             space_mapping_free(p->space_transform); */
/*             p->space_transform = space_mapping_copy(y->space_transform); */
/*             for (ii = 0; ii < y->num_poly; ii++){ */
/*                 p->coeff[ii] = y->coeff[ii] * b; */
/*             } */
/*         //} */
/*         orth_poly_expansion_round(&p); */
/*         return p; */
/*     } */
/*     //if ( (y == NULL && x != NULL) || ((diffb <= ZEROTHRESH) && (x != NULL))){ */
/*     if ( (y == NULL && x != NULL)){ */
/*         //if (a <= ZEROTHRESH){ */
/*         //    p = orth_poly_expansion_init(x->p->ptype,1, x->lower_bound, x->upper_bound); */
/*        // } */
/*         //else{ */
/*             p = orth_poly_expansion_init(x->p->ptype, */
/*                         x->num_poly, x->lower_bound, x->upper_bound); */
/*             space_mapping_free(p->space_transform); */
/*             p->space_transform = space_mapping_copy(x->space_transform); */
/*             for (ii = 0; ii < x->num_poly; ii++){ */
/*                 p->coeff[ii] = x->coeff[ii] * a; */
/*             } */
/*         //} */
/*         orth_poly_expansion_round(&p); */
/*         return p; */
/*     } */

/*     size_t N = x->num_poly > y->num_poly ? x->num_poly : y->num_poly; */

/*     p = orth_poly_expansion_init(x->p->ptype, */
/*                     N, x->lower_bound, x->upper_bound); */
/*     space_mapping_free(p->space_transform); */
/*     p->space_transform = space_mapping_copy(x->space_transform); */
    
/*     size_t xN = x->num_poly; */
/*     size_t yN = y->num_poly; */

/*     //printf("diffa = %G, x==NULL %d\n",diffa,x==NULL); */
/*     //printf("diffb = %G, y==NULL %d\n",diffb,y==NULL); */
/*    // assert(diffa > ZEROTHRESH); */
/*    // assert(diffb > ZEROTHRESH); */
/*     if (xN > yN){ */
/*         for (ii = 0; ii < yN; ii++){ */
/*             p->coeff[ii] = x->coeff[ii]*a + y->coeff[ii]*b;            */
/*             //if ( fabs(p->coeff[ii]) < ZEROTHRESH){ */
/*             //    p->coeff[ii] = 0.0; */
/*            // } */
/*         } */
/*         for (ii = yN; ii < xN; ii++){ */
/*             p->coeff[ii] = x->coeff[ii]*a; */
/*             //if ( fabs(p->coeff[ii]) < ZEROTHRESH){ */
/*             //    p->coeff[ii] = 0.0; */
/*            // } */
/*         } */
/*     } */
/*     else{ */
/*         for (ii = 0; ii < xN; ii++){ */
/*             p->coeff[ii] = x->coeff[ii]*a + y->coeff[ii]*b;            */
/*             //if ( fabs(p->coeff[ii]) < ZEROTHRESH){ */
/*             //    p->coeff[ii] = 0.0; */
/*            // } */
/*         } */
/*         for (ii = xN; ii < yN; ii++){ */
/*             p->coeff[ii] = y->coeff[ii]*b; */
/*             //if ( fabs(p->coeff[ii]) < ZEROTHRESH){ */
/*             //    p->coeff[ii] = 0.0; */
/*             //} */
/*         } */
/*     } */

/*     orth_poly_expansion_round(&p); */
/*     return p; */
/* } */

/* //////////////////////////////////////////////////////////////////////////// */
/* // Algorithms */

/* /\********************************************************\//\** */
/* *   Obtain the real roots of a standard polynomial */
/* * */
/* *   \param[in]     p     - standard polynomial */
/* *   \param[in,out] nkeep - returns how many real roots tehre are */
/* * */
/* *   \return real_roots - real roots of a standard polynomial */
/* * */
/* *   \note */
/* *   Only roots within the bounds are returned */
/* *************************************************************\/ */
/* double * */
/* standard_poly_real_roots(struct StandardPoly * p, size_t * nkeep) */
/* { */
/*     if (p->num_poly == 1) // constant function */
/*     {    */
/*         double * real_roots = NULL; */
/*         *nkeep = 0; */
/*         return real_roots; */
/*     } */
/*     else if (p->num_poly == 2){ // linear */
/*         double root = -p->coeff[0] / p->coeff[1]; */
        
/*         if ((root > p->lower_bound) && (root < p->upper_bound)){ */
/*             *nkeep = 1; */
/*         } */
/*         else{ */
/*             *nkeep = 0; */
/*         } */
/*         double * real_roots = NULL; */
/*         if (*nkeep == 1){ */
/*             real_roots = calloc_double(1); */
/*             real_roots[0] = root; */
/*         } */
/*         return real_roots; */
/*     } */
    
/*     size_t nrows = p->num_poly-1; */
/*     //printf("coeffs = \n"); */
/*     //dprint(p->num_poly, p->coeff); */
/*     while (fabs(p->coeff[nrows]) < ZEROTHRESH ){ */
/*     //while (fabs(p->coeff[nrows]) < DBL_MIN){ */
/*         nrows--; */
/*         if (nrows == 1){ */
/*             break; */
/*         } */
/*     } */

/*     //printf("nrows left = %zu \n",  nrows); */
/*     if (nrows == 1) // linear */
/*     { */
/*         double root = -p->coeff[0] / p->coeff[1]; */
/*         if ((root > p->lower_bound) && (root < p->upper_bound)){ */
/*             *nkeep = 1; */
/*         } */
/*         else{ */
/*             *nkeep = 0; */
/*         } */
/*         double * real_roots = NULL; */
/*         if (*nkeep == 1){ */
/*             real_roots = calloc_double(1); */
/*             real_roots[0] = root; */
/*         } */
/*         return real_roots; */
/*     } */
/*     else if (nrows == 0) */
/*     { */
/*         double * real_roots = NULL; */
/*         *nkeep = 0; */
/*         return real_roots; */
/*     } */

/*     // transpose of the companion matrix */
/*     double * t_companion = calloc_double((p->num_poly-1)*(p->num_poly-1)); */
/*     size_t ii; */
    

/*    // size_t m = nrows; */
/*     t_companion[nrows-1] = -p->coeff[0]/p->coeff[nrows]; */
/*     for (ii = 1; ii < nrows; ii++){ */
/*         t_companion[ii * nrows + ii-1] = 1.0; */
/*         t_companion[ii * nrows + nrows-1] = -p->coeff[ii]/p->coeff[nrows]; */
/*     } */
/*     double * real = calloc_double(nrows); */
/*     double * img = calloc_double(nrows); */
/*     int info; */
/*     int lwork = 8 * nrows; */
/*     double * iwork = calloc_double(8 * nrows); */
/*     //double * vl; */
/*     //double * vr; */
/*     int n = nrows; */

/*     //printf("hello! n=%d \n",n); */
/*     dgeev_("N","N", &n, t_companion, &n, real, img, NULL, &n, */
/*            NULL, &n, iwork, &lwork, &info); */
    
/*     //printf("info = %d", info); */

/*     free (iwork); */
    
/*     int * keep = calloc_int(nrows); */
/*     *nkeep = 0; */
/*     // the 1e-10 is kinda hacky */
/*     for (ii = 0; ii < nrows; ii++){ */
/*         //printf("real[ii] - p->lower_bound = %G\n",real[ii]-p->lower_bound); */
/*         //printf("real root = %3.15G, imag = %G \n",real[ii],img[ii]); */
/*         //printf("lower thresh = %3.20G\n",p->lower_bound-1e-8); */
/*         //printf("zero thresh = %3.20G\n",1e-8); */
/*         //printf("upper thresh = %G\n",p->upper_bound+ZEROTHRESH); */
/*         //printf("too low? %d \n", real[ii] < (p->lower_bound-1e-8)); */
/*         if ((fabs(img[ii]) < 1e-7) &&  */
/*             (real[ii] > (p->lower_bound-1e-8)) &&  */
/*             //(real[ii] >= (p->lower_bound-1e-7)) &&  */
/*             (real[ii] < (p->upper_bound+1e-8))) { */
/*             //(real[ii] <= (p->upper_bound+1e-7))) { */
        
/*             //\* */
/*             if (real[ii] < p->lower_bound){ */
/*                 real[ii] = p->lower_bound; */
/*             } */
/*             if (real[ii] > p->upper_bound){ */
/*                 real[ii] = p->upper_bound; */
/*             } */
/*             //\*\/ */

/*             keep[ii] = 1; */
/*             *nkeep = *nkeep + 1; */
/*             //printf("keep\n"); */
/*         } */
/*         else{ */
/*             keep[ii] = 0; */
/*         } */
/*     } */
    
/*     /\* */
/*     printf("real portions roots = "); */
/*     dprint(nrows, real); */
/*     printf("imag portions roots = "); */
/*     for (ii = 0; ii < nrows; ii++) printf("%E ",img[ii]); */
/*     printf("\n"); */
/*     //dprint(nrows, img); */
/*     *\/ */

/*     double * real_roots = calloc_double(*nkeep); */
/*     size_t counter = 0; */
/*     for (ii = 0; ii < nrows; ii++){ */
/*         if (keep[ii] == 1){ */
/*             real_roots[counter] = real[ii]; */
/*             counter++; */
/*         } */

/*     } */
    
/*     free(t_companion); */
/*     free(real); */
/*     free(img); */
/*     free(keep); */

/*     return real_roots; */
/* } */

/* static int dblcompare(const void * a, const void * b) */
/* { */
/*     const double * aa = a; */
/*     const double * bb = b; */
/*     if ( *aa < *bb){ */
/*         return -1; */
/*     } */
/*     return 1; */
/* } */

/* /\********************************************************\//\** */
/* *   Obtain the real roots of a legendre polynomial expansion */
/* * */
/* *   \param[in]     p     - orthogonal polynomial expansion */
/* *   \param[in,out] nkeep - returns how many real roots tehre are */
/* * */
/* *   \return real_roots - real roots of an orthonormal polynomial expansion */
/* * */
/* *   \note */
/* *       Only roots within the bounds are returned */
/* *       Algorithm is based on eigenvalues of non-standard companion matrix from */
/* *       Roots of Polynomials Expressed in terms of orthogonal polynomials */
/* *       David Day and Louis Romero 2005 */
/* * */
/* *       Multiplying by a factor of sqrt(2*N+1) because using orthonormal, */
/* *       rather than orthogonal polynomials */
/* *************************************************************\/ */
/* double *  */
/* legendre_expansion_real_roots(struct OrthPolyExpansion * p, size_t * nkeep) */
/* { */

/*     double * real_roots = NULL; // output */
/*     *nkeep = 0; */

/*     double m = (p->upper_bound - p->lower_bound) /  */
/*             (p->p->upper - p->p->lower); */
/*     double off = p->upper_bound - m * p->p->upper; */

/*     orth_poly_expansion_round(&p); */
/*    // print_orth_poly_expansion(p,3,NULL); */
/*     //printf("last 2 = %G\n",p->coeff[p->num_poly-1]); */
/*     size_t N = p->num_poly-1; */
/*     //printf("N = %zu\n",N); */
/*     if (N == 0){ */
/*         return real_roots; */
/*     } */
/*     else if (N == 1){ */
/*         if (fabs(p->coeff[N]) <= ZEROTHRESH){ */
/*             return real_roots; */
/*         } */
/*         else{ */
/*             double root = -p->coeff[0] / p->coeff[1]; */
/*             if ( (root >= -1.0-ZEROTHRESH) && (root <= 1.0 - ZEROTHRESH)){ */
/*                 if (root <-1.0){ */
/*                     root = -1.0; */
/*                 } */
/*                 else if (root > 1.0){ */
/*                     root = 1.0; */
/*                 } */
/*                 *nkeep = 1; */
/*                 real_roots = calloc_double(1); */
/*                 real_roots[0] = m*root+off; */
/*             } */
/*         } */
/*     } */
/*     else{ */
/*         /\* printf("I am here\n"); *\/ */
/*         double * nscompanion = calloc_double(N*N); // nonstandard companion */
/*         size_t ii; */
/*         double hnn1 = - (double) (N) / (2.0 * (double) (N) - 1.0); */
/*         /\* double hnn1 = - 1.0 / p->p->an(N); *\/ */
/*         nscompanion[1] = 1.0; */
/*         /\* nscompanion[(N-1)*N] += hnn1 * p->coeff[0] / p->coeff[N]; *\/ */
/*         nscompanion[(N-1)*N] += hnn1 * p->coeff[0] / (p->coeff[N] * sqrt(2*N+1)); */
/*         for (ii = 1; ii < N-1; ii++){ */
/*             assert (fabs(p->p->bn(ii)) < 1e-14); */
/*             double in = (double) ii; */
/*             nscompanion[ii*N+ii-1] = in / ( 2.0 * in + 1.0); */
/*             nscompanion[ii*N+ii+1] = (in + 1.0) / ( 2.0 * in + 1.0); */

/*             /\* nscompanion[(N-1)*N + ii] += hnn1 * p->coeff[ii] / p->coeff[N]; *\/ */
/*             nscompanion[(N-1)*N + ii] += hnn1 * p->coeff[ii] * sqrt(2*ii+1)/ p->coeff[N] / sqrt(2*N+1); */
/*         } */
/*         nscompanion[N*N-2] += (double) (N-1) / (2.0 * (double) (N-1) + 1.0); */

        
/*         /\* nscompanion[N*N-1] += hnn1 * p->coeff[N-1] / p->coeff[N]; *\/ */
/*         nscompanion[N*N-1] += hnn1 * p->coeff[N-1] * sqrt(2*(N-1)+1)/ p->coeff[N] / sqrt(2*N+1); */
        
/*         //printf("good up to here!\n"); */
/*         //dprint2d_col(N,N,nscompanion); */

/*         int info; */
/*         double * scale = calloc_double(N); */
/*         //\* */
/*         //Balance */
/*         int ILO, IHI; */
/*         //printf("am I here? N=%zu \n",N); */
/*         //dprint(N*N,nscompanion); */
/*         dgebal_("S", (int*)&N, nscompanion, (int *)&N,&ILO,&IHI,scale,&info); */
/*         //printf("yep\n"); */
/*         if (info < 0){ */
/*             fprintf(stderr, "Calling dgebl had error in %d-th input in the legendre_expansion_real_roots function\n",info); */
/*             exit(1); */
/*         } */

/*         //printf("balanced!\n"); */
/*         //dprint2d_col(N,N,nscompanion); */

/*         //IHI = M1; */
/*         //printf("M1=%zu\n",M1); */
/*         //printf("ilo=%zu\n",ILO); */
/*         //printf("IHI=%zu\n",IHI); */
/*         //\*\/ */

/*         double * real = calloc_double(N); */
/*         double * img = calloc_double(N); */
/*         //printf("allocated eigs N = %zu\n",N); */
/*         int lwork = 8 * (int)N; */
/*         //printf("got lwork\n"); */
/*         double * iwork = calloc_double(8*N); */
/*         //printf("go here"); */

/*         //dgeev_("N","N", &N, nscompanion, &N, real, img, NULL, &N, */
/*         //        NULL, &N, iwork, &lwork, &info); */
/*         dhseqr_("E","N",(int*)&N,&ILO,&IHI,nscompanion,(int*)&N,real,img,NULL,(int*)&N,iwork,&lwork,&info); */
/*         //printf("done here"); */

/*         if (info < 0){ */
/*             fprintf(stderr, "Calling dhesqr had error in %d-th input in the legendre_expansion_real_roots function\n",info); */
/*             exit(1); */
/*         } */
/*         else if(info > 0){ */
/*             //fprintf(stderr, "Eigenvalues are still uncovered in legendre_expansion_real_roots function\n"); */
/*            // printf("coeffs are \n"); */
/*            // dprint(p->num_poly, p->coeff); */
/*            // printf("last 2 = %G\n",p->coeff[p->num_poly-1]); */
/*            // exit(1); */
/*         } */

/*       //  printf("eigenvalues \n"); */
/*         size_t * keep = calloc_size_t(N); */
/*         for (ii = 0; ii < N; ii++){ */
/*             /\* printf("(%3.15G, %3.15G)\n",real[ii],img[ii]); *\/ */
/*             if ((fabs(img[ii]) < 1e-6) && (real[ii] > -1.0-1e-12) && (real[ii] < 1.0+1e-12)){ */
/*                 if (real[ii] < -1.0){ */
/*                     real[ii] = -1.0; */
/*                 } */
/*                 else if (real[ii] > 1.0){ */
/*                     real[ii] = 1.0; */
/*                 } */
/*                 keep[ii] = 1; */
/*                 *nkeep = *nkeep + 1; */
/*             } */
/*         } */
        
        
/*         if (*nkeep > 0){ */
/*             real_roots = calloc_double(*nkeep); */
/*             size_t counter = 0; */
/*             for (ii = 0; ii < N; ii++){ */
/*                 if (keep[ii] == 1){ */
/*                     real_roots[counter] = real[ii]*m+off; */
/*                     counter++; */
/*                 } */
/*             } */
/*         } */
     

/*         free(keep); keep = NULL; */
/*         free(iwork); iwork  = NULL; */
/*         free(real); real = NULL; */
/*         free(img); img = NULL; */
/*         free(nscompanion); nscompanion = NULL; */
/*         free(scale); scale = NULL; */
/*     } */

/*     if (*nkeep > 1){ */
/*         qsort(real_roots, *nkeep, sizeof(double), dblcompare); */
/*     } */
/*     return real_roots; */
/* } */

/* /\********************************************************\//\** */
/* *   Obtain the real roots of a chebyshev polynomial expansion */
/* * */
/* *   \param[in]     p     - orthogonal polynomial expansion */
/* *   \param[in,out] nkeep - returns how many real roots tehre are */
/* * */
/* *   \return real_roots - real roots of an orthonormal polynomial expansion */
/* * */
/* *   \note */
/* *       Only roots within the bounds are returned */
/* *       Algorithm is based on eigenvalues of non-standard companion matrix from */
/* *       Roots of Polynomials Expressed in terms of orthogonal polynomials */
/* *       David Day and Louis Romero 2005 */
/* * */
/* *       Multiplying by a factor of sqrt(2*N+1) because using orthonormal, */
/* *       rather than orthogonal polynomials */
/* *************************************************************\/ */
/* double *  */
/* chebyshev_expansion_real_roots(struct OrthPolyExpansion * p, size_t * nkeep) */
/* { */
/*     /\* fprintf(stderr, "Chebyshev real_roots not finished yet\n"); *\/ */
/*     /\* exit(1); *\/ */
/*     double * real_roots = NULL; // output */
/*     *nkeep = 0; */

/*     double m = (p->upper_bound - p->lower_bound) /  (p->p->upper - p->p->lower); */
/*     double off = p->upper_bound - m * p->p->upper; */


/*     /\* printf("coeff pre truncate = "); dprint(p->num_, p->coeff); *\/ */
/*     /\* for (size_t ii = 0; ii < p->num_poly; ii++){ *\/ */
/*     /\*     if (fabs(p->coeff[ii]) < 1e-13){ *\/ */
/*     /\*         p->coeff[ii] = 0.0; *\/ */
/*     /\*     } *\/ */
/*     /\* } *\/ */
/*     orth_poly_expansion_round(&p); */
    
/*     size_t N = p->num_poly-1; */
/*     if (N == 0){ */
/*         return real_roots; */
/*     } */
/*     else if (N == 1){ */
/*         if (fabs(p->coeff[N]) <= ZEROTHRESH){ */
/*             return real_roots; */
/*         } */
/*         else{ */
/*             double root = -p->coeff[0] / p->coeff[1]; */
/*             if ( (root >= -1.0-ZEROTHRESH) && (root <= 1.0 - ZEROTHRESH)){ */
/*                 if (root <-1.0){ */
/*                     root = -1.0; */
/*                 } */
/*                 else if (root > 1.0){ */
/*                     root = 1.0; */
/*                 } */
/*                 *nkeep = 1; */
/*                 real_roots = calloc_double(1); */
/*                 real_roots[0] = m*root+off; */
/*             } */
/*         } */
/*     } */
/*     else{ */
/*         /\* printf("I am heare\n"); *\/ */
/*         /\* dprint(N+1, p->coeff); *\/ */
/*         double * nscompanion = calloc_double(N*N); // nonstandard companion */
/*         size_t ii; */

/*         double hnn1 = 0.5; */
/*         double gamma = p->coeff[N]; */
        
/*         nscompanion[1] = 1.0; */
/*         nscompanion[(N-1)*N] -= hnn1*p->coeff[0] / gamma; */
/*         for (ii = 1; ii < N-1; ii++){ */
/*             assert (fabs(p->p->bn(ii)) < 1e-14); */
            
/*             nscompanion[ii*N+ii-1] = 0.5; // ii-th column */
/*             nscompanion[ii*N+ii+1] = 0.5; */

/*             // update last column */
/*             nscompanion[(N-1)*N + ii] -= hnn1 * p->coeff[ii] / gamma; */
/*         } */
/*         nscompanion[N*N-2] += 0.5; */
/*         nscompanion[N*N-1] -= hnn1 * p->coeff[N-1] / gamma; */
        
/*         //printf("good up to here!\n"); */
/*         /\* dprint2d_col(N,N,nscompanion); *\/ */

/*         int info; */
/*         double * scale = calloc_double(N); */
/*         //\* */
/*         //Balance */
/*         int ILO, IHI; */
/*         //printf("am I here? N=%zu \n",N); */
/*         //dprint(N*N,nscompanion); */
/*         dgebal_("S", (int*)&N, nscompanion, (int *)&N,&ILO,&IHI,scale,&info); */
/*         //printf("yep\n"); */
/*         if (info < 0){ */
/*             fprintf(stderr, "Calling dgebl had error in %d-th input in the chebyshev_expansion_real_roots function\n",info); */
/*             exit(1); */
/*         } */

/*         //printf("balanced!\n"); */
/*         //dprint2d_col(N,N,nscompanion); */

/*         //IHI = M1; */
/*         //printf("M1=%zu\n",M1); */
/*         //printf("ilo=%zu\n",ILO); */
/*         //printf("IHI=%zu\n",IHI); */
/*         //\*\/ */

/*         double * real = calloc_double(N); */
/*         double * img = calloc_double(N); */
/*         //printf("allocated eigs N = %zu\n",N); */
/*         int lwork = 8 * (int)N; */
/*         //printf("got lwork\n"); */
/*         double * iwork = calloc_double(8*N); */
/*         //printf("go here"); */

/*         //dgeev_("N","N", &N, nscompanion, &N, real, img, NULL, &N, */
/*         //        NULL, &N, iwork, &lwork, &info); */
/*         dhseqr_("E","N",(int*)&N,&ILO,&IHI,nscompanion,(int*)&N,real,img,NULL,(int*)&N,iwork,&lwork,&info); */
/*         //printf("done here"); */

/*         if (info < 0){ */
/*             fprintf(stderr, "Calling dhesqr had error in %d-th input in the legendre_expansion_real_roots function\n",info); */
/*             exit(1); */
/*         } */
/*         else if(info > 0){ */
/*             //fprintf(stderr, "Eigenvalues are still uncovered in legendre_expansion_real_roots function\n"); */
/*            // printf("coeffs are \n"); */
/*            // dprint(p->num_poly, p->coeff); */
/*            // printf("last 2 = %G\n",p->coeff[p->num_poly-1]); */
/*            // exit(1); */
/*         } */

/*        /\* printf("eigenvalues \n"); *\/ */
/*         size_t * keep = calloc_size_t(N); */
/*         for (ii = 0; ii < N; ii++){ */
/*             /\* printf("(%3.15G, %3.15G)\n",real[ii],img[ii]); *\/ */
/*             if ((fabs(img[ii]) < 1e-6) && (real[ii] > -1.0-1e-12) && (real[ii] < 1.0+1e-12)){ */
/*             /\* if ((real[ii] > -1.0-1e-12) && (real[ii] < 1.0+1e-12)){                 *\/ */
/*                 if (real[ii] < -1.0){ */
/*                     real[ii] = -1.0; */
/*                 } */
/*                 else if (real[ii] > 1.0){ */
/*                     real[ii] = 1.0; */
/*                 } */
/*                 keep[ii] = 1; */
/*                 *nkeep = *nkeep + 1; */
/*             } */
/*         } */

/*         /\* printf("nkeep = %zu\n", *nkeep); *\/ */
        
/*         if (*nkeep > 0){ */
/*             real_roots = calloc_double(*nkeep); */
/*             size_t counter = 0; */
/*             for (ii = 0; ii < N; ii++){ */
/*                 if (keep[ii] == 1){ */
/*                     real_roots[counter] = real[ii]*m+off; */
/*                     counter++; */
/*                 } */
/*             } */
/*         } */
     

/*         free(keep); keep = NULL; */
/*         free(iwork); iwork  = NULL; */
/*         free(real); real = NULL; */
/*         free(img); img = NULL; */
/*         free(nscompanion); nscompanion = NULL; */
/*         free(scale); scale = NULL; */
/*     } */

/*     if (*nkeep > 1){ */
/*         qsort(real_roots, *nkeep, sizeof(double), dblcompare); */
/*     } */
/*     return real_roots; */
/* } */

/* /\********************************************************\//\** */
/* *   Obtain the real roots of a orthogonal polynomial expansion */
/* * */
/* *   \param[in] p     - orthogonal polynomial expansion */
/* *   \param[in] nkeep - returns how many real roots tehre are */
/* * */
/* *   \return real_roots - real roots of an orthonormal polynomial expansion */
/* * */
/* *   \note */
/* *       Only roots within the bounds are returned */
/* *************************************************************\/ */
/* double * */
/* orth_poly_expansion_real_roots(struct OrthPolyExpansion * p, size_t * nkeep) */
/* { */
/*     double * real_roots = NULL; */
/*     enum poly_type ptype = p->p->ptype; */
/*     switch(ptype){ */
/*     case LEGENDRE: */
/*         real_roots = legendre_expansion_real_roots(p,nkeep);    */
/*         break; */
/*     case STANDARD:         */
/*         assert (1 == 0); */
/*         //x need to convert polynomial to standard polynomial first */
/*         //real_roots = standard_poly_real_roots(sp,nkeep); */
/*         //break; */
/*     case CHEBYSHEV: */
/*         real_roots = chebyshev_expansion_real_roots(p,nkeep); */
/*         break; */
/*     case HERMITE: */
/*         assert (1 == 0); */
/*     } */
/*     return real_roots; */
/* } */

/* /\********************************************************\//\** */
/* *   Obtain the maximum of an orthogonal polynomial expansion */
/* * */
/* *   \param[in] p - orthogonal polynomial expansion */
/* *   \param[in] x - location of maximum value */
/* * */
/* *   \return maxval - maximum value */
/* *    */
/* *   \note */
/* *       if constant function then just returns the left most point */
/* *************************************************************\/ */
/* double orth_poly_expansion_max(struct OrthPolyExpansion * p, double * x) */
/* { */
    
/*     double maxval; */
/*     double tempval; */

/*     maxval = orth_poly_expansion_eval(p,p->lower_bound); */
/*     *x = p->lower_bound; */

/*     tempval = orth_poly_expansion_eval(p,p->upper_bound); */
/*     if (tempval > maxval){ */
/*         maxval = tempval; */
/*         *x = p->upper_bound; */
/*     } */
    
/*     if (p->num_poly > 2){ */
/*         size_t nroots; */
/*         struct OrthPolyExpansion * deriv = orth_poly_expansion_deriv(p); */
/*         double * roots = orth_poly_expansion_real_roots(deriv,&nroots); */
/*         if (nroots > 0){ */
/*             size_t ii; */
/*             for (ii = 0; ii < nroots; ii++){ */
/*                 tempval = orth_poly_expansion_eval(p, roots[ii]); */
/*                 if (tempval > maxval){ */
/*                     *x = roots[ii]; */
/*                     maxval = tempval; */
/*                 } */
/*             } */
/*         } */

/*         free(roots); roots = NULL; */
/*         orth_poly_expansion_free(deriv); deriv = NULL; */
/*     } */
/*     return maxval; */
/* } */

/* /\********************************************************\//\** */
/* *   Obtain the minimum of an orthogonal polynomial expansion */
/* * */
/* *   \param[in]     p - orthogonal polynomial expansion */
/* *   \param[in,out] x - location of minimum value */
/* * */
/* *   \return minval - minimum value */
/* *************************************************************\/ */
/* double orth_poly_expansion_min(struct OrthPolyExpansion * p, double * x) */
/* { */

/*     double minval; */
/*     double tempval; */

/*     minval = orth_poly_expansion_eval(p,p->lower_bound); */
/*     *x = p->lower_bound; */

/*     tempval = orth_poly_expansion_eval(p,p->upper_bound); */
/*     if (tempval < minval){ */
/*         minval = tempval; */
/*         *x = p->upper_bound; */
/*     } */
    
/*     if (p->num_poly > 2){ */
/*         size_t nroots; */
/*         struct OrthPolyExpansion * deriv = orth_poly_expansion_deriv(p); */
/*         double * roots = orth_poly_expansion_real_roots(deriv,&nroots); */
/*         if (nroots > 0){ */
/*             size_t ii; */
/*             for (ii = 0; ii < nroots; ii++){ */
/*                 tempval = orth_poly_expansion_eval(p, roots[ii]); */
/*                 if (tempval < minval){ */
/*                     *x = roots[ii]; */
/*                     minval = tempval; */
/*                 } */
/*             } */
/*         } */
/*         free(roots); roots = NULL; */
/*         orth_poly_expansion_free(deriv); deriv = NULL; */
/*     } */
/*     return minval; */
/* } */

/* /\********************************************************\//\** */
/* *   Obtain the maximum in absolute value of an orthogonal polynomial expansion */
/* * */
/* *   \param[in]     p     - orthogonal polynomial expansion */
/* *   \param[in,out] x     - location of maximum */
/* *   \param[in]     oargs - optimization arguments  */
/* *                          required for HERMITE otherwise can set NULL */
/* * */
/* *   \return maxval : maximum value (absolute value) */
/* * */
/* *   \note */
/* *       if no roots then either lower or upper bound */
/* *************************************************************\/ */
/* double orth_poly_expansion_absmax( */
/*     struct OrthPolyExpansion * p, double * x, void * oargs) */
/* { */

/*     //printf("in absmax\n"); */
/*    // print_orth_poly_expansion(p,3,NULL); */
/*     //printf("%G\n", orth_poly_expansion_norm(p)); */

/*     enum poly_type ptype = p->p->ptype; */
/*     if (oargs != NULL){ */

/*         struct c3Vector * optnodes = oargs; */
/*         double mval = fabs(orth_poly_expansion_eval(p,optnodes->elem[0])); */
/*         *x = optnodes->elem[0]; */
/*         double cval = mval; */
/*         if (ptype == HERMITE){ */
/*             mval *= exp(-pow(optnodes->elem[0],2)/2.0); */
/*         } */
/*         *x = optnodes->elem[0]; */
/*         for (size_t ii = 0; ii < optnodes->size; ii++){ */
/*             double val = fabs(orth_poly_expansion_eval(p,optnodes->elem[ii])); */
/*             double tval = val; */
/*             if (ptype == HERMITE){ */
/*                 val *= exp(-pow(optnodes->elem[ii],2)/2.0); */
/*                 //printf("ii=%zu, x = %G. val=%G, tval=%G\n",ii,optnodes->elem[ii],val,tval); */
/*             } */
/*             if (val > mval){ */
/* //                printf("min achieved\n"); */
/*                 mval = val; */
/*                 cval = tval; */
/*                 *x = optnodes->elem[ii]; */
/*             } */
/*         } */
/* //        printf("optloc=%G .... cval = %G\n",*x,cval); */
/*         return cval; */
/*     } */
/*     else if (ptype == HERMITE){ */
/*         fprintf(stderr,"Must specify optimizatino arguments\n"); */
/*         fprintf(stderr,"In the form of candidate points for \n"); */
/*         fprintf(stderr,"finding the absmax of hermite expansion\n"); */
/*         exit(1); */
        
/*     } */
/*     double maxval; */
/*     double norm = orth_poly_expansion_norm(p); */
    
/*     if (norm < ZEROTHRESH) { */
/*         *x = p->lower_bound; */
/*         maxval = 0.0; */
/*     } */
/*     else{ */
/*         //printf("nroots=%zu\n", nroots); */
/*         double tempval; */

/*         maxval = fabs(orth_poly_expansion_eval(p,p->lower_bound)); */
/*         *x = p->lower_bound; */

/*         tempval = fabs(orth_poly_expansion_eval(p,p->upper_bound)); */
/*         if (tempval > maxval){ */
/*             maxval = tempval; */
/*             *x = p->upper_bound; */
/*         } */
/*         if (p->num_poly > 2){ */
/*             size_t nroots; */
/*             struct OrthPolyExpansion * deriv = orth_poly_expansion_deriv(p); */
/*             double * roots = orth_poly_expansion_real_roots(deriv,&nroots); */
/*             if (nroots > 0){ */
/*                 size_t ii; */
/*                 for (ii = 0; ii < nroots; ii++){ */
/*                     tempval = fabs(orth_poly_expansion_eval(p, roots[ii])); */
/*                     if (tempval > maxval){ */
/*                         *x = roots[ii]; */
/*                         maxval = tempval; */
/*                     } */
/*                 } */
/*             } */

/*             free(roots); roots = NULL; */
/*             orth_poly_expansion_free(deriv); deriv = NULL; */
/*         } */
/*     } */
/*     //printf("done\n"); */
/*     return maxval; */
/* } */


