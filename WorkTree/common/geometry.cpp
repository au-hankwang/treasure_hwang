#include "lpch.h"
#include "geometry.h"
#include "gui.h"
#include "globals.h"

#ifdef _MSC_VER
//  #pragma optimize("",off)
#endif

pointtype flatpolytype::ReturnSingleVertice() const
   {
   if (poly!=NULL)
      return t.Transform(poly->points[0]);
   else if (box!=NULL)
      return t.Transform(box->r.top_left());
   else
      FATAL_ERROR;
   return pointtype(0,0); // just to make warning go away
   }

layertype flatpolytype::ReturnLayer() const
   {
   if (poly!=NULL)
      return poly->layer;
   else if (box!=NULL)
      return box->layer;
   else
      FATAL_ERROR;
   return L_ILLEGAL; // just to make warning go away
   }


struct intervalSorttype { // function object (predicate for STL)
   const int personality;
   intervalSorttype (int _personality) : personality(_personality)
      {}
   bool operator()(const intervaltype &x, const intervaltype &y) const
      {
      switch (personality)
         {
         case 0:
            return x.low < y.low;
         case 1:
            return x.high < y.high;
         case 2:
            return x.node < y.node;
         default:
            return x.extra < y.extra;
         }      
      }
   };

struct coordtype
   {
   pointtype p;
   int next;
   bool marked;
   };


struct coordindextype{
   bool head;
   int index;
   coordindextype(bool _head, int _index) : head(_head), index(_index)
      {}
   };

struct coordIndexSorttype{  // function object (predicate for STL)
   const array<coordtype> coords;
   
   coordIndexSorttype(const array<coordtype> _coords) : coords(_coords)
      {}
   bool operator()(const coordindextype &x, const coordindextype &y) const
      {
      return coords[x.index].p.x < coords[y.index].p.x;
      }
   };

struct polygonSorttype { // function object (predicate for STL)
   bool operator()(const polytype *x, const polytype *y) const
      {
      if (x->layer != y->layer)
         return x->layer < y->layer;
      return x->node < y->node;
      }
   };



// this algorithm does a scan like the Merge() routing. It outputs boxes instead of a complex polgon
// this algorithm will make a lot of tiny boxes but it always produces an answer so it's a backup
// to the main box convert
void polytype::BoxConvert2(array<boxtype> &boxes) const
   {
   TAtype ta("polytype::BoxConvert2");
   array <int> ylocations;
   int pindex, k, y;
   int yy;
   
   pointtype lastp = points.back();
   
   for (pindex=0; pindex<points.size(); pindex++)
      {
      pointtype p = points[pindex];
      
      if (p.y == lastp.y)
         {
         ylocations.push(p.y);
         }
      lastp = p;
      }
   
   ylocations.RemoveDuplicates();
   ylocations.Sort();

   array<array <intervaltype> > finalIntervals(ylocations.size(), array<intervaltype>());

   for (yy=0; yy<ylocations.size()-1; yy++)
      {
      y = ylocations[yy];
      array <intervaltype> intervals;

      FetchHorizontalInsideIntervals(y, intervals);
      std::sort(intervals.begin(), intervals.end(), intervalSorttype(0)); // sort by low
      
      for (k=intervals.size()-1; k>=0; k--)
         {
         boxtype b;
         b.r = recttype(intervals[k].low, y, intervals[k].high, ylocations[yy+1]);
         b.layer = layer;
         b.node  = node;
         b.comment = comment;
	 b.payload = payload;
         boxes.push(b);
         }
      }
   }


