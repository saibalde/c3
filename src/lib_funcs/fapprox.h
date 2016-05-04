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

/** \file fapprox.h
 * Provides header files and structure definitions for functions in fapprox.c 
 */

#ifndef FAPPROX_H
#define FAPPROX_H

#include <stdlib.h>

#include "functions.h"

struct OneApproxOpts
{
    enum function_class fc;
    void * aopts;
};

struct OneApproxOpts;
struct OneApproxOpts * 
one_approx_opts_alloc(enum function_class, void *);
void one_approx_opts_free(struct OneApproxOpts *);



struct MultiApproxOpts;
struct MultiApproxOpts * multi_approx_opts_alloc(size_t);
void multi_approx_opts_free(struct MultiApproxOpts *);
void multi_approx_opts_set_dim(struct MultiApproxOpts *,
                               size_t ,
                               struct OneApproxOpts *);
void
multi_approx_opts_set_all_same(struct MultiApproxOpts *,
                               struct OneApproxOpts *);

enum function_class 
multi_approx_opts_get_fc(const struct MultiApproxOpts *, size_t);
void * multi_approx_opts_get_aopts(const struct MultiApproxOpts *, size_t);
size_t multi_approx_opts_get_dim(const struct MultiApproxOpts *);



#endif