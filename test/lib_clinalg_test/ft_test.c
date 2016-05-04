// Copyright (c) 2014-2016, Massachusetts Institute of Technology
//
// This file is part of the Compressed Continuous Computation (C3) toolbox
// Author: Alex A. Gorodetsky 
// Contact: goroda@mit.edu

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

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>
#include <time.h>

#include "array.h"

#include "CuTest.h"
#include "testfunctions.h"

#include "lib_funcs.h"
#include "lib_linalg.h"

#include "lib_clinalg.h"

static void all_opts_free(
    struct Fwrap * fw,
    struct OpeOpts * opts,
    struct OneApproxOpts * qmopts,
    struct MultiApproxOpts * fopts)
{
    fwrap_destroy(fw);
    ope_opts_free(opts);
    one_approx_opts_free(qmopts);
    multi_approx_opts_free(fopts);
}


void Test_function_train_initsum(CuTest * tc){

    printf("Testing function: function_train_initsum \n");

    // functions
    struct Fwrap * fw = fwrap_create(1,"array-vec");
    fwrap_set_num_funcs(fw,4);
    fwrap_set_func_array(fw,0,func,NULL);
    fwrap_set_func_array(fw,1,func2,NULL);
    fwrap_set_func_array(fw,2,func3,NULL);
    fwrap_set_func_array(fw,3,func4,NULL);

    struct OpeOpts * opts = ope_opts_alloc(LEGENDRE);
    struct OneApproxOpts * qmopts = one_approx_opts_alloc(POLYNOMIAL,opts);
    struct MultiApproxOpts * fopts = multi_approx_opts_alloc(4);
    multi_approx_opts_set_all_same(fopts,qmopts);
   
    struct FunctionTrain * ft = function_train_initsum(fopts,fw);
    size_t * ranks = function_train_get_ranks(ft);
    for (size_t ii = 1; ii < 4; ii++ ){
        CuAssertIntEquals(tc,2,ranks[ii]);
    }

    double pt[4];
    double val, tval; 
    
    size_t N = 20;
    double * xtest = linspace(-1.0,1.0,N);
    double err = 0.0;
    double den = 0.0;

    size_t ii,jj,kk,ll;
    for (ii = 0; ii < N; ii++){
        for (jj = 0; jj < N; jj++){
            for (kk = 0; kk < N; kk++){
                for (ll = 0; ll < N; ll++){
                    pt[0] = xtest[ii]; pt[1] = xtest[jj]; 
                    pt[2] = xtest[kk]; pt[3] = xtest[ll];
                    double tval1, tval2, tval3, tval4;
                    func(1,pt,&tval1,NULL);
                    func2(1,pt+1,&tval2,NULL);
                    func3(1,pt+2,&tval3,NULL);
                    func4(1,pt+3,&tval4,NULL);
                    tval = tval1 + tval2 + tval3 + tval4;
                    val = function_train_eval(ft,pt);
                    den += pow(tval,2.0);
                    err += pow(tval-val,2.0);
                }
            }
        }
    }
    err = err/den;
    CuAssertDblEquals(tc,0.0,err,1e-15);
    free(xtest);

    function_train_free(ft);
    all_opts_free(fw,opts,qmopts,fopts);
}   

void Test_function_train_linear(CuTest * tc)
{
    printf("Testing Function: function_train_linear \n");

    struct OpeOpts * opts = ope_opts_alloc(LEGENDRE);
    struct OneApproxOpts * qmopts = one_approx_opts_alloc(POLYNOMIAL,opts);
    struct MultiApproxOpts * fopts = multi_approx_opts_alloc(3);
    multi_approx_opts_set_all_same(fopts,qmopts);
    
    double slope[3] = {1.0, 2.0, 3.0};
    double offset[3] = {0.0, 0.0, 0.0};
    struct FunctionTrain * f =function_train_linear(slope,1,offset,1,fopts);

    double pt[3] = { -0.1, 0.4, 0.2 };
    double eval = function_train_eval(f,pt);
    CuAssertDblEquals(tc, 1.3, eval, 1e-14);
    
    pt[0] = 0.8; pt[1] = -0.2; pt[2] = 0.3;
    eval = function_train_eval(f,pt);
    CuAssertDblEquals(tc, 1.3, eval, 1e-14);

    pt[0] = -0.8; pt[1] = 1.0; pt[2] = -0.01;
    eval = function_train_eval(f,pt);
    CuAssertDblEquals(tc, 1.17, eval, 1e-14);
    
    function_train_free(f);
    all_opts_free(NULL,opts,qmopts,fopts);
}

