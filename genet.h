/**
 * Copyright (C) 2015 Felix Wang
 *
 * Simulation Tool for Asynchrnous Cortical Streams (stacs)
 */

#ifndef __STACS_GENET_H__
#define __STACS_GENET_H__

#include <mpi.h>

#include <algorithm>
#include <list>
#include <cmath>
#include <random>
#include <sstream>
#include <string>
#include <cctype>
#include <vector>
#include <unordered_map>

#include "typedefs.h"
#include "timing.h"

#include <mpi.h>
#include "mpi-interoperate.h"
#include "genet-mpi.h"

#include "pup_stl.h"
#include "genet.decl.h"


#define GRAPHTYPE_NTYPE 2

#define GRAPHTYPE_VTX   0
#define GRAPHTYPE_EDG   1

#define RNGTYPE_NRNG    8
#define REPTYPE_REAL    0
#define REPTYPE_TICK    1

#define RNGTYPE_CONST   0
#define RNGTYPE_UNIF    1
#define RNGTYPE_NORM    2
#define RNGTYPE_BNORM   3
#define RNGTYPE_LIN     4
#define RNGTYPE_LBLIN   5
#define RNGTYPE_UBLIN   6
#define RNGTYPE_BLIN    7

#define RNGPARAM_CONST  1
#define RNGPARAM_UNIF   2
#define RNGPARAM_NORM   2
#define RNGPARAM_BNORM  3
#define RNGPARAM_LIN    2
#define RNGPARAM_LBLIN  3
#define RNGPARAM_UBLIN  3
#define RNGPARAM_BLIN   4

#define VTXSHAPE_CIRCLE 0
#define VTXSHAPE_SPHERE 1

#define VTXPARAM_CIRCLE 1 // TODO: something like point of origin
#define VTXPARAM_SPHERE 1

#define CONNTYPE_UNIF   0
#define CONNTYPE_SIG    1

#define CONNPARAM_UNIF  1
#define CONNPARAM_SIG   3


/**************************************************************************
* Charm++ Messages
**************************************************************************/

// Network distribution
//
void registerNetDist(void);
CkReductionMsg *netDist(int nMsg, CkReductionMsg **msgs);

// Model Information
//
#define MSG_Model 9
class mModel : public CMessage_mModel {
  public:
    idx_t *type;        // type of model (vertex/edge)
    idx_t *xmodname;    // model name prefix
    char *modname;      // model name
    idx_t *xstatetype;    // state generation prefix
    idx_t *xsticktype;    // stick generation prefix
    idx_t *statetype;     // state generation type
    idx_t *sticktype;     // stick representation type
    real_t *stateparam;   // state generation parameters
    real_t *stickparam;   // stick generation parameters
    idx_t nmodel;
    idx_t nstateparam;
    idx_t nstickparam;
};

#define MSG_Graph 14
class mGraph : public CMessage_mGraph {
  public:
    idx_t *vtxmodidx;     // Which vertex to build (modidx from modmap)
    idx_t *vtxorder;      // How many of each vertex to build
    idx_t *vtxshape;      // What shape to build vertices
    idx_t *xvtxparam;     // shape parameters prefix
    real_t *vtxparam;     // shape parameters
    idx_t *edgsource;     // source modidx of edge
    idx_t *xedgtarget;    // target prefix
    idx_t *edgtarget;     // target modidx of edge
    idx_t *edgmodidx;     // modidx of edge
    real_t *edgcutoff;    // cutoff distance of connection
    idx_t *xedgconntype;  // connection type prefix
    idx_t *edgconntype;   // connection type (computes probability threshold)
    idx_t *medgconnparam; // connection parameters sizes
    real_t *edgconnparam; // connection parameters
    idx_t nvtx;
    idx_t nvtxparam;
    idx_t nedg;
    idx_t nedgtarget;
    idx_t nedgconntype;
    idx_t nedgconnparam;
};

#define MSG_Conn 5
class mConn : public CMessage_mConn {
  public:
    idx_t *vtxmodidx;   // vertex model
    real_t *xyz;        // vertex coordinates
    idx_t *xadj;        // prefix for adjacency
    idx_t *adjcy;       // adjacent vertices
    idx_t *edgmodidx;   // edge models
    idx_t datidx;
    idx_t nvtx;
};

#define MSG_Metis 2
class mMetis : public CMessage_mMetis {
  public:
    idx_t *vtxdist; // number of vertices in data
    idx_t *edgdist; // number of edges in data
};
  
#define MSG_Part 8
class mPart : public CMessage_mPart {
  public:
    idx_t *vtxidx;
    idx_t *vtxmodidx;
    real_t *xyz;
    idx_t *xadj;
    idx_t *adjcy;
    idx_t *edgmodidx;
    real_t *state;
    tick_t *stick;
    idx_t datidx;
    idx_t prtidx;
    idx_t nvtx;
    idx_t nstate;
    idx_t nstick;
};
  