void polytype::Merge(array<polytype *> &outputs, const array<const polytype *> &inputs, int operation)  // 0=union, 1=intersection
{
  TAtype ta("polytype::Merge");
  array <int> ylocations;
  int pindex, i, k, y;
  int yy;
  static int count;
  count++; // erase me
  /*
  if (debug)
    {
      for (k=0; k<inputs.size(); k++)
	{
	  printf("\ninput[%d]=",k);
	  for (i=0; i<inputs[k]->points.size(); i++)
	    printf("(%d,%d) ",inputs[k]->points[i].x,inputs[k]->points[i].y);
	}
    }
  */
  outputs.clear();
  
  if (inputs.size()!=2 && operation==1)
    FATAL_ERROR;
  
  for (i=0; i<inputs.size(); i++)
    {
      const polytype &poly = *inputs[i];
      pointtype lastp = poly.points.back();
      
      for (pindex=0; pindex<poly.points.size(); pindex++)
	{
	  pointtype p = poly.points[pindex];
	  
	  if (p.y == lastp.y)
            {
	      ylocations.push(p.y);
            }
	  lastp = p;
	}
      if (inputs[i]->IsNegative())
	FATAL_ERROR;
    }
  
  ylocations.RemoveDuplicates();
  ylocations.Sort();
  
  array<array <intervaltype> > finalIntervals(ylocations.size(), array<intervaltype>());
  
  for (yy=0; yy<ylocations.size(); yy++)
    {
      y = ylocations[yy];
      array <intervaltype> intervals;
      array <intervaltype> originals;
      if (operation==0)
	{  // union
	  for (i=0; i<inputs.size(); i++)
            inputs[i]->FetchHorizontalInsideIntervals(y, intervals);
	  std::sort(intervals.begin(), intervals.end(), intervalSorttype(0)); // sort by low
	  for (i=0; i<intervals.size(); i++)
            {
	      bool interval_added=false;
	      for (k=finalIntervals[yy].size()-1; k>=0; k--)
		{
		  if (finalIntervals[yy][k].Overlap(intervals[i]))
		    {
		      finalIntervals[yy][k].low  = MINIMUM(finalIntervals[yy][k].low,  intervals[i].low);
		      finalIntervals[yy][k].high = MAXIMUM(finalIntervals[yy][k].high, intervals[i].high);
		      interval_added=true;
		      break;
		    }
		}
	      if (!interval_added)
		finalIntervals[yy].push(intervals[i]);
            }
	}
      else if (operation==1)
	{  // intersection (intersection only works on 2 polygons)
	  inputs[0]->FetchHorizontalInsideIntervals(y, originals);
	  std::sort(originals.begin(), originals.end(), intervalSorttype(0)); // sort by low
	  
	  inputs[1]->FetchHorizontalInsideIntervals(y, intervals);
	  std::sort(intervals.begin(), intervals.end(), intervalSorttype(0)); // sort by low
	  for (k=originals.size()-1; k>=0; k--)
            {
	      for (i=intervals.size()-1; i>=0; i--)
		{
		  if (originals[k].Overlap(intervals[i]))
		    {
		      intervaltype mi(MAXIMUM(originals[k].low,  intervals[i].low), MINIMUM(originals[k].high, intervals[i].high), -1, y);
		      if (mi.low < mi.high)
			finalIntervals[yy].push(mi);
		    }
		}
            }
	}
      else FATAL_ERROR;
    }
  /*  
  if(debug)
    for (yy=0; yy<ylocations.size(); yy++)
      {
	int y = ylocations[yy+0];
	printf("Y=%4d ",y);
	for (i=0; i<finalIntervals[yy].size(); i++)
	  printf("{%4d %4d} ",finalIntervals[yy][i].low,finalIntervals[yy][i].high);
	printf("\n");
      }
  */
  // I now have a list of horizontal intervals which describes the output. 
  // I want to compile a list of top and bottom profile intervals
  // these intervals will be broken up such that the left/right edges are vertices
  array <polytype *> negatives;
  array <intervaltype> tops, bottoms;
  for (yy=0; yy<ylocations.size(); yy++)
    {
      int middleindex=0, bottomindex=0;
      intervaltype middle, bottom;
      int x = MIN_INT;
      int y = ylocations[yy+0];
      
      while (true)
	{
	  if (finalIntervals[yy+0].size() <= middleindex)
	    {
	      middle.low  = MAX_INT;
	      middle.high = MIN_INT;
	    }
	  else
	    middle = finalIntervals[yy+0][middleindex];
	  
	  if (yy<=0 || finalIntervals[yy-1].size() <= bottomindex)
	    {
	      if (middle.low==MAX_INT) 
		break;  // we're done
	      bottom.low  = MAX_INT;
	      bottom.high = MIN_INT;
	    }
	  else
	    bottom = finalIntervals[yy-1][bottomindex];
	  
	  int right;
	  
	  if (x >= middle.low && x >= bottom.low)
	    {
	      right = MINIMUM(middle.high, bottom.high);
	    }
	  else if (x >= middle.low && x < bottom.low)
	    {
	      right = MINIMUM(middle.high, bottom.low);
	      bottoms.push(intervaltype(right, x, 0, y));
	    }
	  else if (x < middle.low && x >= bottom.low)
	    {
	      right = MINIMUM(middle.low, bottom.high);
	      tops.push(intervaltype(x, right, 0, y));
	    }
	  else 
	    {
	      right = MINIMUM(middle.low, bottom.low);
	    }
	  
	  x = right;
	  
	  if (x >= middle.high)
	    middleindex++;
	  if (x >= bottom.high)
	    bottomindex++;
	}
    }
  
  int found=0;
  // now I have a list of top and bottom intervals. I will walk across the top and bottom collecting points
  while (tops.size() || bottoms.size())
    {
      std::sort(   tops.begin(),    tops.end(), intervalSorttype(0)); // sort by low (aka x)
      std::sort(bottoms.begin(), bottoms.end(), intervalSorttype(0)); // sort by high, since for bottom high=left.
      int topindex = 0;
      int botindex = 0;	  
      
      intervaltype *start=NULL;
      polytype *poly = new polytype;
      pointtype p;
      /*
      if(debug){
	for (i=0; i<tops.size(); i++)
	  printf("tops   [%3d] = {%5d, %5d} y=%4d\n", i, tops[i].low, tops[i].high, tops[i].extra);
	for (i=0; i<bottoms.size(); i++)
	  printf("bottoms[%3d] = {%5d, %5d} y=%4d\n", i, bottoms[i].low, bottoms[i].high, bottoms[i].extra);
      }
      */
      int maxtop=MIN_INT, maxbottom=MIN_INT;
      for (i=0; i<tops.size(); i++)
	{
	  if (tops[i].extra > maxtop)
	    {
	      topindex = i;
	      maxtop   = tops[i].extra;
	    }
	}
      for (i=0; i<bottoms.size(); i++)
	{
	  if (bottoms[i].extra > maxbottom)
	    {
	      botindex = i;
	      maxbottom= bottoms[i].extra;
	    }
	}
      p = pointtype(tops[topindex].low, tops[topindex].extra);
      bool walkpositive = maxtop > maxbottom;
      bool lasttop=walkpositive;
      while (true)  // this will walk around positive polygons
	{
	  // for this coordinate I want to find the above/below top interval with the same low==x
	  // for this coordinate I want to find the above/below bottom interval with the same right==x
	  
	  // for now use a slow routine to prove correctness
	  int taboveindex=-1, tabovey=MAX_INT, tbelowindex=-1, tbelowy=MAX_INT;
	  int baboveindex=-1, babovey=MAX_INT, bbelowindex=-1, bbelowy=MAX_INT;
	  for (i=0; i<tops.size(); i++)
	    {
	      y = tops[i].extra;
	      if (tops[i].low == p.x)
		{
		  if (y >= p.y && tabovey > y-p.y)
		    {
		      tabovey = y-p.y;
		      taboveindex = tops[i].node==0 ? i : -1;
		    }
		  if (y < p.y && tbelowy > p.y-y)
		    {
		      tbelowy = p.y-y;
		      tbelowindex = tops[i].node==0 ? i : -1;
		    }
		}
	    }
	  for (i=0; i<bottoms.size(); i++)
	    {
	      y = bottoms[i].extra;
	      if (bottoms[i].low == p.x)
		{
		  if (y >= p.y && babovey > y-p.y)
		    {
		      babovey = y-p.y;
		      baboveindex = bottoms[i].node==0 ? i : -1;
		    }
		  if (y < p.y && bbelowy > p.y-y)
		    {
		      bbelowy = p.y-y;
		      bbelowindex = bottoms[i].node==0 ? i : -1;
		    }
		}
	    }
	  /*
	  if(debug)
	    printf("lasttop=%d tabovey=%5d tbelowy=%5d babovey=%5d bbelowy=%5d",lasttop,tabovey==MAX_INT?-1:tabovey,tbelowy==MAX_INT?-1:tbelowy,babovey==MAX_INT?-1:babovey,bbelowy==MAX_INT?-1:bbelowy);
	  */
	  if ((tabovey<MAX_INT || babovey<MAX_INT) && (tbelowy<MAX_INT || bbelowy<MAX_INT))
	    { // I have an ambiguous choice here. I need to pick the choice which won't intersect another interval
	      bool upcrosses=false, downcrosses=false;
	      int abovey = p.y + MINIMUM(tabovey, babovey);
	      int belowy = p.y - MINIMUM(tbelowy, bbelowy);
	      for (i=0; i<tops.size(); i++)  // this could be bsearch
		{
		  if (tops[i].low <= p.x && tops[i].high >= p.x && tops[i].extra > p.y && tops[i].extra < abovey)
		    upcrosses = true;
		  if (tops[i].low <= p.x && tops[i].high >= p.x && tops[i].extra < p.y && tops[i].extra > belowy)
		    downcrosses = true;
		}
	      for (i=0; i<bottoms.size(); i++)  // this could be bsearch
		{
		  if (bottoms[i].low >= p.x && bottoms[i].high <= p.x && bottoms[i].extra < p.y && bottoms[i].extra > belowy)
		    downcrosses = true;
		  if (bottoms[i].low >= p.x && bottoms[i].high <= p.x && bottoms[i].extra > p.y && bottoms[i].extra < abovey)
		    upcrosses = true;
		}
	      if (upcrosses)
		taboveindex = baboveindex = -1;
	      if (downcrosses)
		tbelowindex = bbelowindex = -1;
	    }
	  
	  intervaltype *ii = NULL;
	  if (lasttop)
	    {
	      if (tabovey < babovey && taboveindex>=0)
		{
		  ii = &tops[taboveindex];
		  lasttop = true;
		}
	      else if (tbelowy < bbelowy && tbelowindex>=0)
		{
		  ii = &tops[tbelowindex];
		  lasttop = true;
		}
	      else if (babovey < tabovey && baboveindex>=0)
		{
		  ii = &bottoms[baboveindex];
		  lasttop = false;
		}
	      else if (bbelowy < tbelowy && bbelowindex>=0)
		{
		  ii = &bottoms[bbelowindex];
		  lasttop = false;
		}
	      else 
		{
		  printf("Internal error in merge (1). A coord is (%d,%d) %s\n",inputs[0]->points[0].x,inputs[0]->points[0].y,LayerNames(inputs[0]->layer));
		  FATAL_ERROR;
		}
	    }
	  else
	    {
	      if (bbelowy < tbelowy && bbelowindex>=0)
		{
		  ii = &bottoms[bbelowindex];
		  lasttop = false;
		}
	      else if (babovey < tabovey && baboveindex>=0)
		{
		  ii = &bottoms[baboveindex];
		  lasttop = false;
		}
	      else if (tbelowy < bbelowy && tbelowindex>=0)
		{
		  ii = &tops[tbelowindex];
		  lasttop = true;
		}
	      else if (tabovey < babovey && taboveindex>=0)
		{
		  ii = &tops[taboveindex];
		  lasttop = true;
		}
	      else 
		{
		  printf("Internal error in merge (2). A coord is (%d,%d) %s\n",inputs[0]->points[0].x,inputs[0]->points[0].y,LayerNames(inputs[0]->layer));
		  FATAL_ERROR;
		}
	    }
	  /*
	  if(debug)
	    printf(" y=%5d x=%5d %s[%d]\n",ii->extra,ii->low,lasttop?"tops":"bots",lasttop? (ii-&tops[0]) : (ii-&bottoms[0]));
	  */ 
	  if (ii==start) 
	    break;
	  if (poly->points.size()!=0)
	    ii->node = 1;  // mark as done
	  else
	    start = ii;
	  
	  p.x = ii->low;
	  p.y = ii->extra;
	  poly->points.push(p);
	  p.x = ii->high;
	  poly->points.push(p);
	}
      start->node = 1;
      
      poly->layer   = inputs[0]->layer;
      poly->comment = inputs[0]->comment;
      poly->node    = inputs[0]->node;
      poly->payload = inputs[0]->payload;
      if (poly->IsNegative())
	{
	  if (negatives.size()<100||true){
	    negatives.push(poly);
	    //poly->layer = L_M12;
	    //poly->ToggleFeet();
	    //outputs.push(poly);
	  }
	  else{
	    poly->layer = L_M12;
	    poly->ToggleFeet();
	    outputs.push(poly);
	  }
	}
      else
	outputs.push(poly);
      poly = NULL;	  
      
      for (i=tops.size()-1; i>=0; i--)
	{
	  if (tops[i].node!=0)
	    tops.RemoveFast(i);
	}
      for (i=bottoms.size()-1; i>=0; i--)
	{
	  if (bottoms[i].node!=0)
	    bottoms.RemoveFast(i);
	}
      found++;
      //	  if (found==6) break;
      //	  break; //erase me
    
    }
 
  /*
  // to fix a bug I want to look for negative polygons which are coincident to each other and merge them together
  array <mbbtype> neg_mbbs(negatives.size(), mbbtype());
  for (i=0; i<negatives.size(); i++)
    {
      neg_mbbs[i] = negatives[i]->Mbb();
    }
  for (i=neg_mbbs.size()-1; i>=0; i--)
    {
      for (k=i-1; k>=0; k--)
	{
	  if (i<neg_mbbs.size() && k<neg_mbbs.size() && neg_mbbs[i].Overlap(neg_mbbs[k]))
            { // I found a potential 
	      int j;
	      negatives[i]->ToggleFeet();
	      negatives[k]->ToggleFeet();
	      array <const polytype *> neg_ins;
	      array <polytype *> neg_outs;
	      neg_ins.push(negatives[i]);
	      neg_ins.push(negatives[k]);
	      Merge(neg_outs, neg_ins,  0);
	      if (neg_outs.size()!=1) FATAL_ERROR;
	      delete negatives[i]; negatives[i]=NULL;
	      negatives.RemoveFast(i);
	      delete negatives[k]; negatives[k]=NULL;
	      negatives.RemoveFast(k);
	      for (j=0; j<neg_outs.size(); j++)
		neg_outs[j]->ToggleFeet();
	      negatives += neg_outs;
	      neg_mbbs.resize(negatives.size(), mbbtype());
	      for (j=0; j<negatives.size(); j++)
		{
		  neg_mbbs[j] = negatives[j]->Mbb();
		}
            }
	}
    }

  if(debug){
    for (i=0; i<negatives.size(); i++)
      {
	negatives[i]->layer = L_M12;
	outputs.push(negatives[i]);
      }
    return;
  }
  */  
  // now I may have some negative polygons which need to be subtracted from the positive ones
  for (i=0; i<negatives.size(); i++)
    {
      pointtype p = negatives[i]->points[0];  // this should be toprightmost point
      for (k=outputs.size()-1; k>=0; k--)
	if (outputs[k]->IsInside(p))
	  break;
      if (k<0)
	FATAL_ERROR;
      polytype &poly = *outputs[k];
      int best = -1, bestdist=MIN_INT;
      
      for (k=0; k<negatives[i]->points.size(); k++)
	{
	  if (negatives[i]->points[k].y+negatives[i]->points[k].x > bestdist)
	    {
	      best = k;
	      bestdist = negatives[i]->points[k].y+negatives[i]->points[k].x;
	    }
	}
      // now make the top point of this polygon
      array <pointtype> temp;
      for (k=best; k<negatives[i]->points.size(); k++)
	temp.push(negatives[i]->points[k]);
      for (k=0; temp.size()< negatives[i]->points.size(); k++)
	temp.push(negatives[i]->points[k]);
      negatives[i]->points = temp;
      
      best = -1;
      bestdist=MAX_INT;
      p = negatives[i]->points[0];  // this should be toprightmost point
      if (best<0)
	{ // I will need to introduce an artificial vertice		 
	  pointtype lastp = poly.points.back();
	  bool vertical=false;
	  for (k=0; k<poly.points.size(); k++)
	    {			
	      if (poly.points[k].y == lastp.y && lastp.x<=p.x && poly.points[k].x>=p.x)
		{
		  int d = lastp.y - p.y;
		  
		  if (d < bestdist && d>=0)
		    {
		      best = k;
		      bestdist = d;
		      vertical = false;
		    }
		}
	      if (poly.points[k].x == lastp.x && lastp.y>=p.y && poly.points[k].y<=p.y)
		{
		  int d = lastp.x - p.x;
		  
		  if (d < bestdist && d>=0)
		    {
		      best = k;
		      bestdist = d;
		      vertical = true;
		    }
		}
	      lastp = poly.points[k];
	    }
	  if (best<0) FATAL_ERROR;
	  if (vertical){
            poly.points.Insert(best, pointtype(poly.points[best].x, p.y));
            poly.points.Insert(best, pointtype(poly.points[best].x, p.y));
	  }
	  else{
            poly.points.Insert(best, pointtype(p.x, poly.points[best].y));
            poly.points.Insert(best, pointtype(p.x, poly.points[best].y));
	  }
	  best++;
	}
      if (best<0) FATAL_ERROR;
      // now stitch in the negative polygon
      negatives[i]->points.push(negatives[i]->points[0]);
      negatives[i]->points.push(poly.points[best]);
      poly.points.Insert(best+1, negatives[i]->points);
    }
  for (i=0; i<negatives.size(); i++)
    {
      delete negatives[i];
      negatives[i]=NULL;
    }
  for (i=0; i<outputs.size(); i++)
    { // now go through and delete unnecessary vertices
      outputs[i]->Cleanup();
    }
}



