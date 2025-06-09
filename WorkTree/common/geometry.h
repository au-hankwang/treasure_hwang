#ifndef __GEOMETRY_H_INCLUDED__
#define __GEOMETRY_H_INCLUDED__

#ifndef __GLOBALS_H_INCLUDED__
enum layertype {} ;
#endif


const char *LayerNames(enum layertype layer);

class polytype;
class PENtype;

class intervaltype{
public:
   int low, high;
   int node, extra;
   intervaltype()
      {
      low  = MAX_INT;
      high = MIN_INT;
      node = -1;
      extra = 0;
      }
   intervaltype(int _low, int _high, int _node=-1, int _extra=0) : low(_low), high(_high), node(_node), extra(_extra)
      {}
   void Merge(int point)
      {
      low  = MINIMUM(low,  point);
      high = MAXIMUM(high, point);
      }
   void Merge(const intervaltype &right)
      {
      low  = MINIMUM(low,  right.low);
      high = MAXIMUM(high, right.high);
      }
   bool Overlap(const intervaltype &right) const
      {
      return right.high >= low && right.low <= high;
      }
   bool operator<(const intervaltype &right) const  // predicate for heap routines when doing sweep. Sorts by HIGH not LOW
      {
      return high < right.high;
      }
   };


class pointtype{
public:
   int x,y;

   pointtype()
      {
      x=y=0;
      }
   pointtype(const pointtype &p)
      {
      x = p.x;
      y = p.y;
      }
   pointtype(int _x, int _y)
      {
      x = _x;
      y = _y;
      }
   pointtype operator+(const pointtype &right) const
      {
      pointtype ret(*this);
      ret.x += right.x;
      ret.y += right.y;
      return ret;
      }
   pointtype operator-(const pointtype &right) const
      {
      pointtype ret(*this);
      ret.x -= right.x;
      ret.y -= right.y;
      return ret;
      }
   pointtype operator+(const int &right) const
      {
      pointtype ret(*this);
      ret.x += right;
      ret.y += right;
      return ret;
      }
   pointtype &operator+=(const pointtype &right)
      {
      x += right.x;
      y += right.y;
      return *this;
      }
   pointtype &operator-=(const pointtype &right)
      {
      x -= right.x;
      y -= right.y;
      return *this;
      }
   pointtype &operator+=(const int &right)
      {
      x += right;
      y += right;
      return *this;
      }
   pointtype operator-() const 
      {
      pointtype p(-x,-y);
      return p;
      }
   bool operator<(const pointtype &right) const // no real meaning but lets a sort group like points
      {
      if (x < right.x) return true;
      if (x > right.x) return false;
      if (y < right.y) return true;
      if (y > right.y) return false;
      return false;
      }
   int Distance(const pointtype &right) const  // returns distance squared
      {
      return (__int64)(x-right.x)*(x-right.x) + (__int64)(y-right.y)*(y-right.y);
      }
   int operator*(const pointtype &right) const  // really cross product
      {
      return x*right.y - right.x*y;
      }
   bool operator==(const pointtype &right) const
      {
      return x==right.x && y==right.y;
      }
   bool operator!=(const pointtype &right) const
      {
      return !operator==(right);
      }
   };


class edgetype{
public:
   pointtype p1,p2;
   
