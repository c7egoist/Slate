//============================================================================================================================================
//                                                     SPATIALARITHMETICPROOF.CPP
//============================================================================================================================================
// 🧩 Pins the behaviour of the spatial arithmetic that step 10 lifted out of 21 translation units.
//
// 🔴 119 identical copies of nine functions were folded into one definition in `CurveSpecification.h`.
//    Every caller in the CAD kernel now reaches that one copy, so a wrong sign in it is wrong everywhere
//    at once rather than in one file. These claims are the arithmetic itself, written independently of
//    the implementation: `Cross` is checked by perpendicularity as well as by its components, and
//    `Difference` is checked in the direction the 15 original copies actually computed.
//
// ⚠️ `Difference(Left, Right)` returns the direction FROM Left TO Right. That reads backwards against the
//    name. It is pinned here deliberately, because it is what the copies did and what every caller expects.

#include "SlateShape/Geometry/CurveSpecification/Api/CurveSpecification.h"
#include <cstdio>
#include <cmath>
using namespace Slate;
int F=0,C=0;
void K(bool h,const char*w){++C; if(!h){++F;printf("  FAIL %s\n",w);} }
bool Near(double a,double b){return std::fabs(a-b)<1e-12;}
int main(){
  const SpatialDirection A{1.0,2.0,3.0}, B{-4.0,5.0,-6.0};
  const SpatialPoint P{7.0,8.0,9.0}, Q{1.0,1.0,1.0};
  K(Near(LengthSquared(A),14.0),"LengthSquared");
  K(Near(Dot(A,B),-4.0+10.0-18.0),"Dot");
  const SpatialDirection X=Cross(A,B);
  K(Near(X.Left,2.0*-6.0-3.0*5.0)&&Near(X.Up,3.0*-4.0-1.0*-6.0)&&Near(X.Forward,1.0*5.0-2.0*-4.0),"Cross");
  K(Near(Dot(Cross(A,B),A),0.0)&&Near(Dot(Cross(A,B),B),0.0),"Cross is perpendicular to both");
  const SpatialDirection S=Scaled(A,2.5);
  K(Near(S.Left,2.5)&&Near(S.Up,5.0)&&Near(S.Forward,7.5),"Scaled");
  const SpatialDirection N=Negated(A);
  K(Near(N.Left,-1.0)&&Near(N.Up,-2.0)&&Near(N.Forward,-3.0),"Negated");
  const SpatialDirection D=Added(A,B);
  K(Near(D.Left,-3.0)&&Near(D.Up,7.0)&&Near(D.Forward,-3.0),"Added(direction,direction)");
  const SpatialPoint PP=Added(P,A);
  K(Near(PP.Left,8.0)&&Near(PP.Up,10.0)&&Near(PP.Forward,12.0),"Added(point,direction)");
  const SpatialDirection Df=Difference(P,Q);
  K(Near(Df.Left,-6.0)&&Near(Df.Up,-7.0)&&Near(Df.Forward,-8.0),"Difference is FROM left TO right");
  const SpatialDirection U=Normalize(A);
  K(Near(LengthSquared(U),1.0),"Normalize gives unit length");
  const SpatialDirection Z=Normalize(SpatialDirection{0.0,0.0,0.0});
  K(Near(Z.Left,1.0)&&Near(Z.Up,0.0)&&Near(Z.Forward,0.0),"Normalize of nothing gives the left axis");
  static_assert(LengthSquared(SpatialDirection{1.0,2.0,2.0})==9.0,"constexpr");
  printf("%d claims, %d failures\n%s\n",C,F,F?"REFUTED":"PROVEN");
  return F?1:0;
}
