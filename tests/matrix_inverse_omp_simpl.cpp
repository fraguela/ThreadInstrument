//********************************************************************************
// Copyright (c) 2007-2014 Intel Corporation. All rights reserved.              **
//                                                                              **
// Redistribution and use in source and binary forms, with or without           **
// modification, are permitted provided that the following conditions are met:  **
//   * Redistributions of source code must retain the above copyright notice,   **
//     this list of conditions and the following disclaimer.                    **
//   * Redistributions in binary form must reproduce the above copyright        **
//     notice, this list of conditions and the following disclaimer in the      **
//     documentation and/or other materials provided with the distribution.     **
//   * Neither the name of Intel Corporation nor the names of its contributors  **
//     may be used to endorse or promote products derived from this software    **
//     without specific prior written permission.                               **
//                                                                              **
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"  **
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE    **
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE   **
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE     **
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR          **
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF         **
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS     **
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN      **
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)      **
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF       **
// THE POSSIBILITY OF SUCH DAMAGE.                                              **
//********************************************************************************

/*
 LANG=en_US g++-5.3.0 -fopenmp -O3 -DNDEBUG -I. -I${HOME}/local/include -DNOSERIAL -DNOTEST -o matrix_inverse_omp matrix_inverse_omp.cpp  ~/local/lib/libopenblas.a
 */

#include <utility>
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <cstring>
#include <cassert>
#include <unistd.h>
#include <sys/time.h>
#include "thread_instrument/thread_instrument.h"
#include "tile.h"

int arg_parse(int argc, const char **argv)
{
  if (argc > optind && 0 != atoi(argv[optind]))
  {
    std::cout << "Generating matrix of size " << argv[optind] << std::endl;
  }
  else
  {
    std::cout << "Usage: matrix_inverse dim [tilesize]\n";
    exit(-1);
  }
  
  const int tdim = atoi(argv[optind]);
  
  if (argc > (optind + 1) && 0 != atoi(argv[optind + 1]))
  {
    TILESIZE = atoi(argv[optind + 1]);
  } else {
    TILESIZE = 100;
  }

  return tdim;
}

class tile_array
{
  int m_dim;
  int m_size;
  tile *m_tiles;
  
public:
  
  int dim() const { return m_dim; }
  int size() const { return m_size; }
  
  tile_array( int size = 0 ) :
  m_dim((size + TILESIZE - 1)/TILESIZE), // Size/TILESIZE rounded up
  m_size(size),
  m_tiles( NULL )
  {
    if( m_dim ) m_tiles = new tile[m_dim*m_dim];
  }
  
  ~tile_array()
  {
    delete[] m_tiles;
  }
  
  tile_array(const tile_array& t)
  {
    m_size = t.m_size;
    m_dim = t.m_dim;
    m_tiles = new tile[m_dim*m_dim];
    for (int i = 0; i < m_dim*m_dim; ++i)
      m_tiles[i] = t.m_tiles[i];
  }
  
  tile_array& operator=(const tile_array& t)
  {
    if (this != &t)
    {
      delete[] m_tiles;
      m_size = t.m_size;
      m_dim = t.m_dim;
      m_tiles = new tile[m_dim*m_dim];
      for (int i = 0; i < m_dim*m_dim; ++i)
        m_tiles[i] = t.m_tiles[i];
      
    }
    return *this;
  }
  
  void dump( double epsilon = 1e-12 ) const {
    for (int i = 0; i < m_dim; i++ )
    {
      for (int j = 0; j < m_dim; j++ )
      {
        std::cout << "(" << i << "," << j << ")" << std::endl;
        m_tiles[m_dim*i+j].dump(epsilon);
      }
      std::cout << std::endl;
    }
  }
  