   edgetype()
      {}
   edgetype(pointtype _p1, pointtype _p2) : p1(_p1), p2(_p2)
      {}
   bool Intersect(const edgetype &right) const
      {
      // do a quick rejection test only looking at the mbb
      if ((MAXIMUM(p1.x, p2.x) <= MINIMUM(right.p1.x, right.p2.x)) || (MINIMUM(p1.x, p2.x) >= MAXIMUM(right.p1.x, right.p2.x)))
         return false;
      if ((MAXIMUM(p1.y, p2.y) <= MINIMUM(right.p1.y, right.p2.y)) || (MINIMUM(p1.y, p2.y) >= MAXIMUM(right.p1.y, right.p2.y)))
         return false;
      
      int cross1,cross2;
      cross1 = (right.p1 - p1) * (p2 - p1);      // compute 2 cross products
      cross2 = (right.p2 - p1) * (p2 - p1);
      if (cross1 < 0 && cross2 >0) return true;
      if (cross1 > 0 && cross2 <0) return true;
      if (cross1 == 00 || cross2==0) return true;  // this happens when an endpoint is coincident with the line
      return false;
      }
   // this function doesn't consider an endpoint on the line to count
   bool IntersectStrict(const edgetype &right) const
      {
      // do a quick rejection test only looking at the mbb
      if ((MAXIMUM(p1.x, p2.x) <= MINIMUM(right.p1.x, right.p2.x)) || (MINIMUM(p1.x, p2.x) >= MAXIMUM(right.p1.x, right.p2.x)))
         return false;
      if ((MAXIMUM(p1.y, p2.y) <= MINIMUM(right.p1.y, right.p2.y)) || (MINIMUM(p1.y, p2.y) >= MAXIMUM(right.p1.y, right.p2.y)))
         return false;
      
      int cross1,cross2;
      cross1 = (right.p1 - p1) * (p2 - p1);      // compute 2 cross products
      cross2 = (right.p2 - p1) * (p2 - p1);
      if ((cross1 < 0 && cross2 >0) || (cross1 > 0 && cross2 <0))
         {  // now check again
         cross1 = (p1 - right.p1) * (right.p2 - right.p1);      // compute 2 cross products
         cross2 = (p2 - right.p1) * (right.p2 - right.p1);
         if ((cross1 < 0 && cross2 >0) || (cross1 > 0 && cross2 <0))
            return true;
         }      
      return false;
      }
   };


class recttype{
public:   
   int x0, y0, x1, y1;  // x1>=x0 y1>=y0

   recttype()
      {
      x0 = y0 = x1 = y1 = 0;
      }
   recttype(const recttype &right)
      {
      x0 = right.x0;
      x1 = right.x1;
      y0 = right.y0;
      y1 = right.y1;
      }
#ifdef _MSC_VER
   recttype(const RECT &right)
      {
      x0 = right.left;
      x1 = right.right;
      y0 = right.top;
      y1 = right.bottom;
      }
#endif
   recttype(const pointtype &tl, const pointtype &br)
      {
      x0 = MINIMUM(tl.x, br.x);
      x1 = MAXIMUM(tl.x, br.x);
      y0 = MINIMUM(tl.y, br.y);
      y1 = MAXIMUM(tl.y, br.y);
      }
   recttype(int _x0, int _y0, int _x1, int _y1)
      {
      x0 = MINIMUM(_x0, _x1);
      y0 = MINIMUM(_y0, _y1);
      x1 = MAXIMUM(_x0, _x1);
      y1 = MAXIMUM(_y0, _y1);
      }
   pointtype top_left() const
      {
      pointtype p(x0, y0);
      return p;
      }
   pointtype bottom_right() const
      {
      pointtype p(x1, y1);
      return p;
      }
   recttype &operator=(const recttype &right)
      {
      x0 = right.x0;
      x1 = right.x1;
      y0 = right.y0;
      y1 = right.y1;
      return *this;
      }
   recttype operator+(const pointtype &right) const
      {
      recttype temp(*this);
      temp.x0 += right.x;
      temp.x1 += right.x;
      temp.y0 += right.y;
      temp.y1 += right.y;
      return temp;
      }
   recttype &operator+=(const pointtype &right)
      {      
      x0 += right.x;
      x1 += right.x;
      y0 += right.y;
      y1 += right.y;
      return *this;
      }
   int Width() const
      {
      return x1-x0;
      }
   int Height() const
      {
      return y1-y0;
      }
   bool operator==(const recttype &right) const
      {
      return x0==right.x0 && x1==right.x1 && y0==right.y0 && y1==right.y1;
      }
   int Area() const
      {
      return Height() * Width();
      }
   bool Overlap(const recttype &right) const  // will return true for coincident edges
      {
      if ((right.x0 <= x1 && right.x1 >= x0) || (x0 <= right.x1 && x1 >= right.x0))
         if ((right.y0 <= y1 && right.y1 >= y0) || (y0 <= right.y1 && y1 >= right.y0))
            return true;
      return false;
      }
   bool Overlap_Strict(const recttype &right) const  // will return false for coincident edges
      {
      if ((right.x0 < x1 && right.x1 > x0) || (x0 < right.x1 && x1 > right.x0))
         if ((right.y0 < y1 && right.y1 > y0) || (y0 < right.y1 && y1 > right.y0))
            return true;
      return false;
      }
   };

