// Copyright (c) 2015-2016, Massachusetts Institute of Technology
// Copyright (c) 2016-2017 Sandia Corporation

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







#ifndef LINALG_H
#define LINALG_H

#ifdef __APPLE__
    #include <Accelerate/Accelerate.h>
    #include "/System/Library/Frameworks/Accelerate.framework/Versions/Current/Frameworks/vecLib.framework/Headers/clapack.h"

    #define dgetri_(X, Y, Z, A , B, C, D ) \
            ( dgetri_( (__CLPK_integer *) X, Y, (__CLPK_integer *) Z, (__CLPK_integer *) A, \
                        B, (__CLPK_integer *)C , (__CLPK_integer *) D) )
    #define dgetrf_(X, Y, Z, A ,B, C ) \
            ( dgetrf_( (__CLPK_integer *) X,(__CLPK_integer *) Y, Z, (__CLPK_integer *) A, \
                       (__CLPK_integer *) B, (__CLPK_integer *)C ))
    #define dorgqr_(X,Y,Z,A,B,C,D,E,F) \
            ( dorgqr_( (__CLPK_integer *) X, (__CLPK_integer *) Y, (__CLPK_integer *) Z, A, \
                       (__CLPK_integer *) B, C , D, (__CLPK_integer *) E, (__CLPK_integer *)F) )
    #define dgeqrf_(X, Y, Z, A , B, C, D, E ) \
            ( dgeqrf_( (__CLPK_integer *) X,  (__CLPK_integer *)Y, Z, (__CLPK_integer *) A, \
                        B, C, (__CLPK_integer *) D, (__CLPK_integer *) E) )

    #define dorgrq_(X,Y,Z,A,B,C,D,E,F) \
            ( dorgrq_( (__CLPK_integer *) X, (__CLPK_integer *) Y, (__CLPK_integer *) Z, A, \
                       (__CLPK_integer *) B, C , D, (__CLPK_integer *) E, (__CLPK_integer *)F) )
    #define dgerqf_(X, Y, Z, A , B, C, D, E ) \
            ( dgerqf_( (__CLPK_integer *) X,  (__CLPK_integer *)Y, Z, (__CLPK_integer *) A, \
                        B, C, (__CLPK_integer *) D, (__CLPK_integer *) E) )

    #define dgesdd_(X,Y,Z,A,B,C,D,E,F,G,H,I,J,K) \
            ( dgesdd_(X, (__CLPK_integer *)Y, (__CLPK_integer *)Z, A, (__CLPK_integer *) B,\
                C,D,(__CLPK_integer *) E, F, (__CLPK_integer *) G, H, (__CLPK_integer *) I,\
                (__CLPK_integer *) J, (__CLPK_integer *) K ) )

    #define dgebal_(X, Y, Z, A , B, C, D, E ) \
            ( dgebal_( X,  (__CLPK_integer *)Y, Z, (__CLPK_integer *) A, \
                   (__CLPK_integer *) B, (__CLPK_integer *)C, D, (__CLPK_integer *) E) )

    #define dpotrf_(X,Y,Z,A,B) \
            ( dpotrf_(X, (__CLPK_integer *)Y, Z, (__CLPK_integer *) A, (__CLPK_integer *) B ))

    #define dpotri_(X,Y,Z,A,B) \
            ( dpotri_(X, (__CLPK_integer *)Y, Z, (__CLPK_integer *) A, (__CLPK_integer *) B ))

    #define dtrtri_(X,Y,Z,A,B,C) \
            ( dtrtri_(X,Y, (__CLPK_integer *)Z, A, (__CLPK_integer *) B, (__CLPK_integer *) C ))

    #define dgesv_(X,Y,Z,A,B,C,D,E) \
            ( dgesv_((__CLPK_integer *)X, (__CLPK_integer *)Y, Z, (__CLPK_integer *) A, (__CLPK_integer *) B, \
                C, (__CLPK_integer *) D, (__CLPK_integer *) E))

    #define dhseqr_(X, Y, Z, A , B, C, D, E,F,G,H,I,J,K ) \
            ( dhseqr_( X, Y, (__CLPK_integer *)Z, (__CLPK_integer *)A, \
                   (__CLPK_integer *) B, C, (__CLPK_integer *)D, E, F, G, (__CLPK_integer *) H, \
                   I, (__CLPK_integer *) J, (__CLPK_integer *)K ) )

    #define dsyev_(A,B,C,D,E,F,G,H,J) \
        ( dsyev_(A,B,(__CLPK_integer *) C,D,(__CLPK_integer *) E,F, \
            G, (__CLPK_integer *) H, (__CLPK_integer *) J) )

    #define dgeev_(X, Y, Z, A , B, C, D, E,F,G,H,I,J,K ) \
            ( dgeev_( X, Y, (__CLPK_integer *)Z, A, \
                   (__CLPK_integer *) B, C, D, E, (__CLPK_integer *) F, G, (__CLPK_integer *) H, \
                   I, (__CLPK_integer *) J, (__CLPK_integer *)K ) )

    #define dstegr_(X,Y,Z,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P,Q) \
            ( dstegr_(X,Y,(__CLPK_integer *)Z,A,B,C,D,\
                   (__CLPK_integer *)E,(__CLPK_integer *)F,G,(__CLPK_integer *)H, \
                   I,J,(__CLPK_integer *)K,(__CLPK_integer *)L,M,(__CLPK_integer *)N, \
                   (__CLPK_integer *)O,(__CLPK_integer *)P,(__CLPK_integer *)Q))

    #define dgelsd_(X,Y,Z,A,B,C,D,E,F,G,H,I,J,K)                         \
            ( dgelsd_( (__CLPK_integer *)X, (__CLPK_integer *)Y, (__CLPK_integer *)Z, \
                       (__CLPK_doublereal *)A, (__CLPK_integer *)B, (__CLPK_doublereal *)C,  \
                       (__CLPK_integer *)D, (__CLPK_doublereal *) E, \
	                   (__CLPK_doublereal *)F, (__CLPK_integer *)G, (__CLPK_doublereal *)H, \
                       (__CLPK_integer *)I, (__CLPK_integer *)J, (__CLPK_integer *)K))

                   