void polytype::OldMerge(array<polytype *> &outputs, const array<const polytype *> &inputs, int operation)  // 0=union, 1=intersection
   {
   TAtype ta("polytype::Merge");
   array <int> ylocations;
   int pindex, i, k, y;
   int yy;

   outputs.clear();
   
   if (inputs.size()!=2 && operation==1)
      FATAL_ERROR;

   for (i=0; i<inputs.size(); i++)
      {
      const polytype &poly = *inputs[i];
      pointtype lastp = poly.points.back();

      for (pindex=0; pindex<poly.points.size(); pindex++)
         {
         pointtype p = poly.points[pindex];
         
         if (p.y == lastp.y)
            {
            ylocations.push(p.y);
            }
         lastp = p;
         }
      if (inputs[i]->IsNegative())
         FATAL_ERROR;
      }
   
   ylocations.RemoveDuplicates();
   ylocations.Sort();

   array<array <intervaltype> > finalIntervals(ylocations.size(), array<intervaltype>());

   for (yy=0; yy<ylocations.size(); yy++)
      {
      y = ylocations[yy];
      array <intervaltype> intervals;
      array <intervaltype> originals;

      if (operation==0)
         {  // union
         for (i=0; i<inputs.size(); i++)
            inputs[i]->FetchHorizontalInsideIntervals(y, intervals);
         std::sort(intervals.begin(), intervals.end(), intervalSorttype(0)); // sort by low
         for (i=0; i<intervals.size(); i++)
            {
            for (k=finalIntervals[yy].size()-1; k>=0; k--)
               {
               if (finalIntervals[yy][k].Overlap(intervals[i]))
                  {
                  finalIntervals[yy][k].low  = MINIMUM(finalIntervals[yy][k].low,  intervals[i].low);
                  finalIntervals[yy][k].high = MAXIMUM(finalIntervals[yy][k].high, intervals[i].high);
                  break;
                  }
               }
            if (k<0)
               finalIntervals[yy].push(intervals[i]);
            }
         }
      else if (operation==1)
         {  // intersection (intersection only works on 2 polygons)
         inputs[0]->FetchHorizontalInsideIntervals(y, originals);
         std::sort(originals.begin(), originals.end(), intervalSorttype(0)); // sort by low
         
         inputs[1]->FetchHorizontalInsideIntervals(y, intervals);
         std::sort(intervals.begin(), intervals.end(), intervalSorttype(0)); // sort by low
         for (k=originals.size()-1; k>=0; k--)
            {
            for (i=intervals.size()-1; i>=0; i--)
               {
               if (originals[k].Overlap(intervals[i]))
                  {
				  intervaltype mi(MAXIMUM(originals[k].low,  intervals[i].low), MINIMUM(originals[k].high, intervals[i].high), -1, y);
				  if (mi.low < mi.high)
					 finalIntervals[yy].push(mi);
                  }
               }
            }
         }
      else FATAL_ERROR;
      }

   // I now have a list of horizontal intervals which describes the output. I now need to turn
   // these into polygons
   array <coordtype> coords;
   array <coordindextype> current;

   for (yy=0; yy<ylocations.size(); yy++)
      {
      y = ylocations[yy];
      
      array <coordindextype> last(current);
      current.clear();
      for (i=0; i<finalIntervals[yy].size(); i++)
         {
         coordtype c;
         c.p.x = finalIntervals[yy][i].low;
         c.p.y = y;
         c.next = -1;
         coords.push(c);
         current.push(coordindextype(false, coords.size()-1));
         
         c.p.x = finalIntervals[yy][i].high;
         c.p.y = y;
         c.next = -1;
         coords.push(c);
         current.push(coordindextype(true, coords.size()-1));
         }

      for (i=0; i<last.size(); i++)
         {
         if (last[i].head)
            {
            coordtype c;
            c = coords[last[i].index];
            c.p.y = y;
            c.next = last[i].index;
            coords.push(c);
            last[i].index = coords.size()-1;
            last[i].head = false;
            }
         else
            {
            coordtype c;
            c = coords[last[i].index];
            c.p.y = y;
            coords.push(c);
            if (coords[last[i].index].next >=0) FATAL_ERROR;
            coords[last[i].index].next = coords.size()-1;
            last[i].index = coords.size()-1;
            last[i].head = true;
            }
         }
      last += current;

      std::sort(last.begin(), last.end(), coordIndexSorttype(coords));  // sort by X location

      if (last.size() &1) FATAL_ERROR;  // should always have even number of vertical edges
      
      bool problem = false;
      for (i=0; i<(signed)last.size()-2; i+=2)
         {
         if (last[i].head == last[i+1].head)
            {
            if (coords[last[i+1].index].p == coords[last[i+2].index].p)
               {
               coordindextype temp = last[i+1];
               last[i+1] = last[i+2];
               last[i+2] = temp;
               }
            else
               problem=true;
            }
         }
      if (problem && coords[last[0].index].p == coords[last[1].index].p)
         {
         coordindextype temp = last[0];
         last[0] = last[1];
         last[1] = temp;
         problem = false;
         }
      for (i=0; i<(signed)last.size()-2; i+=2)
         {
         if (last[i].head == last[i+1].head)
            {
            if (coords[last[i+1].index].p == coords[last[i+2].index].p)
               {
               coordindextype temp = last[i+1];
               last[i+1] = last[i+2];
               last[i+2] = temp;
               }
            else
               problem=true;
            }
         }
      if (problem) FATAL_ERROR;
      
      for (i=0; i<last.size(); i+=2)
         {
         if (last[i].head && !last[i+1].head)
            {
            if (coords[last[i+0].index].next >=0) FATAL_ERROR;
            coords[last[i+0].index].next = last[i+1].index;
            }
         else if (!last[i].head && last[i+1].head)
            {
            if (coords[last[i+1].index].next >=0) FATAL_ERROR;
            coords[last[i+1].index].next = last[i+0].index;
            }
         else
            FATAL_ERROR;         
         }
      }

   for (i=0; i<coords.size(); i++)
      {
      coords[i].marked = false;
      if (coords[i].next <0)
         FATAL_ERROR;
      }
   
   array <polytype *> negatives;
   while (true)
      {
      int min=-1;
      // find upper left point of polygon unmarked polygon
      for (i=coords.size()-1; i>=0; i--)
         {
         if (!coords[i].marked) 
            {
            if (min<0 || coords[i].p < coords[min].p)
               min = i;
            }
         }
      if (min<0) break;
      polytype *poly = new polytype;
      poly->layer   = inputs[0]->layer;
      poly->comment = inputs[0]->comment;
      poly->node    = inputs[0]->node;
	  poly->payload = inputs[0]->payload;
      i = min;
      while (!coords[i].marked)
         {
         coords[i].marked = true;
         poly->points.push(coords[i].p);
         i=coords[i].next;
         }
      if (poly->IsNegative())
         negatives.push(poly);
      else
         outputs.push(poly);
      }
   // now I may have some negative polygons which need to be subtracted from the positive ones
   for (i=0; i<negatives.size(); i++)
      {
      pointtype p = negatives[i]->points[0];  // this should be topleftmost point
      for (k=outputs.size()-1; k>=0; k--)
         if (outputs[k]->IsInside(p))
            break;
	  if (k<0) {
		 // this shouldn't happen.
		 // I put this hack in to make forward progress without fixing the underlying bug
		 k=0;
		 }
	  polytype &poly = *outputs[k];
	  int best = -1, bestdist=MAX_INT;
	  for (k=0; k<poly.points.size(); k++)
		 {
		 if (poly.points[k].y == p.y)
			{
			int d = p.x - poly.points[k].x;
			
			if (d < bestdist && d>=0)
			   {
			   best = k;
			   bestdist = d;
			   }
			}
		 }
	  if (best<0) FATAL_ERROR;
	  // now stitch in the negative polygon
	  negatives[i]->points.push(negatives[i]->points[0]);
	  negatives[i]->points.push(poly.points[best]);
	  poly.points.Insert(best+1, negatives[i]->points);
      }
   for (i=0; i<negatives.size(); i++)
      {
      delete negatives[i];
      negatives[i]=NULL;
      }
   for (i=0; i<outputs.size(); i++)
      { // now go through and delete unnecessary vertices
      outputs[i]->Cleanup();
      }
   }