class mbbtype{   // this is a minumum bounding box
   recttype r;
   
   public:
      void Null()
         {
         r.x0=r.y0 = MAX_INT;    // largest int
         r.x1=r.y1 = MIN_INT;    // smallest int
         }
      void Everything()
         {
         r.x0=r.y0 = MIN_INT;    // smallest int
         r.x1=r.y1 = MAX_INT;    // largest int
         }
      mbbtype() {Null();}
      mbbtype(const recttype &_r) : r(_r)
         {}
      void operator= (const recttype &right)
         {
         r=right;
         }
      bool Overlap(const recttype &right) const
         {
         return r.Overlap(right);
         }
      bool Overlap(const mbbtype &right) const
         {
         return r.Overlap(right.r);
         }
      void operator +=(const mbbtype &right)
         {
         r.y0 = MINIMUM(r.y0, right.r.y0);
         r.x0 = MINIMUM(r.x0, right.r.x0);
         r.y1 = MAXIMUM(r.y1, right.r.y1);
         r.x1 = MAXIMUM(r.x1, right.r.x1);
         }
      void Add(pointtype p)
         {
         r.y0 = MINIMUM(r.y0, p.y);
         r.x0 = MINIMUM(r.x0, p.x);
         r.y1 = MAXIMUM(r.y1, p.y);
         r.x1 = MAXIMUM(r.x1, p.x);
         }
      void Add(int x, int y)
         {
         r.y0 =MINIMUM(r.y0, y);
         r.x0 =MINIMUM(r.x0, x);
         r.y1 =MAXIMUM(r.y1, y);
         r.x1 =MAXIMUM(r.x1, x);
         }
      void Add(const recttype &q)
         {
         r.y0 = MINIMUM(r.y0, q.y0);
         r.x0 = MINIMUM(r.x0, q.x0);
         r.y1 = MAXIMUM(r.y1, q.y1);
         r.x1 = MAXIMUM(r.x1, q.x1);
         }
      void Expand(int amount)
         {
         r.y0 -= amount;
         r.x0 -= amount;
         r.y1 += amount;
         r.x1 += amount;
         }
      bool IsNull() const
         {
         if (r.y0 > r.y1 || r.x0 > r.x1) return true;
         return false;
         }
      bool IsEverything() const
         {
         if (r.y0 == MIN_INT || r.x0==MIN_INT || r.y1==MAX_INT || r.x1==MAX_INT)
            return true;
         return false;
         }
      recttype &Rect()
         {return r;}
      const recttype &Rect() const
         {return r;}
   };


// this function uses a translation matrix which works like this
//   [X]global = [t11 t12 t13] [X] local
//   [Y]         [t21 t22 t23] [Y]
//   [K]         [t31 t32 t33] [1]        
class translationtype{
public:
   double matrix[2][3];
   