  int generate_matrix( int dimension )
  {
    printf("Floating point elements per matrix: %i x %i\n", dimension, dimension);
    printf("Floating point elements per tile: %i x %i\n", TILESIZE, TILESIZE);
    
    delete[] m_tiles;
    m_size = dimension;
    m_dim = (m_size + TILESIZE - 1)/TILESIZE; // Size/TILESIZE rounded up
    m_tiles = new tile[m_dim*m_dim];
    
    printf("tiles per matrix: %i x %i\n", m_dim, m_dim);
    int dim = m_dim;
    int size = m_size;
    
    std::cout << "dim(" << dim << ") size(" << size << ")" << std::endl;
    int ii = 0;
    double e = 0.0;
    for (int I = 0; I < dim; I++)
    {
      for (int i = 0; i < TILESIZE; i++)
      {
        int jj = 0;
        for (int J = 0; J < dim; J++)
        {
          for (int j = 0; j < TILESIZE; j++)
          {
            if ((ii < size)&(jj < size)) e = double(rand())/RAND_MAX;
            else if (ii == jj) e = 1; // On-diagonal padding
            else e = 0; // Off-diagonal padding
            m_tiles[dim*I + J].set(i,j,e);
            jj++;
          }
        }
        ii++;
      }
    }
    return m_dim;
  }
  
  
  int identity_check( double epsilon = MINPIVOT ) const
  {
    int ecount = 0;
    
    for (int i = 0; i < m_dim; i++ )
    {
      for (int j = 0; j < m_dim; j++ )
      {
        int tcount = 0;
        
        tile &t = m_tiles[m_dim*i+j];
        
        tcount = (i == j) ?  t.identity_check(epsilon) : t.zero_check(epsilon);
        
        if (tcount == 0 ) continue;
        
        std::cout << "problem in tile(" << i << "," << j << ")" << std::endl;
        ecount += tcount;
      }
    }
    return ecount;
  }
  
  bool equal( const tile_array &b ) const
  {
    if (b.m_dim != m_dim) return false;
    
    for (int i = 0; i < m_dim; i++ )
    {
      for (int j = 0; j < m_dim; j++ )
      {
        tile &t = m_tiles[m_dim*i+j];
        if (!t.equal( b.m_tiles[m_dim*i+j])) return false;
      }
    }
    return true;
  }
  
  // c = this * b
  tile_array multiply(const tile_array &b) const
  {
    tile_array c(m_size);
    tile t;
    for (int i = 0; i < m_dim; i++)
    {
      for (int j = 0; j < m_dim; j++)
      {
        t.zero();
        for (int k = 0; k < m_dim; k++)
        {
          t.multiply_add_in_place(m_tiles[m_dim*i+k], b.m_tiles[m_dim*k+j]);
        }
        c.m_tiles[m_dim*i+j] = t;
      }
    }
    return c;
  }
  
  
  tile_array inverse()
  {
    tile_array b = *this;
    int dim = m_dim;
    
    for (int n = 0; n < dim; n++)
    {
      tile pivot_inverse = b.m_tiles[dim*n+n].inverse();
      b.m_tiles[dim*n+n] = pivot_inverse;
      
      for (int j = 0; j < dim; j++)
      {
        if (j == n) continue;
        
        tile& tnj = b.m_tiles[dim*n+j];
        b.m_tiles[dim*n+j] = pivot_inverse.multiply(tnj);
      }
      
      for (int i = 0; i < dim; i++)
      {
        if (i == n) continue;
        
        tile tin = b.m_tiles[dim*i+n];
        b.m_tiles[dim*i+n] = tin.multiply_negate(pivot_inverse);
        
        for (int j = 0; j < dim; j++)
        {
          if (j == n) continue;
          tile &tnj = b.m_tiles[dim*n+j];
          b.m_tiles[dim*i+j].multiply_subtract_in_place(tin,tnj);
        }
      }
    }
    return b;
  }
  
  tile_array inverse_omp();
};