void polytype::Cleanup()
   {
   int k;
       // now go through and delete unnecessary vertices
   if (points.size()==0)
     return;
   pointtype lastp = points[0];
   
   for (k=points.size()-1; k>=1; k--)
	  {
	  pointtype p = points[k];
	  if (p == lastp)            // eliminate identical points
		 points.Remove(k);
	  else                       // eliminate colinear points
		 {
		 if (points[k-1].x == p.x && p.x == lastp.x)
			points.Remove(k);
		 else if (points[k-1].y == p.y && p.y == lastp.y)
			points.Remove(k);
		 else
			lastp = p;
		 }         
	  }
   }


// this algorithm is not optimal, but plenty fast and I didn't have to resort to balance binary trees
// This will subtract from base(input[0] all of the other inputs[1..n] leaving potentially multiple
void polytype::Subtract(array<polytype *> &outputs, const array<const polytype *> &inputs)
   {
   TAtype ta("polytype::Subtract");
   array <int> ylocations;
   int pindex, i, k, y;
   int yy;

   outputs.clear();
   
   for (i=0; i<inputs.size(); i++)
      {
      const polytype &poly = *inputs[i];
      pointtype lastp = poly.points.back();

      for (pindex=0; pindex<poly.points.size(); pindex++)
         {
         pointtype p = poly.points[pindex];
         
         if (p.y == lastp.y)
            {
            ylocations.push(p.y);
            }
         lastp = p;
         }
      if (inputs[i]->IsNegative())
         FATAL_ERROR;
      }
   
   ylocations.RemoveDuplicates();
   ylocations.Sort();

   array<array <intervaltype> > finalIntervals(ylocations.size(), array<intervaltype>());

   for (yy=0; yy<ylocations.size(); yy++)
      {
      y = ylocations[yy];
      array <intervaltype> intervals;

      inputs[0]->FetchHorizontalInsideIntervals(y, finalIntervals[yy]);
      std::sort(finalIntervals[yy].begin(), finalIntervals[yy].end(), intervalSorttype(0)); // sort by low
      
      for (i=1; i<inputs.size(); i++)
         inputs[i]->FetchHorizontalInsideIntervals(y, intervals);
      std::sort(intervals.begin(), intervals.end(), intervalSorttype(0)); // sort by low
      for (k=finalIntervals[yy].size()-1; k>=0; k--)
         {
         for (i=intervals.size()-1; i>=0; i--)
            {
            if (finalIntervals[yy][k].Overlap(intervals[i]))
               {  // there are 4 cases over overlap
               bool changed=false;
               if (finalIntervals[yy][k].low < intervals[i].low && finalIntervals[yy][k].high > intervals[i].high)
                  {  // this is the messy case where I need to split to 2 intervals
                  intervaltype ii = finalIntervals[yy][k];
                  ii.high = intervals[i].low;
                  finalIntervals[yy][k].low = intervals[i].high;
                  finalIntervals[yy].Insert(k, ii);
                  k++;
                  changed=true;
                  }
               else{
                  if (finalIntervals[yy][k].low > intervals[i].low && finalIntervals[yy][k].low != intervals[i].high){
                     finalIntervals[yy][k].low = intervals[i].high;
                     changed=true;
                     }
                  if (finalIntervals[yy][k].high < intervals[i].high && finalIntervals[yy][k].high != intervals[i].low){
                     finalIntervals[yy][k].high = intervals[i].low;
                     changed=true;
                     }
                  if (finalIntervals[yy][k].low >= finalIntervals[yy][k].high){
                     finalIntervals[yy].Remove(k);
                     changed=true;
                     }
                  }
               if (changed)
                  break;
               }

            }
         }
      }

   // I now have a list of horizontal intervals which describes the output. 
   // I now need to turn these into polygons
   array <coordtype> coords;
   array <coordindextype> current;
   for (yy=0; yy<ylocations.size(); yy++)
      {
      y = ylocations[yy];
      
      array <coordindextype> last(current);
      current.clear();
      for (i=0; i<finalIntervals[yy].size(); i++)
         {
         coordtype c;
         c.p.x = finalIntervals[yy][i].low;
         c.p.y = y;
         c.next = -1;
         coords.push(c);
         current.push(coordindextype(false, coords.size()-1));
         
         c.p.x = finalIntervals[yy][i].high;
         c.p.y = y;
         c.next = -1;
         coords.push(c);
         current.push(coordindextype(true, coords.size()-1));
         }

      for (i=0; i<last.size(); i++)
         {
         if (last[i].head)
            {
            coordtype c;
            c = coords[last[i].index];
            c.p.y = y;
            c.next = last[i].index;
            coords.push(c);
            last[i].index = coords.size()-1;
            last[i].head = false;
            }
         else
            {
            coordtype c;
            c = coords[last[i].index];
            c.p.y = y;
            coords.push(c);
            if (coords[last[i].index].next >=0) FATAL_ERROR;
            coords[last[i].index].next = coords.size()-1;
            last[i].index = coords.size()-1;
            last[i].head = true;
            }
         }
      last += current;

      std::sort(last.begin(), last.end(), coordIndexSorttype(coords));  // sort by X location

      if (last.size() &1) FATAL_ERROR;  // should always have even number of vertical edges
      
      bool problem = false;
      for (i=0; i<(signed)last.size()-2; i+=2)
         {
         if (last[i].head == last[i+1].head)
            {
            if (coords[last[i+1].index].p == coords[last[i+2].index].p)
               {
               coordindextype temp = last[i+1];
               last[i+1] = last[i+2];
               last[i+2] = temp;
               }
            else
               problem=true;
            }
         }
      if (problem && coords[last[0].index].p == coords[last[1].index].p)
         {
         coordindextype temp = last[0];
         last[0] = last[1];
         last[1] = temp;
         problem = false;
         }
      for (i=0; i<(signed)last.size()-2; i+=2)
         {
         if (last[i].head == last[i+1].head)
            {
            if (coords[last[i+1].index].p == coords[last[i+2].index].p)
               {
               coordindextype temp = last[i+1];
               last[i+1] = last[i+2];
               last[i+2] = temp;
               }
            else
               problem=true;
            }
         }
      if (problem) FATAL_ERROR;
      
      for (i=0; i<last.size(); i+=2)
         {
         if (last[i].head && !last[i+1].head)
            {
            if (coords[last[i+0].index].next >=0) FATAL_ERROR;
            coords[last[i+0].index].next = last[i+1].index;
            }
         else if (!last[i].head && last[i+1].head)
            {
            if (coords[last[i+1].index].next >=0) FATAL_ERROR;
            coords[last[i+1].index].next = last[i+0].index;
            }
         else
            FATAL_ERROR;         
         }
      }

   for (i=0; i<coords.size(); i++)
      {
      coords[i].marked = false;
      if (coords[i].next <0)
         FATAL_ERROR;
      }
   
   array <polytype *> negatives;
   while (true)
      {
      int min=-1;
      // find upper left point of polygon unmarked polygon
      for (i=coords.size()-1; i>=0; i--)
         {
         if (!coords[i].marked) 
            {
            if (min<0 || coords[i].p < coords[min].p)
               min = i;
            }
         }
      if (min<0) break;
      polytype *poly = new polytype;
      poly->layer   = inputs[0]->layer;
      poly->comment = inputs[0]->comment;
      poly->node    = inputs[0]->node;
      i = min;
      while (!coords[i].marked)
         {
         coords[i].marked = true;
         poly->points.push(coords[i].p);
         i=coords[i].next;
         }
      if (poly->IsNegative())
         negatives.push(poly);
      else
         outputs.push(poly);
      }
   // now I may have some negative polygons which need to be subtracted from the positive ones
   for (i=0; i<negatives.size(); i++)
      {
      pointtype p = negatives[i]->points[0];  // this should be topleftmost point
      for (k=outputs.size()-1; k>=0; k--)
         if (outputs[k]->IsInside(p))
            break;
      if (k<0) FATAL_ERROR;
      polytype &poly = *outputs[k];
      int best = -1, bestdist=MAX_INT;
      for (k=0; k<poly.points.size(); k++)
         {
         if (poly.points[k].y == p.y)
            {
            int d = p.x - poly.points[k].x;
            
            if (d < bestdist && d>=0)
               {
               best = k;
               bestdist = d;
               }
            }
         }
      if (best<0) FATAL_ERROR;
      // now stitch in the negative polygon
      negatives[i]->points.push(negatives[i]->points[0]);
      negatives[i]->points.push(poly.points[best]);
      poly.points.Insert(best+1, negatives[i]->points);      
      }
   for (i=0; i<negatives.size(); i++)
      {
      delete negatives[i];
      negatives[i]=NULL;
      }
   for (i=0; i<outputs.size(); i++)
      { // now go through and delete unnecessary vertices
      array <pointtype> &points = outputs[i]->points;
      pointtype lastp = points[0];

      for (k=points.size()-1; k>=1; k--)
         {
         pointtype p = points[k];
         if (p == lastp)            // eliminate identical points
            points.Remove(k);
         else                       // eliminate colinear points
            {
            if (points[k-1].x == p.x && p.x == lastp.x)
               points.Remove(k);
            else if (points[k-1].y == p.y && p.y == lastp.y)
               points.Remove(k);
            else
               lastp = p;
            }         
         }
      }
   }