void Test_function_train_quadratic(CuTest * tc)
{
    printf("Testing Function: function_train_quadratic (1/2)\n");
    size_t dim = 3;
    double lb = -3.12;
    double ub = 2.21;

    struct OpeOpts * opts = ope_opts_alloc(LEGENDRE);
    ope_opts_set_lb(opts,lb);
    ope_opts_set_ub(opts,ub);
    struct OneApproxOpts * qmopts = one_approx_opts_alloc(POLYNOMIAL,opts);
    struct MultiApproxOpts * fopts = multi_approx_opts_alloc(dim);
    multi_approx_opts_set_all_same(fopts,qmopts);

    double * quad = calloc_double(dim * dim);
    double * coeff = calloc_double(dim);
    size_t ii,jj,kk;
    for (ii = 0; ii < dim; ii++){
        coeff[ii] = randu();
        for (jj = 0; jj < dim; jj++){
            quad[ii*dim+jj] = randu();
        }
    }

    struct FunctionTrain * f = function_train_quadratic(quad,coeff,fopts);

    size_t N = 10;
    double * xtest = linspace(lb,ub,N);
    double * pt = calloc_double(dim);
    size_t ll, mm;
    double should, is;
    for (ii = 0; ii < N; ii++){
        for (jj = 0; jj < N;  jj++){
            for (kk = 0; kk < N; kk++){
                pt[0] = xtest[ii]; pt[1] = xtest[jj]; pt[2] = xtest[kk];
                should = 0.0;
                for (ll = 0; ll< dim; ll++){
                    for (mm = 0; mm < dim; mm++){
                        should += (pt[ll]-coeff[ll])*quad[mm*dim+ll]*(pt[mm]-coeff[mm]);
                    }
                }
                //printf("should=%G\n",should);
                is = function_train_eval(f,pt);
                //printf("is=%G\n",is);
                CuAssertDblEquals(tc,should,is,1e-12);
            }
        }
    }
    free(xtest);
    free(pt);
    free(quad);
    free(coeff);
    
    function_train_free(f);
    all_opts_free(NULL,opts,qmopts,fopts);
}

void Test_function_train_quadratic2(CuTest * tc)
{
    printf("Testing Function: function_train_quadratic (2/2)\n");
    size_t dim = 4;
    double lb = -1.32;
    double ub = 6.0;

    struct OpeOpts * opts = ope_opts_alloc(LEGENDRE);
    ope_opts_set_lb(opts,lb);
    ope_opts_set_ub(opts,ub);
    struct OneApproxOpts * qmopts = one_approx_opts_alloc(POLYNOMIAL,opts);
    struct MultiApproxOpts * fopts = multi_approx_opts_alloc(dim);
    multi_approx_opts_set_all_same(fopts,qmopts);

    double * quad = calloc_double(dim * dim);
    double * coeff = calloc_double(dim);
    size_t ii,jj,kk,zz;
    for (ii = 0; ii < dim; ii++){
        coeff[ii] = randu();
        for (jj = 0; jj < dim; jj++){
            quad[ii*dim+jj] = randu();
        }
    }

    struct FunctionTrain * f = function_train_quadratic(quad,coeff,fopts);

    size_t N = 10;
    double * xtest = linspace(lb,ub,N);
    double * pt = calloc_double(dim);
    size_t ll, mm;
    double should, is;
    for (ii = 0; ii < N; ii++){
        for (jj = 0; jj < N;  jj++){
            for (kk = 0; kk < N; kk++){
                for (zz = 0; zz < N; zz++){
                    pt[0] = xtest[ii]; pt[1] = xtest[jj]; pt[2] = xtest[kk]; pt[3] = xtest[zz];
                    should = 0.0;
                    for (ll = 0; ll< dim; ll++){
                        for (mm = 0; mm < dim; mm++){
                            should += (pt[ll]-coeff[ll])*quad[mm*dim+ll]*(pt[mm]-coeff[mm]);
                        }
                    }
                    //printf("should=%G\n",should);
                    is = function_train_eval(f,pt);
                    //printf("is=%G\n",is);
                    CuAssertDblEquals(tc,should,is,1e-12);
                }
            }
        }
    }
    
    free(xtest);
    free(pt);
    free(quad);
    free(coeff);
    function_train_free(f);
    all_opts_free(NULL,opts,qmopts,fopts);
}