#else
    /* #include <gsl/gsl_cblas.h> */
    #include <cblas.h>
    #include "f2c.h" // this is worrysome because it redfines ints and such 
    #include "clapack.h"

    #define dgetri_(X, Y, Z, A , B, C, D ) \
            ( dgetri_( (integer *) X, Y, (integer *) Z, (integer *) A, \
                        B, (integer *)C , (integer *) D) )
    #define dgetrf_(X, Y, Z, A ,B, C ) \
            ( dgetrf_( (integer *) X,(integer *) Y, Z, (integer *) A, \
                       (integer *) B, (integer *)C ))
    #define dorgqr_(X,Y,Z,A,B,C,D,E,F) \
            ( dorgqr_( (integer *) X, (integer *) Y, (integer *) Z, A, \
                       (integer *) B, C , D, (integer *) E, (integer *)F) )
    #define dgeqrf_(X, Y, Z, A , B, C, D, E ) \
            ( dgeqrf_( (integer *) X,  (integer *)Y, Z, (integer *) A, \
                        B, C, (integer *) D, (integer *) E) )

    #define dorgrq_(X,Y,Z,A,B,C,D,E,F) \
            ( dorgrq_( (integer *) X, (integer *) Y, (integer *) Z, A, \
                       (integer *) B, C , D, (integer *) E, (integer *)F) )
    #define dgerqf_(X, Y, Z, A , B, C, D, E ) \
            ( dgerqf_( (integer *) X,  (integer *)Y, Z, (integer *) A, \
                        B, C, (integer *) D, (integer *) E) )

    #define dgesdd_(X,Y,Z,A,B,C,D,E,F,G,H,I,J,K) \
            ( dgesdd_(X, (integer *)Y, (integer *)Z, A, (integer *) B,\
                C,D,(integer *) E, F, (integer *) G, H, (integer *) I,\
                (integer *) J, (integer *) K ) )

    #define dstev_(X, Y, Z, A , B, C, D, E ) \
            ( dstev_( X,  (integer *)Y, Z, A, \
                        B, (integer *)C, D, (integer *) E) )

    #define dgeev_(X, Y, Z, A , B, C, D, E,F,G,H,I,J,K ) \
            ( dgeev_( X, Y, (integer *)Z, A, \
                   (integer *) B, C, D, E, (integer *) F, G, (integer *) H, \
                   I, (integer *) J, (integer *)K ) )

    #define dsyev_(A,B,C,D,E,F,G,H,J) \
        ( dsyev_(A,B,(integer *) C,D,(integer *) E,F, \
            G, (integer *) H, (integer *) J) )

    #define dhseqr_(X, Y, Z, A , B, C, D, E,F,G,H,I,J,K ) \
            ( dhseqr_( X, Y, (integer *)Z, (integer *)A, \
                   (integer *) B, C, (integer *)D, E, F, G, (integer *) H, \
                   I, (integer *) J, (integer *)K ) )

    #define dgebal_(X, Y, Z, A , B, C, D, E ) \
            ( dgebal_( X,  (integer *)Y, Z, (integer *) A, \
                       (integer *) B, (integer *)C, D, (integer *) E) )

    #define dpotrf_(X,Y,Z,A,B) \
            ( dpotrf_(X, (integer *)Y, Z, (integer *) A, (integer *) B ))

    #define dpotri_(X,Y,Z,A,B) \
            ( dpotri_(X, (integer *)Y, Z, (integer *) A, (integer *) B ))

    #define dtrtri_(X,Y,Z,A,B,C) \
            ( dtrtri_(X,Y, (integer *)Z, A, (integer *) B, (integer *) C ))

    #define dgesv_(X,Y,Z,A,B,C,D,E) \
            ( dgesv_((integer *)X, (integer *)Y, Z, (integer *) A, (integer *) B, \
                C, (integer *) D, (integer *) E))

    #define dstegr_(X,Y,Z,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P,Q) \
            ( dstegr_(X,Y,(integer *)Z,A,B,C,D,\
                   (integer *)E,(integer *)F,G,(integer *)H, \
                   I,J,(integer *)K,(integer *)L,M,(integer *)N, \
                   (integer *)O,(integer *)P,(integer *)Q))

    #define dgels_(X,Y,Z,A,B,C,D,E,F,G,H)                                   \
            ( dgels_(X,(integer * )Y,(integer *)Z,(integer *)A,B,(integer *)C, \
                       D, (integer *) E,F,(integer *)G,(integer *)H))

    #define dgelsd_(X,Y,Z,A,B,C,D,E,F,G,H,I,J,K)                         \
            ( dgelsd_( (integer *)X, (integer *)Y, (integer *)Z, \
                       (doublereal *)A, (integer *)B, (doublereal *)C,  \
                       (integer *)D, (doublereal *) E,                  \
	                   (doublereal *)F, (integer *)G, (doublereal *)H, (integer *)I, \
                           (integer *)J, (integer *)K))