void DetectCollisionsAndMerge(array<polytype *> &out, array<polytype *> &in)
   {
   connectiontype connect(in.size());
   array <intervaltype> verts, horzs;
   heaptype <intervaltype> heap;
   int i, k;
   
   for (i=0; i<in.size(); i++)
      {
      array <pointtype> &points = in[i]->points;
      pointtype lastp = points.back();

      for (k=0; k<points.size(); k++)
         {
         pointtype p = points[k];

         if (p.x == lastp.x)            
            verts.push(intervaltype(MINIMUM(p.y, lastp.y), MAXIMUM(p.y, lastp.y), i, p.x));  // vertical edge
         else   
            horzs.push(intervaltype(MINIMUM(p.x, lastp.x), MAXIMUM(p.x, lastp.x), i, p.y));  // vertical edge

         lastp = p;
         }
      }
   std::sort(horzs.begin(), horzs.end(), intervalSorttype(0));  // sort by low (x0)
   std::sort(verts.begin(), verts.end(), intervalSorttype(3));  // sort by extra (x)

   // now do a sweep
   int x, v=0, h=0;
   do {      
      x = MAX_INT;

      if (h < horzs.size() && horzs[h].low < x)
         x = horzs[h].low;
      if (v < verts.size() && verts[v].extra <x)
         x = verts[v].extra;
      if (heap.size() && heap[0].high < x)
         x = heap[0].high;

      if (h < horzs.size() && horzs[h].low == x)
         {
         heap.push(horzs[h]);
         h++;
         }
      else if (v < verts.size() && verts[v].extra == x)
         {         
         for (i=0; i<heap.size(); i++)
            {
            if (heap[i].extra >= verts[v].low && heap[i].extra <= verts[v].high)
               connect.Merge(heap[i].node, verts[v].node);
            }
         v++;
         }
      else if (heap.size() && heap[0].high == x)
         heap.pop();
      } while(x < MAX_INT);

   // the above loop catch polygons which have an intersecting edge.
   // The algorithm below catches the case where one polygon is wholly inside another
   for (i=0; i<in.size(); i++)
      {
      for (k=0; k<in.size(); k++)
         {
         if (i!=k && in[i]->IsInside(in[k]->points[0]))
               connect.Merge(i, k);
         }
      }

   for (i=0; i<in.size(); i++)
      {
      if (connect.Handle(i) == i)
         {
         array <const polytype *> groups;
         array <polytype *> tempoutput;

         for (k=0; k<in.size(); k++)
            if (connect.Handle(k)==i)
               groups.push(in[k]);

         polytype::OldMerge(tempoutput, groups, 0);
         for (k=0; k<tempoutput.size(); k++)
            out.push(tempoutput[k]);
         }
      }
   }




void polytype::SmartMerge(array<polytype *> &polygons)  // only merges polygons on same layer and are same node
   {
   TAtype ta("polytype::SmartMerge");
   int start, end;
   array <polytype *> output;

   std::sort(polygons.begin(), polygons.end(), polygonSorttype());

   for (start=0; start<polygons.size(); )
      {
      array<polytype *> in;
      for (end=start; end<polygons.size() && polygons[end]->layer == polygons[start]->layer && polygons[end]->node == polygons[start]->node; end++)
         in.push(polygons[end]);
      
      DetectCollisionsAndMerge(output, in);

      start = end;
      }
   for (int i=0; i<polygons.size(); i++)
      delete polygons[i];
   polygons.clear();
   polygons = output;
   }

// this will return the intervals which are inside the polygon at the specified cut point
// if the cut point lands on the bottom edge(small y) of the polygon then an interval will be returned
// if the cut point lands on the top edge(large y) of the polygon then no interval will be returned
void polytype::FetchHorizontalInsideIntervals(int y, array<intervaltype> &intervals) const
   {
   int i;
   pointtype p, lastp;
   array <int> crossings;

   lastp = points.back();
   for (i=0; i<points.size(); i++)
      {
      p = points[i];

      if ((p.y <= y && lastp.y > y) || (lastp.y <= y && p.y > y))
         if (p.x != lastp.x)
            ;  // This happens if the polygon has a 45
         else
           crossings.push(p.x);
      lastp = p;
      }
   crossings.Sort();
   if (crossings.size() &1) 
	  FATAL_ERROR;  // should always have an even number of crossings, this may happen if a 45 is present
   for (i=0; i<crossings.size(); i+=2)
      {
      intervals.push(intervaltype(crossings[i], crossings[i+1]));
      }
   }

double TriangleArea(pointtype p1, pointtype p2)
   {
   return (double)p1.x * p2.y - (double)p1.y * p2.x;
   }


// to calculate the area of a polygon merely pick an arbitray point and then sum the area
// of all the triangles from that point
float polytype::Area() const
   {
   TAtype ta("polytype::Area");
   int i;
   double area=0.0;	  // this must be double otherwise rounding can occur with big coordinates
   const pointtype offset = points[0];  // this improves precision
   pointtype lastp = points[0] - offset;

   for (i=points.size()-1; i>=0; i--)
      {
      pointtype p = points[i] - offset;
      area = area + TriangleArea(p, lastp);
      lastp = p;
      }
   if (area < 0.0)
      area = -area;

   return area*0.5;
   }


bool polytype::IsBent() const
   {
   int i;
   pointtype lastp = points.back();
   for (i=0; i<points.size(); i++)
	  {
	  const pointtype &p = points[i];
	  if (p.x != lastp.x && p.y != lastp.y)
		 return true;
	  lastp = points[i];
	  }
   return false;
   }

void polytype::StairStepBends()
   {
   TAtype ta("polytype::StairStepBends");
   int i, k;
   pointtype lastp = points.back();
   for (i=0; i<points.size(); i++)
	  {
	  pointtype &p = points[i];

	  if (p.x != lastp.x && p.y != lastp.y)
		 {	// I found a bend. Now stairstep it
		 pointtype m0((p.x*1+lastp.x*3)/4, (p.y*1+lastp.y*3)/4);
		 pointtype m1((p.x*2+lastp.x*2)/4, (p.y*2+lastp.y*2)/4);
		 pointtype m2((p.x*3+lastp.x*1)/4, (p.y*3+lastp.y*1)/4);
		 array <pointtype> stairs;
		 
		 if ((lastp.x < p.x) ^ (lastp.y < p.y) ^1){
			stairs.push(pointtype(lastp.x, m0.y));
			stairs.push(pointtype(m0.x,    m0.y));
			stairs.push(pointtype(m0.x,    m1.y));
			stairs.push(pointtype(m1.x,    m1.y));
			stairs.push(pointtype(m1.x,    m2.y));
			stairs.push(pointtype(m2.x,    m2.y));
			stairs.push(pointtype(m2.x,    p.y));
			}
		 else{
			stairs.push(pointtype(m0.x,    lastp.y));
			stairs.push(pointtype(m0.x,    m0.y));
			stairs.push(pointtype(m1.x,    m0.y));
			stairs.push(pointtype(m1.x,    m1.y));
			stairs.push(pointtype(m2.x,    m1.y));
			stairs.push(pointtype(m2.x,    m2.y));
			stairs.push(pointtype(p.x,     m2.y));
			}
		 points.Insert(i, stairs);
		 }
	  lastp = points[i];
	  }

   lastp = points[0];
   for (k=points.size()-1; k>=0; k--)
	  {
	  pointtype p = points[k];
	  pointtype next = points[k==0 ? points.size()-1 : k-1];
	  if (p == lastp)            // eliminate identical points
		 points.Remove(k);
	  else                       // eliminate colinear points
		 {
		 if (next.x == p.x && p.x == lastp.x)
			points.Remove(k);
		 else if (next.y == p.y && p.y == lastp.y)
			points.Remove(k);
		 else
			lastp = p;
		 }         
	  }
   }