void Test_function_train_sum_function_train_round(CuTest * tc)
{
    printf("Testing Function: function_train_sum and ft_round \n");

    size_t dim = 3;
    struct OpeOpts * opts = ope_opts_alloc(LEGENDRE);
    struct OneApproxOpts * qmopts = one_approx_opts_alloc(POLYNOMIAL,opts);
    struct MultiApproxOpts * fopts = multi_approx_opts_alloc(dim);
    multi_approx_opts_set_all_same(fopts,qmopts);
    
    double coeffs[3] = {1.0, 2.0, 3.0};
    double off[3] = {0.0,0.0,0.0};
    struct FunctionTrain * a = function_train_linear(coeffs,1,off,1,fopts);

    double coeffsb[3] = {1.5, -0.2, 3.310};
    double offb[3]    = {0.0, 0.0, 0.0};
    struct FunctionTrain * b = function_train_linear(coeffsb,1,offb,1,fopts);
    
    struct FunctionTrain * c = function_train_sum(a,b);
    size_t * ranks = function_train_get_ranks(c);
    CuAssertIntEquals(tc,1,ranks[0]);
    CuAssertIntEquals(tc,4,ranks[1]);
    CuAssertIntEquals(tc,4,ranks[2]);
    CuAssertIntEquals(tc,1,ranks[3]);

    double pt[3];
    double eval, evals;
    
    pt[0] = -0.1; pt[1] = 0.4; pt[2]=0.2; 
    eval = function_train_eval(c,pt);
    evals = -0.1*(1.0 + 1.5) + 0.4*(2.0-0.2) + 0.2*(3.0 + 3.31);
    CuAssertDblEquals(tc, evals, eval, 1e-14);
    
    pt[0] = 0.8; pt[1] = -0.2; pt[2] = 0.3;
    evals = 0.8*(1.0 + 1.5) - 0.2*(2.0-0.2) + 0.3*(3.0 + 3.31);
    eval = function_train_eval(c,pt);
    CuAssertDblEquals(tc, evals, eval, 1e-14);

    pt[0] = -0.8; pt[1] = 1.0; pt[2] = -0.01;
    evals = -0.8*(1.0 + 1.5) + 1.0*(2.0-0.2) - 0.01*(3.0 + 3.31);
    eval = function_train_eval(c,pt);
    CuAssertDblEquals(tc, evals, eval,1e-14);
    
    struct FunctionTrain * d = function_train_round(c, 1e-10,fopts);
    ranks = function_train_get_ranks(d);
    CuAssertIntEquals(tc,1,ranks[0]);
    CuAssertIntEquals(tc,2,ranks[1]);
    CuAssertIntEquals(tc,2,ranks[2]);
    CuAssertIntEquals(tc,1,ranks[3]);

    pt[0] = -0.1; pt[1] = 0.4; pt[2]=0.2; 
    eval = function_train_eval(d,pt);
    evals = -0.1*(1.0 + 1.5) + 0.4*(2.0-0.2) + 0.2*(3.0 + 3.31);
    CuAssertDblEquals(tc, evals, eval, 1e-14);
    
    pt[0] = 0.8; pt[1] = -0.2; pt[2] = 0.3;
    evals = 0.8*(1.0 + 1.5) - 0.2*(2.0-0.2) + 0.3*(3.0 + 3.31);
    eval = function_train_eval(d,pt);
    CuAssertDblEquals(tc, evals, eval, 1e-14);

    pt[0] = -0.8; pt[1] = 1.0; pt[2] = -0.01;
    evals = -0.8*(1.0 + 1.5) + 1.0*(2.0-0.2) - 0.01*(3.0 + 3.31);
    eval = function_train_eval(d,pt);
    CuAssertDblEquals(tc, evals, eval,1e-14);
    
    function_train_free(a);
    function_train_free(b);
    function_train_free(c);
    function_train_free(d);
    all_opts_free(NULL,opts,qmopts,fopts);
}

