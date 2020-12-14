/* Copyright 2020 Stanford University
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#if (__AVX2__ == 1) || (__AVX__ == 1)
#include <immintrin.h>
#endif

struct Kernel {
    long iterations;
    void *x;
    Kernel(long y) : iterations(y) {}
};

double execute_kernel_compute(const Kernel &k) {
#if __AVX2__ == 1
  __m256d A[16];
  
  for (int i = 0; i < 16; i++) {
    //A[i] = _mm256_set_pd(1.0f, 2.0f, 3.0f, 4.0f);
    A[i] = _mm256_set_pd(0.9f, 0.99f, 0.999f, 0.9999f);
  }
  
  for (long iter = 0; iter < k.iterations; iter++) {
    for (int i = 0; i < 16; i++) {
      //A[i] = _mm256_fmadd_pd(A[i], A[i], A[i]);
      A[i] = _mm256_fnmsub_pd(A[i], A[i], A[i]);
    }
  }
#else
  double A[64];
  
  for (int i = 0; i < 64; i++) {
    //A[i] = 1.2345;
    A[i] = 0.999;
  }
  
  for (long iter = 0; iter < k.iterations; iter++) {
    for (int i = 0; i < 64; i++) {
        //A[i] = A[i] * A[i] + A[i];
        A[i] = A[i] - A[i] * A[i];
    }
  } 
#endif
  double *C = (double *)A;
  double dot = 1.0;
  for (int i = 0; i < 64; i++) {
    dot *= C[i];
  }
  return dot;  
}