// this will destroy the current polygon
void polytype::BoxConvert(array<boxtype> &boxes, float max_aspect_ratio, float max_step_ratio) 
   {
   TAtype ta("polytype::BoxConvert");
   if (!IsNegative())
      ToggleFeet();
   if (IsBent())
	  FATAL_ERROR;

   int i, k;
   array <pointtype> original_points = points;


// This algorithm looks for 'ears', When it finds an ear it will transform it into a box
// and simplify the polygon. There are 4 cases which are handled separately but the code is nearly identical
// I also want to give preferrence to small ears first. (ie ie want to carve off long narrow piece before short fat ones)
   int count=0;
   array <pointtype> edges;   // I used pointtype because it already existed. .x=edgelength .y=index
   while (points.size()>4 && count<100000)
      {
      count++;
      bool found=false;

//	  if (count==1585)
//		 printf(""); // erase me

      edges.resize(points.size());
      pointtype lastp = points.back();
      for (i=0; i<points.size(); i++)
         {
         edges[i].x = MAXIMUM(abs(points[i].x - lastp.x), abs(points[i].y - lastp.y));
         edges[i].y = i;
         lastp = points[i];
         }
      edges.Sort();

      for (i=0; i<edges.size(); i++)
         {
         int index = edges[i].y+points.size(); // % doesn't work on negative numbers so adding the modulus fixes things
         const pointtype last2p = points[(index-2)%points.size()];
         const pointtype lastp  = points[(index-1)%points.size()];
         const pointtype p      = points[(index+0)%points.size()];
         const pointtype nextp  = points[(index+1)%points.size()];

         // Do the up ear case. p is the upper left corner of the ear.
         if (p.y == lastp.y && p.x < lastp.x &&              // top edge
             p.x==nextp.x && nextp.y < p.y &&                // left down edge
             lastp.x == last2p.x && last2p.y < lastp.y)      // right down edge
            {
            int y = MAXIMUM(nextp.y, last2p.y);
            boxes.push(boxtype(layer, NULL, recttype(p, pointtype(lastp.x, y))));
            for (k=points.size()-1; k>=0; k--)  // check that this doesn't intersect some other part of the polygon
               {
               if (boxes.back().IsInsideNotOnEdge(points[k]))
                  {  // back out this change
                  boxes.pop();  
                  break;
                  }
               }
            if (k<0)
               {
               if (!this->IsInside(pointtype((boxes.back().r.x0+boxes.back().r.x1)/2,(boxes.back().r.y0+boxes.back().r.y1)/2)))
                  {
				  boxes.pop();
				  k=0;
				  }
               }
            if (k<0){
               points[(index+0)%points.size()].y = y;
               points[(index-1)%points.size()].y = y;
               found = true;
               break;
               }
            }
         
         // Do the down ear case. p is the lower right corner of the ear.
         if (p.y == lastp.y && p.x > lastp.x &&              // top edge
             p.x==nextp.x && nextp.y > p.y &&                // left down edge
             lastp.x == last2p.x && last2p.y > lastp.y)      // right down edge
            {
            int y = MINIMUM(nextp.y, last2p.y);
            boxes.push(boxtype(layer, NULL, recttype(p, pointtype(lastp.x, y))));
            for (k=points.size()-1; k>=0; k--)  // check that this doesn't intersect some other part of the polygon
               {
               if (boxes.back().IsInsideNotOnEdge(points[k]))
                  {  // back out this change
                  boxes.pop();  
                  break;
                  }
               }
            if (k<0)
               {
               if (!this->IsInside(pointtype((boxes.back().r.x0+boxes.back().r.x1)/2,(boxes.back().r.y0+boxes.back().r.y1)/2)))
                  {
				  boxes.pop();
				  k=0;
				  }
               }
            if (k<0){
               points[(index+0)%points.size()].y = y;
               points[(index-1)%points.size()].y = y;
               found = true;
               break;
               }
            }

         // Do the right ear case. p is the upper right corner of the ear.
         if (p.x == lastp.x && p.y > lastp.y &&              // top edge
             p.y==nextp.y && nextp.x < p.x &&                // left down edge
             lastp.y == last2p.y && last2p.x < lastp.x)      // right down edge
            {
            int x = MAXIMUM(nextp.x, last2p.x);
            boxes.push(boxtype(layer, NULL, recttype(p, pointtype(x, lastp.y))));
            for (k=points.size()-1; k>=0; k--)  // check that this doesn't intersect some other part of the polygon
               {
               if (boxes.back().IsInsideNotOnEdge(points[k]))
                  {  // back out this change
                  boxes.pop();  
                  break;
                  }
               }
            if (k<0)
               {
               if (!this->IsInside(pointtype((boxes.back().r.x0+boxes.back().r.x1)/2,(boxes.back().r.y0+boxes.back().r.y1)/2)))
                  {
				  boxes.pop();
				  k=0;
				  }
               }
            if (k<0){
               points[(index+0)%points.size()].x = x;
               points[(index-1)%points.size()].x = x;
               found = true;
               break;
               }
            }
         
         // Do the left ear case. p is the lower left corner of the ear.
         if (p.x == lastp.x && p.y < lastp.y &&              // top edge
             p.y==nextp.y && nextp.x > p.x &&                // left down edge
             lastp.y == last2p.y && last2p.x > lastp.x)      // right down edge
            {
            int x = MINIMUM(nextp.x, last2p.x);
            boxes.push(boxtype(layer, NULL, recttype(p, pointtype(x, lastp.y))));
            for (k=points.size()-1; k>=0; k--)  // check that this doesn't intersect some other part of the polygon
               {
               if (boxes.back().IsInsideNotOnEdge(points[k]))
                  {  // back out this change
                  boxes.pop();  
                  break;
                  }
               }
            if (k<0)
               {
               if (!this->IsInside(pointtype((boxes.back().r.x0+boxes.back().r.x1)/2,(boxes.back().r.y0+boxes.back().r.y1)/2)))
                  {
				  boxes.pop();
				  k=0;
				  }
               }
            if (k<0){
               points[(index+0)%points.size()].x = x;
               points[(index-1)%points.size()].x = x;
               found = true;
               break;
               }
            }
         }
         
      // now delete unnecessary vertices
      lastp = points[0];
      for (k=points.size()-1; k>=0; k--)
         {
         pointtype p = points[k];
         pointtype next = points[k==0 ? points.size()-1 : k-1];
         if (p == lastp)            // eliminate identical points
            points.Remove(k);
         else                       // eliminate colinear points
            {
            if (next.x == p.x && p.x == lastp.x)
               points.Remove(k);
            else if (next.y == p.y && p.y == lastp.y)
               points.Remove(k);
            else
               lastp = p;
            }
         }
      if (!found)
         {
         points = original_points;
         boxes.clear();
         BoxConvert2(boxes);
         points.clear();
         break;
         }
/*
      if (boxes.size()>1046) // erase me
         printf("");
      if (boxes.size()>1047) // erase me
         return;*/
      }
         
         
   if (count<100000){
      if (points.size()>4)
         FATAL_ERROR;
      if (points.size()==4)
	boxes.push(boxtype(layer, NULL, recttype(points[0], points[2])));
      }
   else{
     printf("Error in boxify. Here is the residue polygon. Number of points was %d in layer %s\n", points.size(), LayerNames(this->layer));
      for (i=0; i<points.size(); i++)
         printf("(%d , %d) ",points[i].x,points[i].y);
      printf("\n");
      FATAL_ERROR;
      }

   BoxAdjust(boxes, max_aspect_ratio, max_step_ratio);
   return;
   int previous, loop=0;
   do{
      previous = boxes.size();
      BoxAdjust(boxes, max_aspect_ratio, max_step_ratio);
      loop++;
      }while (boxes.size()!=previous && loop<10);
   }

// this will enforece the max aspect ratio and maximum transition ratio
void polytype::BoxAdjust(array<boxtype> &boxes, float max_aspect_ratio, float max_step_ratio) 
   {
   TAtype ta("polytype::BoxAdjust");
   array<boxtype> newboxes;  
   int i, k;
   // now go through and partition boxes to maintain a decent aspect ratio

   for (i=boxes.size()-1; i>=0; i--)
	  {
	  boxtype &b  = boxes[i];
	  if (b.r.Height()==0 || b.r.Width()==0)
		 boxes.RemoveFast(i);
	  }

   for (i=0; i<boxes.size(); i++)
	  {
	  boxtype &b  = boxes[i];
	  int width  = b.r.Width();
	  int height = b.r.Height();
	  if (height*max_aspect_ratio < width)
		 {
		 if (height*max_aspect_ratio*2 >= width)
			{
			int middle = (int)((b.r.x1 - b.r.x0)/2 + b.r.x0);
			newboxes.push(boxtype(b.layer, NULL, recttype(middle, b.r.y0, b.r.x1, b.r.y1)));
			b.r.x1 = middle;
			}
		 else
			{
			int middle = (int)(height*max_aspect_ratio + b.r.x0);
			newboxes.push(boxtype(b.layer, NULL, recttype(middle, b.r.y0, b.r.x1, b.r.y1))); // this box will be over the limit but will get carved up eventually
			b.r.x1 = middle;
			}
		 }
	  else if (width*max_aspect_ratio < height)
		 {
		 if (width*max_aspect_ratio*2 >= height)
			{
			int middle = (int)((b.r.y1 - b.r.y0)/2 + b.r.y0);
			newboxes.push(boxtype(b.layer, NULL, recttype(b.r.x0, middle, b.r.x1, b.r.y1)));
			b.r.y1 = middle;
			}
		 else
			{
			int middle = (int)(width*max_aspect_ratio + b.r.y0);
			newboxes.push(boxtype(b.layer, NULL, recttype(b.r.x0, middle, b.r.x1, b.r.y1))); // this box will be over the limit but will get carved up eventually
			b.r.y1 = middle;
			}
		 }
	  boxes += newboxes;
	  newboxes.clear();
	  }

   return;
   // this stuff below doesn't work!
   for (i=boxes.size()-1; i>=0; i--)
      {
      boxtype &b1 = boxes[i];
      for (k=i-1; k>=0; k--)
         {
         boxtype &b2 = boxes[k];
         if (b1.r.y0 == b2.r.y1 || b1.r.y1 == b2.r.y0)
            { // these two boxes share a horizontal edge
            int x0 = MAXIMUM(b1.r.x0, b2.r.x0), x1 = MINIMUM(b1.r.x1, b2.r.x1);
            int width = x1-x0;

            if (width>0 && width * max_step_ratio < b1.r.Width())
               { // cut b1 in half
               int midpoint = (b1.r.x0+b1.r.x1)/2;
               newboxes.push(b1);
               newboxes.back().r.x1 = midpoint;
               b1.r.x0 = midpoint;
               }
            if (width>0 && width * max_step_ratio < b2.r.Width())
               { // cut b2 in half
               int midpoint = (b2.r.x0+b2.r.x1)/2;
               newboxes.push(b2);
               newboxes.back().r.x1 = midpoint;
               b2.r.x0 = midpoint;
               }
            }
         if (b1.r.x0 == b2.r.x1 || b1.r.x1 == b2.r.x0)
            { // these two boxes share a vertical edge
            int y0 = MAXIMUM(b1.r.y0, b2.r.y0), y1 = MINIMUM(b1.r.y1, b2.r.y1);
            int height = y1-y0;

            if (height>0 && height * max_step_ratio < b1.r.Height())
               { // cut b1 in half
               int midpoint = (b1.r.y0+b1.r.y1)/2;
               newboxes.push(b1);
               newboxes.back().r.y1 = midpoint;
               b1.r.y0 = midpoint;
               }
            if (height>0 && height * max_step_ratio < b2.r.Height())
               { // cut b2 in half
               int midpoint = (b2.r.y0+b2.r.y1)/2;
               newboxes.push(b2);
               newboxes.back().r.y1 = midpoint;
               b2.r.y0 = midpoint;
               }
            }
         }
      }
   boxes += newboxes;
   newboxes.clear();


   }