#define MSG_Order 2
class mOrder : public CMessage_mOrder {
  public:
    idx_t *vtxidxold;
    idx_t *vtxidxnew;
    idx_t datidx;
    idx_t nvtx;
};
  


/**************************************************************************
* Data Structures
**************************************************************************/

// Model
//
struct model_t {
  idx_t type;
  std::string modname;
  std::vector<idx_t> statetype;
  std::vector<std::vector<real_t>> stateparam;
  std::vector<idx_t> sticktype;
  std::vector<std::vector<real_t>> stickparam;
};

// Vertices
//
struct vertex_t {
  idx_t modidx;
  idx_t order;
  idx_t shape;
  std::vector<real_t> param;
};

// Edges
//
struct edge_t {
  idx_t source;
  std::vector<idx_t> target;
  idx_t modidx;
  real_t cutoff;
  std::vector<idx_t> conntype;
  std::vector<std::vector<real_t>> connparam;
};

// Size Distributions
//
struct dist_t {
  idx_t prtidx;
  idx_t nvtx;
  idx_t nedg;
  idx_t nstate;
  idx_t nstick;

  bool operator<(const dist_t& dist) const {
    return prtidx < dist.prtidx;
  }
};

// Vertex ordering
//
struct vtxorder_t {
  idx_t modidx;
  idx_t vtxidx;
  idx_t vtxidxloc; // local index of vertex
  bool operator < (const vtxorder_t& vtx) const {
    return (modidx < vtx.modidx);
  }
};

// Edge ordering
//
struct edgorder_t {
  idx_t edgidx;
  idx_t modidx;
  std::vector<real_t> state;
  std::vector<tick_t> stick;
  bool operator < (const edgorder_t& edg) const {
    return (edgidx < edg.edgidx);
  }
};

/**************************************************************************
* Charm++ Mainchare
**************************************************************************/

// Main
//
class Main : public CBase_Main {
  public:
    /* Constructors and Destructors */
    Main(CkArgMsg* msg);
    Main(CkMigrateMessage* msg);

    /* Control */
    void Control();
    void ReturnControl();
    void Halt(CkReductionMsg *msg);

    /* Persistence */
    int ParseConfig(std::string configfile);
    int ReadModel();
    int ReadGraph();
    int ReadMetis();
    int WriteDist();

    mModel* BuildModel();
    mGraph* BuildGraph();
    mMetis* BuildMetis();

  private:
    /* Chare Proxy */
    CProxy_GeNet genet;
    /* Models */
    std::vector<model_t> models;
    std::unordered_map<std::string, idx_t> modmap; // maps model name to object index
    /* Graph information */
    std::vector<vertex_t> vertices;
    std::vector<edge_t> edges;
    std::vector<std::string> graphtype;
    /* Persistence */
    std::vector<dist_t> netdist;
    std::vector<idx_t> vtxdist;
    std::vector<idx_t> edgdist;
    /* Bookkeeping */
    std::string mode;
    bool buildflag;
    bool partsflag;
    bool metisflag;
    bool orderflag;
    bool writeflag;
};


/**************************************************************************
* Charm++ Generate Network
**************************************************************************/

// Generate Network
//
class GeNet : public CBase_GeNet {
  public:
    /* Constructors and Destructors */
    GeNet(mModel *msg);
    GeNet(CkMigrateMessage* msg);
    ~GeNet();

    /* Build Network */
    void Build(mGraph *msg);
    void Connect(mConn *msg);
    void ConnRequest(idx_t reqidx);

    /* Partition Network */
    void SetPartition();

    /* Reorder Network */
    void Read(mMetis *msg);
    void ScatterPart();
    void GatherPart(mPart *msg);
    void Order(mOrder *msg);
    void Reorder(mOrder *msg);
    mOrder* BuildOrder();

    /* Write Network */
    void Write(const CkCallback &cb);

    /* Connections */
    mConn* BuildPrevConn(idx_t reqidx);
    mConn* BuildCurrConn();
    mConn* BuildNextConn();
    idx_t MakeConnection(idx_t source, idx_t target, real_t dist);
    std::vector<real_t> BuildEdgState(idx_t modidx, real_t dist);
    std::vector<tick_t> BuildEdgStick(idx_t modidx, real_t dist);