   translationtype()
      {
      SetToIdentity();
      }
   void SetToIdentity()
      {
      matrix[0][0] = matrix[0][1] = matrix[0][2] = 0.0;
      matrix[1][0] = matrix[1][1] = matrix[1][2] = 0.0;
      matrix[0][0] = 1;   // no rotation, no translation
      matrix[1][1] = 1;
      }
   translationtype& operator=(const translationtype &rhs)
      {
      if(this == &rhs)
         return *this;
      memcpy(matrix, rhs.matrix, sizeof(matrix));
      return *this;
      }
   void RotateCW90()
      {
      translationtype t;
      t.matrix[0][0] = 0;  t.matrix[0][1] = 1;  t.matrix[0][2] = 0;
      t.matrix[1][0] = -1; t.matrix[1][1] = 0;  t.matrix[1][2] = 0;
      *this = (*this) * t;
      }
   void RotateCCW90()
      {
      translationtype t;
      t.matrix[0][0] = 0;  t.matrix[0][1] = -1; t.matrix[0][2] = 0;
      t.matrix[1][0] = 1;  t.matrix[1][1] = 0;  t.matrix[1][2] = 0;
      *this = (*this) * t;
      }
   void Rotate180()
      {
      translationtype t;
      t.matrix[0][0] = -1; t.matrix[0][1] = 0;  t.matrix[0][2] = 0;
      t.matrix[1][0] = 0;  t.matrix[1][1] = -1; t.matrix[1][2] = 0;
      *this = (*this) * t;
      }
   void NegateX()                   // flip along Y axis(transpose left-right)
      {
      translationtype t;
      t.matrix[0][0] = -1; t.matrix[0][1] = 0;  t.matrix[0][2] = 0;
      t.matrix[1][0] = 0;  t.matrix[1][1] = 1;  t.matrix[1][2] = 0;
      *this = (*this) * t;
      }
   void NegateY()                   // flip along X axis(transpose top-bottom)
      {
      translationtype t;
      t.matrix[0][0] = 1;  t.matrix[0][1] = 0;  t.matrix[0][2] = 0;
      t.matrix[1][0] = 0;  t.matrix[1][1] = -1; t.matrix[1][2] = 0;
      *this = (*this) * t;
      }
   void Scale(float xscale, float yscale)
      {
      matrix[0][0] *= xscale; matrix[1][0] *= xscale; 
      matrix[0][1] *= yscale; matrix[1][1] *= yscale; 
      }
   void Translate(pointtype p)
      {
      matrix[0][2] += p.x;
      matrix[1][2] += p.y;
      }
   void Translate(float x, float y)
      {
      matrix[0][2] += x;
      matrix[1][2] += y;
      }
   translationtype operator*(const translationtype &right) const
      {
      int r,c;
      translationtype t;
      t.matrix[0][0] = 0;  t.matrix[0][1] = 0;  t.matrix[0][2] = matrix[0][2];
      t.matrix[1][0] = 0;  t.matrix[1][1] = 0;  t.matrix[1][2] = matrix[1][2];

      for (r=0; r<2; r++){
         for (c=0; c<3; c++)
            {
            t.matrix[r][c] += matrix[r][0] * right.matrix[0][c];
            t.matrix[r][c] += matrix[r][1] * right.matrix[1][c];
            }
         }
      return t;
      }
   translationtype Transform(const translationtype &right) const
      {
      translationtype t = operator*(right);
      return t;
      }
   pointtype Transform(pointtype l) const
      {
      pointtype g;
      g.x = ROUND_TO_INT(matrix[0][0] * l.x + matrix[0][1] * l.y + matrix[0][2]);
      g.y = ROUND_TO_INT(matrix[1][0] * l.x + matrix[1][1] * l.y + matrix[1][2]);
      return g;
      }
   float TransformX(float x, float y) const
      {
      return matrix[0][0] * x + matrix[0][1] * y + matrix[0][2];
      }
   float TransformY(float x, float y) const
      {
      return matrix[1][0] * x + matrix[1][1] * y + matrix[1][2];
      }
   recttype Transform(recttype l) const
      {
      pointtype p0=l.top_left(), p1=l.bottom_right(), p2, p3;
      recttype r;

      p2.x = ROUND_TO_INT(matrix[0][0] * p0.x + matrix[0][1] * p0.y + matrix[0][2]);
      p2.y = ROUND_TO_INT(matrix[1][0] * p0.x + matrix[1][1] * p0.y + matrix[1][2]);
      p3.x = ROUND_TO_INT(matrix[0][0] * p1.x + matrix[0][1] * p1.y + matrix[0][2]);
      p3.y = ROUND_TO_INT(matrix[1][0] * p1.x + matrix[1][1] * p1.y + matrix[1][2]);

      r.x0 = MINIMUM(p2.x, p3.x);
      r.x1 = MAXIMUM(p2.x, p3.x);
      r.y0 = MINIMUM(p2.y, p3.y);
      r.y1 = MAXIMUM(p2.y, p3.y);
      return r;
      }
   pointtype Inverse(pointtype g) const
      {
      pointtype l;
      float z = matrix[1][1]*matrix[0][0] - matrix[0][1]*matrix[1][0];
      
      l.x = ROUND_TO_INT(( matrix[1][1]*g.x - matrix[0][1]*g.y + matrix[1][2]*matrix[0][1] - matrix[0][2]*matrix[1][1]) / z);
      l.y = ROUND_TO_INT((-matrix[1][0]*g.x + matrix[0][0]*g.y + matrix[0][2]*matrix[1][0] - matrix[1][2]*matrix[0][0]) / z);

//      pointtype p2 = Transform(l);
//      if (p2 != g) 
//         FATAL_ERROR;
      return l;
      }
   float ReturnXScale(float multiplicand=1.0) const
      {
	  float f = matrix[0][0]*multiplicand;
      if (f >=0.0)
	     return f;
	  return -f;
      }
   float ReturnYScale(float multiplicand=1.0) const
      {
	  float f = matrix[1][1]*multiplicand;
      if (f >=0.0)
	     return f;
	  return -f;
      }
   void PrintMatrix()
      {
      printf("\n%2d %2d %2d", matrix[0][0], matrix[0][1], matrix[0][2]);
      printf("\n%2d %2d %2d", matrix[1][0], matrix[1][1], matrix[1][2]);
      }
   pointtype OutToString(char *buffer, const int size, const int internalScale);
   };


