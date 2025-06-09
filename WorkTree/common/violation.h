#ifndef __VIOLATION_H_INCLUDED
#define __VIOLATION_H_INCLUDED
#include <list>
#include "geometry.h"

class EMFailureSegment{
  //Failure segment looks like this in the report.
  //2.28  M1  997.35,1396.71  997.50,1397.03  OR
  //10236.04mV  CA  1073.05,616.83  1073.09,616.87
  
 private:
  char m_comment[256];
  boxtype bounding_box;
  float m_em_x;
  float m_ir_drop;
 public:
  EMFailureSegment(char comment[256], boxtype bb, float em_x, float ir_drop){
    strcpy(m_comment,comment);
    bounding_box=bb;
    m_em_x=em_x;
    m_ir_drop=ir_drop;
  }
  boxtype GetBoundingBox() {return bounding_box;}
  char* GetComment() {return m_comment;}
  float GetEMx(){return m_em_x;}
  float GetIRd(){return m_ir_drop;}
  bool operator<(EMFailureSegment& rhs){return (GetEMx() < rhs.GetEMx() ? true : false);}
  static bool CompareDC(EMFailureSegment& lhs, EMFailureSegment& rhs){return lhs.GetEMx() < rhs.GetEMx();}
  static bool CompareIR(EMFailureSegment& lhs, EMFailureSegment& rhs){return lhs.GetIRd() < rhs.GetIRd();}
};

class Violation{
 private:
  int m_id;
  char m_violname[256];
  float m_edges;
  int m_numdrivers;
  double m_rc;
  float m_em_x;
  float m_ir_drop;
  float m_deltaT;
  list<EMFailureSegment> m_failing_segments;
  bool m_is_sigtype, m_is_pem_violation, m_is_ir_violation;
 public:
  char* GetViolationName(){return m_violname;}
  list<EMFailureSegment>& GetFailingSegments() {return m_failing_segments;}
  //Violation may either be signal EM Eg: net: csclk_mesh   2.0   0   35.7ps   5C  2.28X 
  Violation(int id, char name[256], float edges, int drivers, double rc, float deltaT, float em_x): m_is_sigtype(true), m_is_pem_violation(false), m_is_ir_violation(false) {m_id=id; strcpy(m_violname,name);m_edges=edges;m_numdrivers=drivers;m_rc=rc;m_deltaT=deltaT;m_em_x=em_x;m_is_sigtype=true;};
  //or Power EM Eg: vio: (20013)  10236.0mV
  Violation(int id, char name[256], float em_x, float ir_drop, bool is_pem_violation, bool is_ir_violation): m_edges(0), m_numdrivers(0), m_rc(0), m_deltaT(0) {m_id=id; strcpy(m_violname,name); m_em_x=em_x; m_ir_drop=ir_drop;m_is_sigtype=false; m_is_pem_violation=is_pem_violation; m_is_ir_violation=is_ir_violation;}

  bool operator< (Violation& rhs)  {
    if ((GetEMx() > 1.0)||(rhs.GetEMx() > 1.0))
      return GetEMx() < rhs.GetEMx() ? true : false;
    else {
      //We sort by deltaT
      return GetDeltaT() < rhs.GetDeltaT() ? true : false;
    }
  }
  static bool CompareDC(Violation& lhs, Violation& rhs){
    return (lhs.GetEMx() < rhs.GetEMx()) ? true : false;
  }
  static bool CompareDeltaT(Violation& lhs, Violation& rhs){
    return (lhs.GetDeltaT() < rhs.GetDeltaT()) ? true : false;
  }
  static bool CompareIR(Violation& lhs, Violation& rhs){
    return (lhs.GetIRDrop() < rhs.GetIRDrop()) ? true : false;
  }
  float GetEdges(){return m_edges;}
  int GetNumDrivers() {return m_numdrivers;}
  double GetRC() {return m_rc;}
  float GetEMx(){return m_em_x;}
  float GetIRDrop(){return m_ir_drop;}
  bool IsSigType(){return m_is_sigtype;}
  int GetId() {return m_id;}
  bool IsPEMViolation(){return m_is_pem_violation;}
  bool IsIRViolation(){return m_is_ir_violation;}
  float GetDeltaT() {return m_deltaT;}
};

#endif