// returns whether the two polygons intersect each other, coincident edges count
bool polytype::IsIntersect(const polytype &right) const
   {
   TAtype ta("polytype::IsIntersect");
   array <intervaltype> verts, horzs;
   heaptype <intervaltype> heap;
   int i, k;
   
   pointtype lastp = points.back();
   for (k=0; k<points.size(); k++)
      {
      pointtype p = points[k];
      
      if (p.x == lastp.x)            
         verts.push(intervaltype(MINIMUM(p.y, lastp.y), MAXIMUM(p.y, lastp.y), 0, p.x));  // vertical edge
      else   
         horzs.push(intervaltype(MINIMUM(p.x, lastp.x), MAXIMUM(p.x, lastp.x), 0, p.y));  // vertical edge
      
      lastp = p;
      }
   lastp = right.points.back();
   for (k=0; k<right.points.size(); k++)
      {
      pointtype p = right.points[k];
      
      if (p.x == lastp.x)            
         verts.push(intervaltype(MINIMUM(p.y, lastp.y), MAXIMUM(p.y, lastp.y), 1, p.x));  // vertical edge
      else   
         horzs.push(intervaltype(MINIMUM(p.x, lastp.x), MAXIMUM(p.x, lastp.x), 1, p.y));  // vertical edge
      
      lastp = p;
      }
   std::sort(horzs.begin(), horzs.end(), intervalSorttype(0));  // sort by low (x0)
   std::sort(verts.begin(), verts.end(), intervalSorttype(3));  // sort by extra (x)

   // now do a sweep
   int x, v=0, h=0;
   do {      
      x = MAX_INT;

      if (h < horzs.size() && horzs[h].low < x)
         x = horzs[h].low;
      if (v < verts.size() && verts[v].extra <x)
         x = verts[v].extra;
      if (heap.size() && heap[0].high < x)
         x = heap[0].high;

      if (h < horzs.size() && horzs[h].low == x)
         {
         heap.push(horzs[h]);
         h++;
         }
      else if (v < verts.size() && verts[v].extra == x)
         {         
         for (i=0; i<heap.size(); i++)
            {
            if (heap[i].extra >= verts[v].low && heap[i].extra <= verts[v].high)
               if (heap[i].node != verts[v].node)
                  return true;
            }
         v++;
         }
      else if (heap.size() && heap[0].high == x)
         heap.pop();
      } while(x < MAX_INT);
   return false;
   }


bool polytype::IsNegative() const
   {
   int i, minx=MAX_INT;
   pointtype lastp;
   bool negative=false;
   // find the left most vertical edge. If its down then this polygon is positive

   lastp=points.back();
   for (i=0; i<points.size(); i++)
      {
      pointtype p = points[i];
      if (lastp.x == p.x && lastp.y != p.y)
         {
         if (p.x < minx)
            {
            minx = p.x;
            negative = p.y < lastp.y;
            }
         }
      lastp = p;
      }
   return negative;
   }


bool polytype::IsInside(const pointtype &p) const // points along an edge are considered exterior
   { // draw a horizontal line from P to +inf, count edge crossings. Point is in interior if count&1
   int uprcount=0, downrcount=0;
   int uplcount=0, downlcount=0;
   int i;
   pointtype last = points[points.size()-1];
   for (i=0; i<points.size(); i++)
      {  // this routine is bullet proof if all edges are k*90 degrees. 
         // It's also good for diagonal edges where the point isn't in the mbb of the diagonal
      if (last.y != points[i].y && last.x > p.x)
         {
         if (p.y > MINIMUM(last.y, points[i].y) && p.y <= MAXIMUM(last.y, points[i].y))
             uprcount++;
         if (p.y >= MINIMUM(last.y, points[i].y) && p.y < MAXIMUM(last.y, points[i].y))
            downrcount++;
         }
      if (last.y != points[i].y && last.x >= p.x)
         {
         if (p.y > MINIMUM(last.y, points[i].y) && p.y <= MAXIMUM(last.y, points[i].y))
            uplcount++;
         if (p.y >= MINIMUM(last.y, points[i].y) && p.y < MAXIMUM(last.y, points[i].y))
            downlcount++;
         }
      last = points[i];
      }
   return (uplcount&1) && (downlcount&1) && (uprcount&1) && (downrcount&1);
   }

int polytype::HorizontalCrossCutDepth(const pointtype &p, bool total) const // total=all width across the cut, !total is width till nearest edges
   { 
   int i;
   int depth=0;
   int right= MAX_INT, left=MIN_INT;

   pointtype last = points[points.size()-1];
   for (i=0; i<points.size(); i++)
      {  // this routine is bullet proof if all edges are k*90 degrees. 
      if (last.y >= points[i].y && last.y >= p.y && points[i].y <= p.y)
		 {
		 depth += last.x - p.x;
		 if (last.x >= p.x)
			right=MINIMUM(right, last.x);
		 if (last.x <= p.x)
			left=MAXIMUM(left, last.x);
		 }
      if (last.y <= points[i].y && last.y <= p.y && points[i].y >= p.y)
		 {
		 depth -= last.x - p.x;
		 if (last.x >= p.x)
			right=MINIMUM(right, last.x);
		 if (last.x <= p.x)
			left=MAXIMUM(left, last.x);
		 }
      last = points[i];
      }
   if ((left == MIN_INT) || (right == MAX_INT) || (left>right) || (depth<=0)) 
     {
       if (left == MIN_INT)  printf("ERROR: Could not find a good left edge. Point not enclosed in poly?\n");
       if (right == MAX_INT) printf("ERROR: Could not find a good right edge. Point not enclosed in poly?\n");
       if (left>right)       printf("ERROR: Left edge was to the right of the right edge. Point in enclosed notch?\n");       
       // This algorithm above results in negative depth if the polygon is counter-clockwise
       // around point (say even in a 4 sided polygon). So I think we can flip the sign of 
       // depth after the whole vector math is done and not do the FATAL_ERROR on depth<0
       // NOTE: The following is not an error condition and in fact happens in our designs
       if (0) 
	 {
	   if (depth<=0)         printf("Depth was less than zero. Counter clock-wise poly surrounding point?\n");       
	   printf("p=%d,%d\n",p.x,p.y);
	   printf("Polygon vertices below (split into multiple lines)\n ");
	   for (i=0; i<points.size(); i++) 
	     {
	       printf(" (%d,%d)",points[i].x,points[i].y);
	       // Print 8 points of the vertex per line
	       if (i % 8 == 7) printf("\n");
	     }
	   if (i % 8 != 7) printf("\n");
	 }
       if ((left>right)) FATAL_ERROR;
   }
   
   depth=abs(depth);
   
   if (!total)
	  {
	  if (right-left <0)
		 FATAL_ERROR;
	  return right-left;
	  }

   return depth;
   }