void Test_function_train_scale(CuTest * tc)
{
    printf("Testing Function: function_train_scale \n");
    size_t dim = 4;
    // functions
    struct Fwrap * fw = fwrap_create(1,"array-vec");
    fwrap_set_num_funcs(fw,4);
    fwrap_set_func_array(fw,0,func,NULL);
    fwrap_set_func_array(fw,1,func2,NULL);
    fwrap_set_func_array(fw,2,func3,NULL);
    fwrap_set_func_array(fw,3,func4,NULL);

    struct OpeOpts * opts = ope_opts_alloc(LEGENDRE);
    struct OneApproxOpts * qmopts = one_approx_opts_alloc(POLYNOMIAL,opts);
    struct MultiApproxOpts * fopts = multi_approx_opts_alloc(dim);
    multi_approx_opts_set_all_same(fopts,qmopts);

    struct FunctionTrain * ft = function_train_initsum(fopts,fw);

    double pt[4];
    double val, tval;
    double tval1,tval2,tval3,tval4;
    double scale = 4.0;
    function_train_scale(ft,scale);
    size_t N = 10;
    double * xtest = linspace(-1.0,1.0,N);
    double err = 0.0;
    double den = 0.0;

    size_t ii,jj,kk,ll;
    for (ii = 0; ii < N; ii++){
        for (jj = 0; jj < N; jj++){
            for (kk = 0; kk < N; kk++){
                for (ll = 0; ll < N; ll++){
                    pt[0] = xtest[ii]; pt[1] = xtest[jj]; 
                    pt[2] = xtest[kk]; pt[3] = xtest[ll];
                    func(1,pt,&tval1,NULL);
                    func2(1,pt+1,&tval2,NULL);
                    func3(1,pt+2,&tval3,NULL);
                    func4(1,pt+3,&tval4,NULL);
                    tval = tval1 + tval2 + tval3 + tval4;
                    tval = tval * scale;
                    val = function_train_eval(ft,pt);
                    den += pow(tval,2.0);
                    err += pow(tval-val,2.0);
                }
            }
        }
    }
    err = err/den;
    CuAssertDblEquals(tc,0.0,err,1e-15);
    free(xtest);
    
    all_opts_free(fw,opts,qmopts,fopts);
    function_train_free(ft);
}