struct polypayloadtype{
  float f, g;
  int index;
};


class boxtype{
public:
   recttype r;
   enum layertype layer;
   const char *comment;     // comment, node, handle are merely payload not used by any geometry routines
   const char *propertyvalue;
   int node;
   polypayloadtype payload;

   boxtype()
      {
      clear();
      }
   boxtype(enum layertype _layer, const char* _propertyvalue, const recttype &_r)
      {
      clear();
      layer = _layer;
      propertyvalue=_propertyvalue;
      r = _r;
      }   
   void clear()
      {
      layer = (layertype)0;
      comment  = NULL;
      node     = -1;
      }
   bool IsValid() const {
      return r.x1 > r.x0 && r.y1 > r.y0;
      }
   bool IsInside(const pointtype &p) const
      {
      return p.x >= r.x0 && p.x <= r.x1 && p.y >= r.y0 && p.y <= r.y1;
      }
   bool IsInsideNotOnEdge(const pointtype &p) const
      {
      return p.x > r.x0 && p.x < r.x1 && p.y > r.y0 && p.y < r.y1;
      }
   bool IsInsideNotOnCorner(const pointtype &p) const
      {
      if ((p.x==r.x0 || p.x==r.x1) && (p.y==r.y0 || p.y==r.y1))
         return false;
      return p.x >= r.x0 && p.x <= r.x1 && p.y >= r.y0 && p.y <= r.y1;
      }
   bool Overlap(const boxtype &right) const
      {
      return r.Overlap(right.r);
      }
   mbbtype Mbb() const 
      {
      return mbbtype(r);
      }
   int Area() const
      {
      return r.Area();
      }
   void Translate(const translationtype &t);
   void FetchHorizontalInsideIntervals(int y, array<intervaltype> &intervals) const;
//   void Paint(wxDC &dc, const translationtype &t, const PENtype &pen) const;
   void OutToDLF(FILE *fptr, mbbtype &mbb, const int internalScale, const int units);
   };


class flatpolytype{
public:
   int flat;
   translationtype t;
   recttype mbb;
   const polytype *poly;
   const boxtype *box;
   flatpolytype(int _flat, translationtype &_t, recttype &_mbb, const polytype *_poly, const boxtype *_box) : flat(_flat), t(_t), mbb(_mbb), poly(_poly), box(_box)
      {}
   flatpolytype()
      {
      flat = -1;
      poly=NULL;
      box=NULL;
      }
   bool operator<(const flatpolytype &right) const
      {
      return mbb.y1 < right.mbb.y1;
      }
   pointtype ReturnSingleVertice() const;
   layertype ReturnLayer() const;
   };