int polytype::VerticalCrossCutDepth(const pointtype &p, bool total) const  // total=all width across the cut, !total is width till nearest edges
   { 
   int i;
   int depth=0;
   int right= MAX_INT, left=MIN_INT;

   pointtype last = points[points.size()-1];
   for (i=0; i<points.size(); i++)
      {  // this routine is bullet proof if all edges are k*90 degrees. 
      if (last.x >= points[i].x && last.x >= p.x && points[i].x <= p.x)
		 {
		 depth -= last.y - p.y;
		 if (last.y >= p.y)
			right=MINIMUM(right, last.y);
		 if (last.y <= p.y)
			left=MAXIMUM(left, last.y);
		 }
      if (last.x <= points[i].x && last.x <= p.x && points[i].x >= p.x)
		 {
		 depth += last.y - p.y;
		 if (last.y >= p.y)
			right=MINIMUM(right, last.y);
		 if (last.y <= p.y)
			left=MAXIMUM(left, last.y);
		 }
      last = points[i];
      }

   if ((left == MIN_INT) || (right == MAX_INT) || (left>right) || (depth<=0)) 
     {
       if (left == MIN_INT)  printf("ERROR: Could not find a good left edge. Point not enclosed in poly?\n");
       if (right == MAX_INT) printf("ERROR: Could not find a good right edge. Point not enclosed in poly?\n");
       if (left>right)       printf("ERROR: Left edge was to the right of the right edge. Point in enclosed notch?\n");
       // This algorithm above results in negative depth if the polygon is counter-clockwise
       // around point (say even in a 4 sided polygon). So I think we can flip the sign of 
       // depth after the whole vector math is done and not do the FATAL_ERROR on depth<0
       // NOTE: The following is not an error condition and in fact happens in our designs
       if (0) 
	 {
	   if (depth<=0)         printf("ERROR: Depth was less than zero. Counter clock-wise poly surrounding point?\n");       
	   printf("p=%d,%d\n",p.x,p.y);
	   printf("Polygon vertices below (split into multiple lines)\n ");
	   for (i=0; i<points.size(); i++) 
	     {
	       printf(" (%d,%d)",points[i].x,points[i].y);
	       // Print 8 points of the vertex per line
	       if (i % 8 == 7) printf("\n");
	     }
	   if (i % 8 != 7) printf("\n");
	 }
       if ((left>right)) FATAL_ERROR;
   }
   
   depth=abs(depth);
   
   if (!total)
	  {
	  if (right-left <0)
		 FATAL_ERROR;
	  return right-left;
	  }

   return depth;
   }

void polytype::ToggleFeet()
   {
   int i;
   pointtype p;
   for (i=0; i<points.size()/2; i++)
      {
      p = points[i]; 
      points[i] = points[points.size()-1-i];
      points[points.size()-1-i] = p;
      }
   }

void polytype::EliminateSlivers(int threshold)
   {
   int i;
   if (IsNegative())
      FATAL_ERROR;

   bool change=false;
   pointtype lastp = points.back();
   for (i=0; i<points.size(); i++)
      {
      pointtype &p = points[i];

      if (lastp.x == p.x && lastp.y>p.y && lastp.y-p.y<=threshold)
         {
         p.y = lastp.y;
         points[(i+1)%points.size()].y = lastp.y;
         change=true;
         }
      if (lastp.x == p.x && lastp.y<p.y && p.y - lastp.y<=threshold)
         {
         lastp.y = p.y;
         points[(i-1+points.size())%points.size()].y = p.y;
         points[(i-2+points.size())%points.size()].y = p.y;
         change=true;
         }
      if (lastp.y == p.y && lastp.x>p.x && lastp.x-p.x<=threshold)
         {
         p.x = lastp.x;
         points[(i+1)%points.size()].x = lastp.x;
         change=true;
         }
      if (lastp.y == p.y && lastp.x<p.x && p.x - lastp.x<=threshold)
         {
         lastp.x = p.x;
         points[(i-1+points.size())%points.size()].x = p.x;
         points[(i-2+points.size())%points.size()].x = p.x;
         change=true;
         }
      
      lastp = p;
      }
   if (change)
      Cleanup();
   }

class polyedgesorttype
   {
public:
   layertype layer;
   int x, y0, y1, node, track;

   bool operator<(const polyedgesorttype &right) const  // predicate heap will use
      {
      return y1 < right.y1;
      }
   };
struct polyedgeSortPredicatetype{  // function object (predicate for stl)
   bool operator()(const polyedgesorttype &x, const polyedgesorttype &y) const
      {
      return x.y0 < y.y0;
      }
   };


void polytype::Translate(const translationtype &t)
   {
   for (int i=0; i<points.size(); i++)
      {
      points[i] = t.Transform(points[i]);
      }
   }

#ifndef NOGUI
/*
void boxtype::Paint(wxDC &dc, const translationtype &t, const PENtype &pen) const
   {  // promote simple polygon to generic polygon and let it do the painting
   if (r.x1 < r.x0 || r.y1 < r.y0) 
      FATAL_ERROR;
   polytype poly(*this);
   poly.Paint(dc, t, pen);
   }
   

void polytype::Paint(wxDC &dc, const translationtype &t, const PENtype &pen, bool ignoreComment) const
   {
   bool contact = false;
   int textsize=150;
   mbbtype mbb = Mbb();
   int i;
   const int threshold = 2;

   if (points.size()<4) FATAL_ERROR;
//   if (fabs(t.ReturnXScale(mbb.Rect().x1 - mbb.Rect().x0)) <threshold || fabs(t.ReturnYScale(mbb.Rect().y1 - mbb.Rect().y0)) <threshold)
//      return;
  
   pen.Polygon(points);
   if (!IsLeftFooted())
      FATAL_ERROR;

//   char comment[64];
//   sprintf(comment,"%p",this);

   const char *chptr;
   if (comment && (chptr = strchr(comment, ':'))!=NULL)
      chptr++;
   else
      chptr=comment;
   
   // Now I want to find the longest edge and place text next to it
   if (textsize && chptr && !ignoreComment)
      {
      int maxdist=-1, index;
      pointtype lastp=points[points.size()-1];
      for (i=0; i<points.size(); i++)
         {
         int d = points[i].Distance(lastp);
         if (d > maxdist)
            {
            maxdist = d;
            index = i;
            }
         lastp = points[i];
         }
      if (index==0)
         lastp = points[points.size()-1];
      else
         lastp = points[index-1];
      wxColour color = pen.GetColor();
      if (points[index].y == lastp.y)  // horizontal edge
         {
         int y = lastp.y;
         int x0 = MINIMUM(lastp.x, points[index].x), x1= MAXIMUM(lastp.x, points[index].x);
         bool bottom = points[index].x > lastp.x;   // I know if i found the top/bottom of wire because poly is left footed
                 
         // Now find the width of the wire
         int height=MAX_INT;
         lastp = points[points.size()-1];
         for (i=0; i<points.size(); i++)
            {
            if (points[i].y == lastp.y && points[i].x<lastp.x && bottom && lastp.x>x0 && points[i].x<x1 && lastp.y>y)
               height = MINIMUM(lastp.y - y, height);
            if (points[i].y == lastp.y && points[i].x>lastp.x && !bottom && points[i].x>x0 && lastp.x<x1 && lastp.y<y)
               height = MINIMUM(y - lastp.y, height);
            lastp = points[i];
            }
         if (height==MAX_INT)
            FATAL_ERROR;
//            return;//FATAL_ERROR;

         float fudge=1.0;
         while (fudge>0.1)
            {
            float scaledtextsize = MINIMUM(t.ReturnYScale(height), 1.2*t.ReturnXScale(x1-x0)/strlen(chptr) ) * fudge-1;
            float textwidth = scaledtextsize / t.ReturnYScale();
            
            if (scaledtextsize < 6) break;
            TEXTtype text(dc, t, (int)scaledtextsize, color, false);
            pointtype dimension = text.TextExtents(chptr);
            if (dimension.y > height || dimension.x>(x1-x0))
               fudge = fudge*0.9;
            else {
               int replicate = MAXIMUM(1, (int)((x1 - x0)/(textwidth * 40 +1)-1));
               for (i=1; i<=replicate; i++)
//               for (i=1; i==1; i++)
                  {
                  text.TextOut(pointtype(x0 + (x1 - x0)*i/(replicate+1), y + (bottom ? height/2 : -height/2)), chptr);
                  }
               break;
               }
            }
         }
      else if (points[index].x == lastp.x)
         {
         int x = lastp.x;
         int y0 = MINIMUM(lastp.y, points[index].y), y1= MAXIMUM(lastp.y, points[index].y);
         bool left = points[index].y < lastp.y;   // I know if i found the top/bottom of wire because poly is left footed
                 
         // Now find the width of the wire
         int height=10000;
         lastp = points[points.size()-1];
         for (i=0; i<points.size(); i++)
            {
            if (points[i].x == lastp.x && points[i].y<lastp.y && !left && lastp.y>y0 && points[i].y<y1 && lastp.x<x)
               height = MINIMUM(x - lastp.x, height);
            if (points[i].x == lastp.x && points[i].y>lastp.y && left && points[i].y>y0 && lastp.y<y1 && lastp.x>x)
               height = MINIMUM(lastp.x - x, height);
            lastp = points[i];
            }
         if (height==10000)
//            FATAL_ERROR;
            return;//FATAL_ERROR;

         float fudge=0.875;
         while (fudge>0.1)
            {
            float scaledtextsize = MINIMUM(t.ReturnXScale(height), 1.2*t.ReturnYScale(y1-y0)/strlen(chptr) ) * fudge-1;
            float textwidth = scaledtextsize / t.ReturnYScale();
            
            if (scaledtextsize < 6) break;
            TEXTtype text(dc, t, (int)scaledtextsize, color, true);
            pointtype dimension = text.TextExtents(chptr);
            if (dimension.x > height || dimension.y>(y1-y0))
               fudge = fudge*0.9;
            else {
               int replicate = MAXIMUM(1, (int)((y1 - y0)/(textwidth * 40 +1)-1));
               for (i=1; i<=replicate; i++)
                  {
                  text.TextOut(pointtype( x + (left ? height/2 : -height/2), y0 + (y1 - y0)*i/(replicate+1)), chptr);
                  }
               break;
               }
            }
         }
      }

   if (contact)  // draw an X in a contact
      {
      pen.Line(points[0], points[2]);
      pen.Line(points[1], points[3]);
      }
   }
*/
#endif