    /* Helper Functions */
    idx_t strtomodidx(const char* nptr, char** endptr) {
      const char *s;
      char c;
      int any;
      idx_t modidx = IDX_T_MAX;
      std::string name;
      // get rid of leading spaces
      s = nptr;
      do {
        c = *s++;
      } while (isspace(c));
      // read until space or end of line
      for (;; c = *s++) {
        // check for valid characters
        if (std::isalnum(c) || c == '_')
          name.append(&c,1);
        else
          break;
      }
      // If model exists, set it
      any = 0;
      if (modmap.find(name) != modmap.end()) {
        modidx = modmap[name];
        any = 1;
      }
      if (endptr != NULL)
        *endptr = (char *)(any ? s - 1 : nptr);
      return (modidx);
    }
    // Compute sigmoid
    real_t sigmoid(real_t x, real_t maxprob, real_t midpoint, real_t slope) {
      return maxprob * (1.0 - 1.0/(1.0 + std::exp( -slope * (x - midpoint) )));
    }
    // RNG State constant
    real_t rngconst(real_t *param) {
      return param[0];
    }
    // RNG State uniform
    real_t rngunif(real_t *param) {
      return param[0] + (param[1] - param[0])*((*unifdist)(rngine));
    }
    // RNG State normal
    real_t rngnorm(real_t *param) {
      return param[0] + (std::abs(param[1]))*((*normdist)(rngine));
    }
    // RNG State bounded normal
    real_t rngbnorm(real_t *param) {
      real_t state = (*normdist)(rngine);
      real_t bound = std::abs(param[2]);
      if (state > bound) { state = bound; }
      else if (state < -bound) { state = -bound; }
      return param[0] + (std::abs(param[1]))*state;
    }
    // RNG State linear
    real_t rnglin(real_t *param, real_t dist) {
      return dist*param[0] + param[1];
    }
    // RNG State lower bounded linear
    real_t rnglblin(real_t *param, real_t dist) {
      real_t state = dist*param[0] + param[1];
      if (state < param[2]) {
        state = param[3];
      }
      return state;
    }
    // RNG State bounded linear
    real_t rngblin(real_t *param, real_t dist) {
      real_t state = dist*param[0] + param[1];
      if (state < param[2]) {
        state = param[2];
      }
      if (state > param[3]) {
        state = param[3];
      }
      return state;
    }

  private:
    /* Network Data */
    std::vector<idx_t> vtxdist;
    std::vector<real_t> xyz;
    std::vector<std::vector<idx_t>> adjcy;
    std::vector<std::vector<std::vector<real_t>>> state;
        // first level is the vertex, second level is the models, third is state data
    std::vector<std::vector<std::vector<tick_t>>> stick;
    /* Models */
    std::vector<model_t> models;
    std::vector<std::string> modname;     // model names in order of object index
    std::unordered_map<std::string, idx_t> modmap; // maps model name to object index
    std::vector<std::string> rngtype;     // rng types in order of definitions
    std::vector<idx_t> vtxmodidx; // vertex model index into netmodel
    std::vector<std::vector<idx_t>> edgmodidx; // edge model index into netmodel
    /* Connection information */
    std::vector<std::vector<std::vector<idx_t>>> adjcyconn;
        // first level is the data parts, second level are per vertex, third level is edges
    std::list<idx_t> adjcyreq; // parts that are requesting thier adjacency info
    std::vector<std::vector<std::vector<idx_t>>> edgmodidxconn; // edge model index into netmodel
        // first level is the data parts, second level are per vertex, third level is edges
    /* Graph information */
    std::vector<vertex_t> vertices; // vertex models and build information
    std::vector<edge_t> edges; // edge models and connection information
    /* Metis */
    std::vector<idx_t> vtxdistmetis; // distribution of vertices on data
    std::vector<idx_t> edgdistmetis; // distribution of edges on data
    std::vector<idx_t> partmetis; // which vertex goes to which part
    std::vector<std::vector<idx_t>> vtxidxpart; // vertex indices to go to a part
    std::vector<std::vector<idx_t>> vtxmodidxpart; // vertex models to go to a part
    std::vector<std::vector<idx_t>> xyzpart; // vertex models to go to a part
    std::vector<std::vector<std::vector<idx_t>>> adjcypart; // edge indices (by vtxidx) to go to a part
    std::vector<std::vector<std::vector<idx_t>>> edgmodidxpart; // edge models to go to a part
    std::vector<std::vector<std::vector<real_t>>> statepart;
        // first level is the part, second is the models, thrid is state data
    std::vector<std::vector<std::vector<tick_t>>> stickpart;
    /* Reordering */
    std::vector<std::vector<vtxorder_t>> vtxorder; // modidx and vtxidx for sorting
    std::vector<edgorder_t> edgorder; // edgidx and states for sorting
    std::vector<std::vector<real_t>> xyzorder; // coordinates by vertex
    std::vector<std::vector<std::vector<idx_t>>> adjcyorder; // adjacency by vertex
    std::vector<std::vector<std::vector<idx_t>>> edgmodidxorder; // edge models by vertex
    std::vector<std::vector<std::vector<std::vector<real_t>>>> stateorder; // state by vertex
    std::vector<std::vector<std::vector<std::vector<tick_t>>>> stickorder; // stick by vertex
    std::list<mOrder *> ordering;
    /* Bookkeeping */
    idx_t datidx;
    idx_t cpdat, cpprt;
    idx_t nprt, xprt;
    idx_t norder; // total order
    idx_t norderdat; // order per data
    std::vector<idx_t> norderprt;  // order of vertices per network part
    std::vector<std::vector<idx_t>> nordervtx;  // order of vertex models 
    /* Random Number Generation */
    std::mt19937 rngine;
    std::uniform_real_distribution<real_t> *unifdist;
    std::normal_distribution<real_t> *normdist;
};


#endif //__STACS_GENET_H__