tile_array tile_array::inverse_omp()
{
  // This cannot be replaced by a THREADINSTRUMENT_TIMED_LOG because the created variable
  //would not be known outside
  ThreadInstrument::log("INIT_COPY", 0, true);
  tile_array b = *this;
  ThreadInstrument::log("INIT_COPY", 1, true);
  
  int dim = m_dim;
  tile * const tiles = b.m_tiles;

#pragma omp parallel
#pragma omp single
  {
    for (int n = 0; n < dim; n++)
    {
      //tile& pivot_inverse = tiles[dim*n+n];
      
#pragma omp task depend(inout:tiles[dim*n+n])
      {
        THREADINSTRUMENT_TIMED_LOG("INVERSE",
                                   tiles[dim*n+n] = tiles[dim*n+n].inverse();
                                   );
      }
      
      for (int j = 0; j < dim; j++)
      {
        if (j == n) continue;
        
        //tile& tile_nj = tiles[dim*n+j];
        
#pragma omp task depend(inout:tiles[dim*n+j]) depend(in:tiles[dim*n+n])
        {
          THREADINSTRUMENT_TIMED_LOG("MULTIPLY",
                                     tiles[dim*n+j] = tiles[dim*n+n].multiply(tiles[dim*n+j]);
                                     );
        }
      }
      
      for (int i = 0; i < dim; i++)
      {
        if (i == n) continue;
        
        //tile& tin = b.m_tiles[dim*i+n];
        
        
        for (int j = 0; j < dim; j++)
        {
          if (j == n) continue;
          
          //tile& tile_ij = b.m_tiles[dim*i+j];
          //tile& tile_nj = b.m_tiles[dim*n+j];
#pragma omp task depend(inout:tiles[dim*i+j]) depend(in:tiles[dim*i+n], tiles[dim*n+j])
          {
            THREADINSTRUMENT_TIMED_LOG("MULT_SUBS_IN_PLACE",
                                       tiles[dim*i+j].multiply_subtract_in_place(tiles[dim*i+n], tiles[dim*n+j]);
                                       );
          }
        }
        
        //tile& tile_in = b.m_tiles[dim*i+n];
#pragma omp task depend(inout:tiles[dim*i+n]) depend(in:tiles[dim*n+n])
        {
          THREADINSTRUMENT_TIMED_LOG("MULT_NEGATE",
                                     tile tcopy = tiles[dim*i+n];
                                     tiles[dim*i+n] = tcopy.multiply_negate(tiles[dim*n+n]);
                                     );
        }
      }
      
    }
    
  } // end omp single, omp parallel
  
  return b;
}

void report_time( const char * mode, tile_array& a, double time ) 
{
  std::cout <<  mode << " Total Time: " << time << " sec" << std::endl;
  float Gflops = ((float)2*a.size()*a.size()*a.size())/((float)1000000000);
  if (Gflops >= .000001) printf("Floating-point operations executed: %f billion\n", Gflops);
  if (time >= .001) printf("Floating-point operations executed per unit time: %6.2f billions/sec\n", Gflops/time);
}

int main(int argc, const char *argv[])
{ struct timeval t0, t1, t;
  
  int tsz = arg_parse(argc, argv);
  tile_array in_array;
  int tdim = in_array.generate_matrix(tsz);
  
  ThreadInstrument::registerLogPrinter(ThreadInstrument::pictureTimePrinter);

  /* Should use OMP_NUM_THREADS */
 
#ifndef NOSERIAL
    // invert serially
  std::cout << "Invert serially" << std::endl;
  gettimeofday(&t0, NULL);
  tile_array out_array = in_array.inverse();
  gettimeofday(&t1, NULL);
  timersub(&t1, &t0, &t);
  report_time( "Serial", out_array, t.tv_sec + t.tv_usec / 1000000.0 );

    //tile_array test = in_array.multiply(out_array);
    //test.identity_check(1e-6);
#endif

  std::cout << "Invert Omp" << std::endl;
  //debug::set_num_threads(1);
  gettimeofday(&t0, NULL);
  tile_array out_array2 = in_array.inverse_omp();
  gettimeofday(&t1, NULL);
  timersub(&t1, &t0, &t);
  report_time( "Omp", out_array2, t.tv_sec + t.tv_usec / 1000000.0 );
  
#ifndef NOTEST
  tile_array test2 = in_array.multiply(out_array2);
  test2.identity_check(1e-6);
#endif
  
  ThreadInstrument::dumpLog("matrix_inverse_omp_simpl.log");
  return 0;
}
