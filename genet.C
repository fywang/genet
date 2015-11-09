/**
 * Copyright (C) 2015 Felix Wang
 *
 * Simulation Tool for Asynchrnous Cortical Streams (stacs)
 */

#include "genet.h"

/**************************************************************************
* Charm++ Read-Only Variables
**************************************************************************/
/*readonly*/ std::string filebase;
/*readonly*/ std::string filemod;
/*readonly*/ idx_t npdat;
/*readonly*/ idx_t npnet;
/*readonly*/ idx_t rngseed;


/**************************************************************************
* Main
**************************************************************************/

// Main entry point
//
Main::Main(CkArgMsg* msg) {
  // Display title
  CkPrintf("Generate Network for STACS (genet)\n");

  // Command line arguments
  std::string configfile;
  if (msg->argc < 2) {
    configfile = "config.yml"; // default
    mode = std::string("build");
  }
  else if (msg->argc == 2) {
    mode = msg->argv[1];
    if (mode != "build" && mode != "order") {
      configfile = msg->argv[1];
      mode = std::string("build");
    }
    else {
      configfile = "config.yml"; // default
    }
  }
  else if (msg->argc == 3) {
    configfile = msg->argv[1];
    mode = msg->argv[2];
    if (mode != "build" && mode != "order") {
      CkPrintf("Error: mode %s not valid\n"
               "       valid modes: build, order\n", mode.c_str());
      CkExit();
    }
  }
  else {
    CkPrintf("Usage: [config file] [mode]\n");
    CkExit();
  }
  delete msg;

  // Parsing config
  if (ParseConfig(configfile)) {
    CkPrintf("Error loading config...\n");
    CkExit();
  }

  // Basic error check
  if (npdat != CkNumPes()) {
    CkPrintf("Error: npdat (%" PRIidx ") does not match CkNumPes (%d)\n"
             "       Use '+p%" PRIidx "' to set %" PRIidx " PEs in Charm++\n",
             npdat, CkNumPes(), npdat, npdat);
    CkExit();
  }
  
  // Display configuration information
  CkPrintf("Loaded config from %s\n"
           "  Data Files (npdat):     %" PRIidx "\n"
           "  Network Parts (npnet):  %" PRIidx "\n",
           configfile.c_str(), npdat, npnet);

  CkPrintf("Initializing models\n");
  // Read model information
  if (ReadModel()) {
    CkPrintf("Error loading models...\n");
    CkExit();
  }
  
  // Print out model information
  for (idx_t i = 0; i < models.size(); ++i) {
    CkPrintf("  Model: %" PRIidx "   ModName: %s   Type: %s   States: %d   Sticks: %d\n",
        i+1, models[i].modname.c_str(), graphtype[models[i].type].c_str(), models[i].statetype.size(), models[i].sticktype.size());
  }

  // Set up control flags
    buildflag = true;
    metisflag = true;
    orderflag = true;
    writeflag = true;
  if (mode == "build") {
    metisflag = false;
    orderflag = false;
  }
  else if (mode == "order") {
    buildflag = false;
  }

  // Build model message
  mModel *mmodel = BuildModel();
  
  // Set Round Robin Mapping
  CkArrayOptions opts(npdat);
  CProxy_RRMap rrMap = CProxy_RRMap::ckNew();
  opts.setMap(rrMap);

  // Create chare array
  CkCallback *cb = new CkCallback(CkReductionTarget(Main, Control), thisProxy);
  genet = CProxy_GeNet::ckNew(mmodel, opts);
  genet.ckSetReductionClient(cb);
}

// Main migration
//
Main::Main(CkMigrateMessage* msg) {
  delete msg;
}

// Main control
//
void Main::Control() {
  if (buildflag) {
    CkPrintf("Building network\n");
    buildflag = false;

    // Read graph information
    if (ReadGraph()) {
      CkPrintf("Error loading graph...\n");
      CkExit();
    }
    mGraph *mgraph = BuildGraph();

    // Build Network
    CkCallback *cb = new CkCallback(CkReductionTarget(Main, Control), thisProxy);
    genet.Build(mgraph);
    genet.ckSetReductionClient(cb);
  }
  else if (metisflag) {
    CkPrintf("Reading network\n");
    metisflag = false;
    
    if (ReadMetis()) {
      CkPrintf("Error loading metis...\n");
      CkExit();
    }
    mMetis *mmetis = BuildMetis();
    
    CkCallback *cb = new CkCallback(CkReductionTarget(Main, Control), thisProxy);
    genet.Read(mmetis);
    genet.ckSetReductionClient(cb);
  }
  else if (orderflag) {
    CkPrintf("Reordering network\n");
    orderflag = false;
    
    CkCallback *cb = new CkCallback(CkReductionTarget(Main, Control), thisProxy);
    genet.ScatterPart();
    genet.ckSetReductionClient(cb);
  }
  else if (writeflag) {
    CkPrintf("Writing network\n");
    writeflag = false;
    
    CkCallback *cb = new CkCallback(CkReductionTarget(Main, Halt), thisProxy);
    genet.Write();
    genet.ckSetReductionClient(cb);
  }
}

// Main Stop
//
void Main::Halt(CkReductionMsg *msg) {
  CkPrintf("Finalizing network\n");
 
  // Save network part distribution to local
  netdist.clear();
  for (int i = 0; i < (msg->getSize())/sizeof(dist_t); ++i) {
    netdist.push_back(*((dist_t *)msg->getData()+i));
  }
  delete msg;

  // Write distribution
  if (WriteDist()) {
    CkPrintf("Error writing distribution...\n");
    CkExit();
  }

  CkExit();
}


