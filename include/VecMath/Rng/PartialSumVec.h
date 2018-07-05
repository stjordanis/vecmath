#ifndef  PartialSumVec_h
#define  PartialSumVec_h

// #include "Vc/Vc"
#include "VecCore/VecCore"
#include "VecCore/Backend/VcSimdArray.h"


//  Constants - until moved to common header
//
constexpr uint64_t gMers61 = 0x1fffffffffffffff;

constexpr int VecWidth=4;  // For AVX ... 64 x 4 = 256 bits

// ---------------------------------------------------------------
// If two numbers are both 0 <= a, b < P
//   the following method can be used for their sum Mod p
//
template< typename dataType>
dataType 
SumModuloP( dataType a, dataType b, uint64_t P )
{
  vecCore::Mask<dataType> overP= a > (dataType) P;
  MaskedAssign( a, overP, a-P );
}

// ---------------------------------------------------------------
//
//
template< typename dataType, bool largeVal = false >
dataType 
FastModuloP61( const dataType a )
{
  auto low = a & gMers61;  //  Max = gMers61 = 0x0fff ffff ffff ffff
  auto hi  = a >> 61;      //  Max = 7 
  auto s   = low + hi;     //  Max = 0x1000 0000 0000 0006

  auto low2 = s & gMers61; 
  auto hi2  = s >> 61;     //  Max = 1
  return low2 + hi2;       
}

// ---------------------------------------------------------------
//
//
template< typename dataType, unsigned int N, bool enableMod >
void
PartialSumVec( const dataType inpArr[N], dataType outArr[N] )
{
   using vecCore::Set;
   using vecCore::Get;
   using ScalarType = uint64_t; // or Scalar<dataType> ...
   using Mask_v = Vc::SimdMaskArray<ScalarType, VecWidth>;
  
   // First implementation assumes that dataType is VcSIMDArray<4>::UInt64_v
   dataType sum1[N];

   // Shift one - from even to odd only
   for( int i= 0; i<N; ++i) {
      dataType shift1;      
      shift1 = dataType( 0UL );
      Set( shift1, 1, Get( inpArr[i], 0 ));
      Set( shift1, 3, Get( inpArr[i], 2 ));
      // General:
      // for( int k= 1; i<=N; k+= 2) {
      // Set( shift1[i], k, Get( inpArr[i], k-1 ));      

      sum1[i] = inpArr[i] + shift1;
   }
   // Expected result (til now): 
   // Index: [ 0   1    2     3   ]    [  4   5    6     7   ]
   //  'i'     ------- 0 ----------    ---------- 1 ----------   
   // Local: [ 0   1    2     3   ]    [  0   1    2     3   ]  
   // Sums:  [ 0  0+1   2    2+3  ]    [  4  4+5   6    6+7  ]
   
   const Mask<dataType> MaskStep2 = { false, false, true, true };
   for( int i= 0; i<N; ++i) {
      dataType    tshift2( 0UL );
      ScalarType  val1= Get( sum1[i], 1 );
      tshift2= Blend( MaskStep2, dataType( 0L ), dataType(val1) );
      // Sum of up to four values
      outArr[i] = sum1[i] + tshift2; 
   }
   // Expected result (til now): 
   // Global: [ 0   1    2     3   ]    [  4   5    6     7   ]
   //  'i'     ------- 0 ----------    ---------- 1 ----------
   // Local:  [ 0   1    2     3   ]    [  0   1    2     3   ]  
   // Sums:   [ 0  0+1   0-2  0-3  ]    [  4  4+5  4-6   4-7  ]

   for( int i= 1; i<N; i+=2)
   {
      ScalarType  sumPrevious= Get( outArr[i-1], 3 );
      outArr[i] += dataType( sumPrevious );
   }
   // Expected result (til now): 
   // Global: [ 0   1    2     3   ]    [  4   5    6     7   ]
   //  'i'     ------- 0 ----------    ---------- 1 ----------
   // Local:  [ 0   1    2     3   ]    [  0   1    2     3   ]  
   // Sums:   [ 0  0+1   0-2  0-3  ]    [ 0-4 0-5  0-6   0-7  ]

   // Global: [ 8   9   10    11   ]    [ 12   13    14    15   ]
   //  'i'     ------- 2 ----------    ---------- 3 ----------
   // Local:  [ 0   1    2     3   ]    [  0    1     2     3   ]  
   // Sums:   [ 8  8-9  8-10  8-11 ]    [ 8-12 8-13  8-14  8-15  ]

   
   if( enableMod ){
      for( int i= 0; i<N; ++i) {
         outArr[i] = FastModuloMers61( outArr[i] );
      }
   }

   //for( int k= 2; i<N; i+=2)
   // {
   constexpr int k= 2;
   ScalarType sum8= Get( outArr[k-1], 3 );
   for( int i= 2; i< std::max(4U,N); i++) {
      outArr[i] += dataType( sum8 );

      // Expected result (til now): 
      // Global: [ 8   9   10    11   ]    [ 12   13    14    15   ]
      //  'i'     ------- 2 ----------    ---------- 3 ----------
      // Local:  [ 0   1    2     3   ]    [  0    1     2     3   ]  
      // Sums:   [0-8 0-9  0-10  0-11 ]    [ 0-12 0-13  0-14  0-15  ]

      if( enableMod ){
          ModMers61( outArr[i] );
      }
   }
   // }
}


/***
template< typename dataType, unsigned int N >
void
PartialSumV3( dataType a[] ) // , unsigned int N )  // [N] // InOut
{
  int intermed= 4;
  for( unsigned int i=1; i< intermed; i += i )
  {
    unsigned int len= i; 
    unsigned int skip= i+i;
    for( unsigned int j=i; j< N; j += skip )
    {      
       dataType prev= a[j-1];
       unsigned int kTop = Min( j + len, N );
       for( unsigned int k=j; k< kTop; k++ )
       {
          a[k] += prev;
       }
    }
  }
  for( unsigned int i=intermed; i< N; i += i )
  {
    unsigned int len= i; 
    unsigned int skip= i+i;
    for( unsigned int j=i; j< N; j += skip )
    {      
       dataType prev= a[j-1];
       unsigned int kTop = Min( j + len, N );
       for( unsigned int k=j; k< kTop; k++ )
       // Must be fully vectorised (modulo tail ? )
       {
          a[k] += prev;
       }
    }
  }  
}
***/

template< typename dataType, unsigned int N>
void
AltPartialSumModuloP( dataType a[], const int P ) // , unsigned int N )  // [N] // InOut
{
  for( unsigned int i=1; i< N; i += i )
  {
    unsigned int len= i; 
    unsigned int skip= i+i;
    for( unsigned int j=i; j< N; j += skip )
    {      
       dataType prev= a[j-1];
       unsigned int kTop = Min( j + len, N );
       for( unsigned int k=j; k< kTop; k++ )
       {
          a[k] += prev;
          Mask<dataType> overP= a[k] > P;
          MaskedAssign( a[k], overP, a[k]-P );
          //  or use a FastModulo(x,P61) ??
       }
    }
  }
}

#endif