class polytype{

public:
   // the points define the polygon and are left-footed. ie. they trace out the polygon counter-clockwise
   // the last point is implied to connect up with the first point
   array <pointtype> points;
   enum layertype layer;
   const char *comment;     // comment, node, handle are merely payload not used by any geometry routines
   const char *propertyvalue;    //Should be the same as comment, but PNR writes out a property value which we need to store.
   polypayloadtype payload;                 // more payload
   int node;
   polytype()
      {
      clear();
      }
   polytype(enum layertype _layer, const char* _propertyvalue, const recttype &r)
      {
      clear();
      layer = _layer;
      propertyvalue=_propertyvalue;
      points.push_back(pointtype(r.x0, r.y0));
      points.push_back(pointtype(r.x0, r.y1));
      points.push_back(pointtype(r.x1, r.y1));
      points.push_back(pointtype(r.x1, r.y0));
      }   
   polytype(const boxtype &right)
      {
      layer   = right.layer;
      comment = right.comment;
      node    = right.node;
      payload.index = -1;
	  payload.f     = 0.0;
	  payload.g     = 0.0;
      if (right.r.x0 > right.r.x1 || right.r.y0 > right.r.y1) FATAL_ERROR;
      points.push_back(pointtype(right.r.x0, right.r. y0));
      points.push_back(pointtype(right.r.x0, right.r. y1));
      points.push_back(pointtype(right.r.x1, right.r. y1));
      points.push_back(pointtype(right.r.x1, right.r. y0));
      if (IsNegative()) FATAL_ERROR;
      }
   polytype(const flatpolytype &right)
      {
      node = right.flat;
      if (right.poly!=NULL)
         {
         comment  = right.poly->comment;
         layer    = right.poly->layer;
         for (int i=0; i<right.poly->points.size(); i++)
            points.push(right.t.Transform(right.poly->points[i]));
         }
      else if (right.box!=NULL)
         {
         comment  = right.box->comment;
         layer    = right.box->layer;
         pointtype tr = right.t.Transform(right.box->r.top_left());
         pointtype bl = right.t.Transform(right.box->r.bottom_right());
         points.push(tr);
         points.push(tr);
         points.push(bl);
         points.push(bl);
         points[1].y = points[2].y;
         points[3].y = points[0].y;
         }
      else
         FATAL_ERROR;
      }
   void clear()
      {
      layer = (layertype)0;
      comment  = NULL;
      propertyvalue=NULL;
      node     = -1;
      payload.index = -1;
	  payload.f = 0.0;
	  payload.g = 0.0;
	  points.clear();
      }
   polytype &operator+=(const polytype &right); // merges a polygon into this one
   polytype &operator+=(const pointtype &right)
      {
      for (int i=0; i<points.size(); i++)
         points[i] += right;
      return *this;
      }
   polytype &operator=(const polytype &right)
      {
      points  = right.points;
      layer   = right.layer;
      comment = right.comment;
      node    = right.node;
      payload = right.payload;
      return *this;
      }
   bool IsNegative() const;                     // returns that this polygon represent negative space (vertice are in opposite order)
   bool IsInside(const pointtype &p) const;     // points along an edge are considered exterior   
   bool IsLeftFooted() const
      {
      return IsNegative();
      }
   bool IsBent() const;                     // return true if the polygon has non 90 edges(ie 45)
   void StairStepBends();
   float Area() const;
   bool IsIntersect(const polytype &poly) const;// returns whether the two polygons intersect each other, coincident edge count
   void ToggleFeet();
   mbbtype Mbb() const 
      {
      mbbtype mbb;
      for (int i=0; i<points.size(); i++)
         mbb.Add(points[i]);
      return mbb;
      }
   void Translate(const translationtype &t);
   void FetchHorizontalInsideIntervals(int y, array<intervaltype> &intervals) const;
//   void Paint(wxDC &dc, const translationtype &t, const PENtype &pen, bool ignoreComment=false) const;
   void OutToDLF(FILE *fptr, mbbtype &mbb, const int internalScale, const int units);
   void BoxConvert(array<boxtype> &boxes, float max_aspect_ratio, float max_step_ratio);
   void BoxConvert2(array<boxtype> &boxes) const;
   void BoxAdjust(array<boxtype> &boxes, float max_aspect_ratio, float max_step_ratio);
   void Cleanup();
   int HorizontalCrossCutDepth(const pointtype &p, bool total=false) const;
   int VerticalCrossCutDepth(const pointtype &p, bool total=false) const;
   void EliminateSlivers(int threshold);

   static void Merge(array<polytype *> &output, const array<const polytype *> &input, int operation);  // 0=union, 1=intersection, 2=xor
   static void OldMerge(array<polytype *> &output, const array<const polytype *> &input, int operation);  // 0=union, 1=intersection, 2=xor
   static void Subtract(array<polytype *> &output, const array<const polytype *> &input);  // input[0] will be subtracted by inputs[1..n]
   static void SmartMerge(array<polytype *> &polygons);  // only merges polygons on same layer, input should be deletable
   static void Display(const char *title, array<polytype *> polygons);  // Pops up a window showing polygons
   };





#endif