#endif

#include "matrix_util.h"

void c3linalg_multiple_vec_mat(size_t, size_t, size_t, const double *, size_t,
                               const double *, size_t, double *,size_t);
void c3linalg_multiple_mat_vec(size_t, size_t, size_t, const double *, size_t,
                               const double *, size_t, double *,size_t);
int qr(size_t, size_t, double *, size_t);
void rq_with_rmult(size_t, size_t, double *, size_t, size_t, size_t, double *, size_t);
void svd(size_t, size_t, size_t, double *, double *, double *, double *);
size_t truncated_svd(size_t, size_t, size_t, double *, double **, double **, double **, double);
size_t pinv(size_t, size_t, size_t, double *, double *, double);

double norm2(double *, int);
double norm2diff(double *, double *, int);
double mean(double *, size_t);
double mean_size_t(size_t *, size_t);

struct mat * kron(const struct mat *, const struct mat *);
void kron_col(int, int, double *, int, int, int, double *, int, double *, int);
void vec_kron(size_t, size_t, double *, size_t, size_t, size_t, 
        double *, size_t, double *, double, double *);
void vec_kronl(size_t, size_t, double *, size_t, size_t, size_t, 
        double *, size_t, long double *, double, long double *);

// decompositions
struct fiber_list{
    size_t index;
    double * vals;
    struct fiber_list * next;
};
struct fiber_info{
    size_t nfibers;
    struct fiber_list * head;
};
void AddFiber(struct fiber_list **, size_t, double *, size_t);
int IndexExists(struct fiber_list *, size_t);
double * getIndex(struct fiber_list *, size_t);
void DeleteFiberList(struct fiber_list **);

struct sk_decomp {
    size_t n;
    size_t m;
    size_t rank;
    size_t * rows_kept;
    size_t * cols_kept;
    size_t cross_rank;
    double * cross_inv;
    struct fiber_info * row_vals;
    struct fiber_info * col_vals;
    int success;
};

void init_skf(struct sk_decomp **, size_t, size_t, size_t);
void sk_decomp_to_full(struct sk_decomp *, double *);
void free_skf(struct sk_decomp **);

int comp_pivots(const double *, int, int, int *);
int maxvol_rhs(const double *, size_t, size_t, size_t *, double *); //
int skeleton(double *, size_t, size_t, size_t, size_t *, size_t *, double);
int skeleton_func(double (*A)(int,int, int, void*), void *, size_t, 
                size_t, size_t, size_t *, size_t *, double);
int
skeleton_func2(int (*Ap)(double *, double, size_t, size_t, double *,
                void *),
                   void *, struct sk_decomp **,  double *, double *, 
                   double);
void linear_ls(size_t, size_t, double *, double *, double *);
#endif