/**************************************************************************
* Generate Network
**************************************************************************/

// GeNet Constructor
//
GeNet::GeNet(mModel *msg) {
  // Bookkeeping
  datidx = thisIndex;
  idx_t ndiv = npnet/npdat;
  idx_t nrem = npnet%npdat;
  nprt = ndiv + (datidx < nrem);
  xprt = datidx*ndiv + (datidx < nrem ? datidx : nrem);
  
  // Set up random number generator
  rngine.seed(rngseed+datidx);
  unifdist = new std::uniform_real_distribution<real_t> (0.0, 1.0);
  normdist = new std::normal_distribution<real_t> (0.0, 1.0);

  // RNG types (for errors)
  rngtype.resize(RNGTYPE_NRNG);
  rngtype[RNGTYPE_CONST] = std::string("constant");
  rngtype[RNGTYPE_UNIF] = std::string("uniform");
  rngtype[RNGTYPE_NORM] = std::string("normal");
  rngtype[RNGTYPE_BNORM] = std::string("bounded normal");
  rngtype[RNGTYPE_LIN] = std::string("linear");
  rngtype[RNGTYPE_LBLIN] = std::string("lower bounded linear");
  rngtype[RNGTYPE_UBLIN] = std::string("upper bounded linear");
  rngtype[RNGTYPE_BLIN] = std::string("bounded linear");

  // Set up counters
  idx_t jstateparam = 0;
  idx_t jstickparam = 0;

  // Set up maps
  modmap.clear();
  modname.resize(msg->nmodel+1);
  modname[0] = std::string("none");
  modmap[modname[0]] = 0;
  // Read in models
  models.resize(msg->nmodel);
  for (idx_t i = 0; i < models.size(); ++i) {
    // modname
    models[i].modname = std::string(msg->modname + msg->xmodname[i], msg->modname + msg->xmodname[i+1]);
    modname[i+1] = models[i].modname;
    modmap[models[i].modname] = i+1;
    // type
    models[i].type = msg->type[i];
    // prepare containers
    models[i].statetype.resize(msg->xstatetype[i+1] - msg->xstatetype[i]);
    models[i].stateparam.resize(msg->xstatetype[i+1] - msg->xstatetype[i]);
    for (idx_t j = 0; j < models[i].statetype.size(); ++j) {
      // statetype
      models[i].statetype[j] = msg->statetype[msg->xstatetype[i] + j];
      switch (models[i].statetype[j]) {
        case RNGTYPE_CONST:
          models[i].stateparam[j].resize(RNGPARAM_CONST);
          break;
        case RNGTYPE_UNIF:
          models[i].stateparam[j].resize(RNGPARAM_UNIF);
          break;
        case RNGTYPE_NORM:
          models[i].stateparam[j].resize(RNGPARAM_NORM);
          break;
        case RNGTYPE_BNORM:
          models[i].stateparam[j].resize(RNGPARAM_BNORM);
          break;
        case RNGTYPE_LIN:
          models[i].stateparam[j].resize(RNGPARAM_LIN);
          break;
        case RNGTYPE_BLIN:
          models[i].stateparam[j].resize(RNGPARAM_BLIN);
          break;
        default:
          CkPrintf("Error: unknown statetype\n");
          break;
      }
      for (idx_t s = 0; s < models[i].stateparam[j].size(); ++s) {
        models[i].stateparam[j][s] = msg->stateparam[jstateparam++];
      }
    }
    // prepare containers
    models[i].sticktype.resize(msg->xsticktype[i+1] - msg->xsticktype[i]);
    models[i].stickparam.resize(msg->xsticktype[i+1] - msg->xsticktype[i]);
    for (idx_t j = 0; j < models[i].sticktype.size(); ++j) {
      // sticktype
      models[i].sticktype[j] = msg->sticktype[msg->xsticktype[i] + j];
      switch (models[i].sticktype[j]) {
        case RNGTYPE_CONST:
          models[i].stickparam[j].resize(RNGPARAM_CONST);
          break;
        case RNGTYPE_UNIF:
          models[i].stickparam[j].resize(RNGPARAM_UNIF);
          break;
        case RNGTYPE_NORM:
          models[i].stickparam[j].resize(RNGPARAM_NORM);
          break;
        case RNGTYPE_BNORM:
          models[i].stickparam[j].resize(RNGPARAM_BNORM);
          break;
        case RNGTYPE_LIN:
          models[i].stickparam[j].resize(RNGPARAM_LIN);
          break;
        case RNGTYPE_BLIN:
          models[i].stickparam[j].resize(RNGPARAM_BLIN);
          break;
        default:
          CkPrintf("Error: unknown statetype\n");
          break;
      }
      for (idx_t s = 0; s < models[i].stickparam[j].size(); ++s) {
        models[i].stickparam[j][s] = msg->stickparam[jstickparam++];
      }
    }
  }
  // Sanity check
  CkAssert(jstateparam == msg->nstateparam);
  CkAssert(jstickparam == msg->nstickparam);

  // cleanup
  delete msg;

  // Try to do MPI stuff
  // MPI initialized by Charm++ if using MPI for communication
  int world_size;
  MPI_Comm_size(MPI_COMM_WORLD, &world_size);
  int world_rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
  CkPrintf("GaNet PE: %d   MPI: %d/%d\n", datidx, world_rank, world_size);

  // return control to main
  contribute(0, NULL, CkReduction::nop);
}

// GeNet Migration
GeNet::GeNet(CkMigrateMessage* msg) {
  delete msg;
}

// GeNet Destructor
//
GeNet::~GeNet() {
}

/**************************************************************************
* Charm++ Definitions
**************************************************************************/
#include "genet.def.h"