void Test_function_train_product(CuTest * tc)
{
    printf("Testing Function: function_train_product \n");
    size_t dim = 4;

    // functions
    struct Fwrap * fw = fwrap_create(1,"array-vec");
    fwrap_set_num_funcs(fw,4);
    fwrap_set_func_array(fw,0,func,NULL);
    fwrap_set_func_array(fw,1,func2,NULL);
    fwrap_set_func_array(fw,2,func3,NULL);
    fwrap_set_func_array(fw,3,func4,NULL);

    struct Fwrap * fw2 = fwrap_create(1,"array-vec");
    fwrap_set_num_funcs(fw2,4);
    fwrap_set_func_array(fw2,0,func2,NULL);
    fwrap_set_func_array(fw2,1,func5,NULL);
    fwrap_set_func_array(fw2,2,func4,NULL);
    fwrap_set_func_array(fw2,3,func6,NULL);

    struct OpeOpts * opts = ope_opts_alloc(LEGENDRE);
    struct OneApproxOpts * qmopts = one_approx_opts_alloc(POLYNOMIAL,opts);
    struct MultiApproxOpts * fopts = multi_approx_opts_alloc(dim);
    multi_approx_opts_set_all_same(fopts,qmopts);

    
    struct FunctionTrain * ft = function_train_initsum(fopts,fw);
    struct FunctionTrain * gt = function_train_initsum(fopts,fw2);
    struct FunctionTrain * ft2 =  function_train_product(ft,gt);

    double pt[4];
    double val, tval1,tval2; 
    size_t N = 10;
    double * xtest = linspace(-1.0,1.0,N);
    double err = 0.0;
    double den = 0.0;

    size_t ii,jj,kk,ll;
    for (ii = 0; ii < N; ii++){
        for (jj = 0; jj < N; jj++){
            for (kk = 0; kk < N; kk++){
                for (ll = 0; ll < N; ll++){
                    pt[0] = xtest[ii]; pt[1] = xtest[jj]; 
                    pt[2] = xtest[kk]; pt[3] = xtest[ll];
                    tval1 =  function_train_eval(ft,pt);
                    tval2 =  function_train_eval(gt,pt);
                    val = function_train_eval(ft2,pt);
                    den += pow(tval1*tval2,2.0);
                    err += pow(tval1*tval2-val,2.0);
                }
            }
        }
    }
    err = err/den;

    CuAssertDblEquals(tc,0.0,err,1e-15);
    
    free(xtest);
    function_train_free(ft);
    function_train_free(gt);
    function_train_free(ft2);
    fwrap_destroy(fw2);
    all_opts_free(fw,opts,qmopts,fopts);
}


void Test_function_train_integrate(CuTest * tc)
{
    printf("Testing Function: function_train_integrate \n");
    size_t dim = 4;

    // functions
    struct Fwrap * fw = fwrap_create(1,"array-vec");
    fwrap_set_num_funcs(fw,4);
    fwrap_set_func_array(fw,0,func,NULL);
    fwrap_set_func_array(fw,1,func2,NULL);
    fwrap_set_func_array(fw,2,func3,NULL);
    fwrap_set_func_array(fw,3,func4,NULL);

    struct OpeOpts * opts = ope_opts_alloc(LEGENDRE);
    ope_opts_set_lb(opts,0.0);
    struct OpeOpts * opts2 = ope_opts_alloc(LEGENDRE);
    ope_opts_set_lb(opts2,-1.0);
    struct OpeOpts * opts3 = ope_opts_alloc(LEGENDRE);
    ope_opts_set_lb(opts3,-5.0);
    struct OpeOpts * opts4 = ope_opts_alloc(LEGENDRE);
    ope_opts_set_lb(opts4,-5.0);
    struct OneApproxOpts * qmopts = one_approx_opts_alloc(POLYNOMIAL,opts);
    struct OneApproxOpts * qmopts2 = one_approx_opts_alloc(POLYNOMIAL,opts2);
    struct OneApproxOpts * qmopts3 = one_approx_opts_alloc(POLYNOMIAL,opts3);
    struct OneApproxOpts * qmopts4 = one_approx_opts_alloc(POLYNOMIAL,opts4);

    struct MultiApproxOpts * fopts = multi_approx_opts_alloc(dim);
    multi_approx_opts_set_dim(fopts,0,qmopts);
    multi_approx_opts_set_dim(fopts,1,qmopts2);
    multi_approx_opts_set_dim(fopts,2,qmopts3);
    multi_approx_opts_set_dim(fopts,3,qmopts4);

    struct FunctionTrain * ft = function_train_initsum(fopts,fw);
    double out =  function_train_integrate(ft);
    
    double shouldbe = 110376.0/5.0;
    double rel_error = pow(out-shouldbe,2)/fabs(shouldbe);
    CuAssertDblEquals(tc, 0.0 ,rel_error,1e-15);

    all_opts_free(fw,opts,qmopts,fopts);
    all_opts_free(NULL,opts2,qmopts2,NULL);
    all_opts_free(NULL,opts3,qmopts3,NULL);
    all_opts_free(NULL,opts4,qmopts4,NULL);
    function_train_free(ft);
}

