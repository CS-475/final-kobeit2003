#ifndef EDGE_UTILS_H
#define EDGE_UTILS_H


#include "include/GPoint.h"
#include <cmath>


struct Edge {
   float m;   
   float b;   
   int top;   
   int bottom;
   int xR; 
   int xL;
   int wind;
   int currentX; 
  


   bool isValid(int y) const {
       return (y >= top && y < bottom);
   }


   float computeX(int y) const {
       return m * y + b;
   }


   bool isUseful() const {
       return top < bottom;
   }
};


Edge clipEdge(const Edge& edge, int canvasWidth, int canvasHeight);
Edge makeEdge(const GPoint& p0, const GPoint& p1);


#endif