void Test_function_train_inner(CuTest * tc)
{
    printf("Testing Function: function_train_inner \n");
    size_t dim = 4;

    // functions
    struct Fwrap * fw = fwrap_create(1,"array-vec");
    fwrap_set_num_funcs(fw,4);
    fwrap_set_func_array(fw,0,func,NULL);
    fwrap_set_func_array(fw,1,func2,NULL);
    fwrap_set_func_array(fw,2,func3,NULL);
    fwrap_set_func_array(fw,3,func4,NULL);

    struct Fwrap * fw2 = fwrap_create(1,"array-vec");
    fwrap_set_num_funcs(fw2,4);
    fwrap_set_func_array(fw2,0,func6,NULL);
    fwrap_set_func_array(fw2,1,func5,NULL);
    fwrap_set_func_array(fw2,2,func4,NULL);
    fwrap_set_func_array(fw2,3,func3,NULL);

    struct OpeOpts * opts = ope_opts_alloc(LEGENDRE);
    struct OneApproxOpts * qmopts = one_approx_opts_alloc(POLYNOMIAL,opts);
    struct MultiApproxOpts * fopts = multi_approx_opts_alloc(dim);
    multi_approx_opts_set_all_same(fopts,qmopts);

    struct FunctionTrain * ft = function_train_initsum(fopts,fw);
    struct FunctionTrain * gt = function_train_initsum(fopts,fw2);
    struct FunctionTrain * ft2 =  function_train_product(gt,ft);
    
    double int1 = function_train_integrate(ft2);
    double int2 = function_train_inner(gt,ft);
    
    double relerr = pow(int1-int2,2)/pow(int1,2);
    CuAssertDblEquals(tc,0.0,relerr,1e-13);
    
    function_train_free(ft);
    function_train_free(ft2);
    function_train_free(gt);
    fwrap_destroy(fw2);
    all_opts_free(fw,opts,qmopts,fopts);
}

CuSuite * CLinalgFuncTrainGetSuite(){

    CuSuite * suite = CuSuiteNew();
    SUITE_ADD_TEST(suite, Test_function_train_initsum);
    SUITE_ADD_TEST(suite, Test_function_train_linear);
    SUITE_ADD_TEST(suite, Test_function_train_quadratic);
    SUITE_ADD_TEST(suite, Test_function_train_quadratic2);
    SUITE_ADD_TEST(suite, Test_function_train_sum_function_train_round);
    SUITE_ADD_TEST(suite, Test_function_train_scale);
    SUITE_ADD_TEST(suite, Test_function_train_product);
    SUITE_ADD_TEST(suite, Test_function_train_integrate);
    SUITE_ADD_TEST(suite, Test_function_train_inner);
    
    /* SUITE_ADD_TEST(suite, Test_ftapprox_cross); */
    /* SUITE_ADD_TEST(suite, Test_ftapprox_cross2); */
    /* SUITE_ADD_TEST(suite, Test_ftapprox_cross3); */
    /* SUITE_ADD_TEST(suite, Test_ftapprox_cross4); */
    /* SUITE_ADD_TEST(suite, Test_function_train_eval_co_peruturb); */
    /* SUITE_ADD_TEST(suite, Test_ftapprox_cross_hermite1); */
    /* SUITE_ADD_TEST(suite, Test_ftapprox_cross_hermite2); */
    /* SUITE_ADD_TEST(suite, Test_ftapprox_cross_linelm1); */
    /* SUITE_ADD_TEST(suite, Test_ftapprox_cross_linelm2); */
    /* SUITE_ADD_TEST(suite, Test_ftapprox_cross_linelm3); */
    /* SUITE_ADD_TEST(suite, Test_sin10dint); */

    //SUITE_ADD_TEST(suite, Test_sin100dint);
    //SUITE_ADD_TEST(suite, Test_sin1000dint);
    return suite;
}
