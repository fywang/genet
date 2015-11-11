/**
 * Copyright (C) 2015 Felix Wang
 *
 * Simulation Tool for Asynchrnous Cortical Streams (stacs)
 *
 * iocsr.C
 * Handles condensed sparse row format
 */

#include "genet.h"

// Maximum size of input line (bytes)
#define MAXLINE 1280000

/**************************************************************************
* Charm++ Read-Only Variables
**************************************************************************/
extern /*readonly*/ std::string filebase;
extern /*readonly*/ std::string filemod;
extern /*readonly*/ idx_t npdat;
extern /*readonly*/ idx_t npnet;


/**************************************************************************
* Reduction for network distribution
**************************************************************************/

CkReduction::reducerType net_dist;
/*initnode*/
void registerNetDist(void) {
  net_dist = CkReduction::addReducer(netDist);
}

CkReductionMsg *netDist(int nMsg, CkReductionMsg **msgs) {
  std::vector<dist_t> ret;
  ret.clear();
  for (int i = 0; i < nMsg; i++) {
    for (std::size_t j = 0; j < msgs[i]->getSize()/sizeof(dist_t); ++j) {
      // Extract data and reduce 
      ret.push_back(*((dist_t *)msgs[i]->getData() + j));
    }
  }
  return CkReductionMsg::buildNew(ret.size()*sizeof(dist_t), ret.data());
}


/**************************************************************************
* Partition Network Reading
**************************************************************************/

// Read graph partitioning
//
void GeNet::Read(mMetis *msg) {
  /* Bookkeeping */
  idx_t nsizedat;
  idx_t nstatedat;
  idx_t nstickdat;
  //idx_t nstickdat;
  /* File operations */
  FILE *pPart;
  FILE *pCoord;
  FILE *pAdjcy;
  FILE *pState;
  char csrfile[100];
  char *line;
  char *oldstr, *newstr;

  // Copy over metis distributions
  vtxdistmetis.resize(npdat+1);
  edgdistmetis.resize(npdat+1);
  for (idx_t i = 0; i < npdat+1; ++i) {
    vtxdistmetis[i] = msg->vtxdist[i];
    edgdistmetis[i] = msg->edgdist[i];
  }
  // TODO: Is edgdistmetis ever used?

  // cleanup
  delete msg;
  
  // Prepare buffer
  line = new char[MAXLINE];

  // Open files for reading
  sprintf(csrfile, "%s.part.%" PRIidx "", filebase.c_str(), datidx);
  pPart = fopen(csrfile,"r");
  sprintf(csrfile, "%s.coord.%" PRIidx "", filebase.c_str(), datidx);
  pCoord = fopen(csrfile,"r");
  sprintf(csrfile, "%s.adjcy.%" PRIidx "", filebase.c_str(), datidx);
  pAdjcy = fopen(csrfile,"r");
  sprintf(csrfile, "%s.state.%" PRIidx "", filebase.c_str(), datidx);
  pState = fopen(csrfile,"r");
  if (pPart == NULL || pCoord == NULL || pAdjcy == NULL ||
      pState == NULL || line == NULL) {
    CkPrintf("Error opening files for reading\n");
    CkExit();
  }

  // Initialize sizes
  partmetis.resize(vtxdistmetis[datidx+1] - vtxdistmetis[datidx]);
  vtxidxpart.resize(npnet);
  vtxmodidxpart.resize(npnet);
  xyzpart.resize(npnet);
  edgmodidxpart.resize(npnet);
  adjcypart.resize(npnet);
  statepart.resize(npnet);
  stickpart.resize(npnet);
  nsizedat = 0;
  nstatedat = 0;
  nstickdat = 0;

  // Read in graph information
  for (std::size_t i = 0; i < partmetis.size(); ++i) {
    while(fgets(line, MAXLINE, pPart) && line[0] == '%');
    oldstr = line;
    newstr = NULL;
    // partmetis
    partmetis[i] = strtoidx(oldstr, &newstr, 10);
    CkAssert(partmetis[i] < npnet);
    vtxidxpart[partmetis[i]].push_back(vtxdistmetis[datidx]+i);
    adjcypart[partmetis[i]].push_back(std::vector<idx_t>());

    // Read in line (coordinates)
    while(fgets(line, MAXLINE, pCoord) && line[0] == '%');
    oldstr = line;
    newstr = NULL;
    // xyz
    for (idx_t j = 0; j < 3; ++j) {
      real_t coord = strtoreal(oldstr, &newstr);
      oldstr = newstr;
      xyzpart[partmetis[i]].push_back(coord);
    }

    // Read line (per vertex)
    while(fgets(line, MAXLINE, pAdjcy) && line[0] == '%');
    oldstr = line;
    newstr = NULL;
    for(;;) {
      idx_t edg = strtoidx(oldstr, &newstr, 10);
      // check for end of line
      if (edg == 0 && oldstr != line)
        break;
      oldstr = newstr;
      // adjcy
      adjcypart[partmetis[i]].back().push_back(edg);
      ++nsizedat;
    }

    // Extract State Information
    // Read line (vertex)
    while(fgets(line, MAXLINE, pState) && line[0] == '%');
    oldstr = line;
    newstr = NULL;
    // extract model index from name
    idx_t modidx = strtomodidx(oldstr, &newstr);
    CkAssert(modidx != IDX_T_MAX);
    oldstr = newstr;
    // vtxmodidx
    vtxmodidxpart[partmetis[i]].push_back(modidx);
    statepart[partmetis[i]].push_back(std::vector<real_t>());
    stickpart[partmetis[i]].push_back(std::vector<tick_t>());
    CkAssert(modidx > 0);
    for(std::size_t s = 0; s < models[modidx-1].statetype.size(); ++s) {
      real_t stt = strtoreal(oldstr, &newstr);
      oldstr = newstr;
      // state
      statepart[partmetis[i]].back().push_back(stt);
      ++nstatedat;
    }
    for(std::size_t s = 0; s < models[modidx-1].sticktype.size(); ++s) {
      tick_t stt = strtotick(oldstr, &newstr, 10);
      oldstr = newstr;
      // state
      stickpart[partmetis[i]].back().push_back(stt);
      ++nstickdat;
    }

    // edgmodidx
    modidx = strtomodidx(oldstr, &newstr);
    oldstr = newstr;
    edgmodidxpart[partmetis[i]].push_back(std::vector<idx_t>());
    while (modidx != IDX_T_MAX) {
      // modidx
      edgmodidxpart[partmetis[i]].back().push_back(modidx);
      statepart[partmetis[i]].push_back(std::vector<real_t>());
      stickpart[partmetis[i]].push_back(std::vector<tick_t>());
      // only push edge state if model and not 'none'
      if (modidx > 0) {
        for(std::size_t s = 0; s < models[modidx-1].statetype.size(); ++s) {
          real_t stt = strtoreal(oldstr, &newstr);
          oldstr = newstr;
          // state
          statepart[partmetis[i]].back().push_back(stt);
          ++nstatedat;
        }
        for(std::size_t s = 0; s < models[modidx-1].sticktype.size(); ++s) {
          tick_t stt = strtotick(oldstr, &newstr, 10);
          oldstr = newstr;
          // state
          stickpart[partmetis[i]].back().push_back(stt);
          ++nstickdat;
        }
      }
      modidx = strtomodidx(oldstr, &newstr);
      oldstr = newstr;
    }
  }

  // Cleanup
  fclose(pPart);
  fclose(pCoord);
  fclose(pAdjcy);
  fclose(pState);
  delete[] line;

  // Print out some information
  CkPrintf("  File: %" PRIidx "   Vertices: %" PRIidx "   Edges: %" PRIidx "   States: %" PRIidx "   Sticks: %" PRIidx"\n",
      datidx, partmetis.size(), nsizedat, nstatedat, nstickdat);

  // Prepare for partitioning
  cpdat = 0;
  norderdat = 0;
  vtxorder.resize(nprt);
  xyzorder.resize(nprt);
  adjcyorder.resize(nprt);
  edgmodidxorder.resize(nprt);
  stateorder.resize(nprt);
  stickorder.resize(nprt);
  norderprt.resize(nprt);
  for (idx_t i = 0; i < nprt; ++i) {
    norderprt[i] = 0;
  }
  vtxdist.resize(npdat+1);
  vtxdist[0] = 0;
  
  // return control to main
  contribute(0, NULL, CkReduction::nop);
}


/**************************************************************************
* Generate Network Writing
**************************************************************************/

// Write graph adjacency distribution
//
void GeNet::Write() {
  /* Bookkeeping */
  std::vector<dist_t> rdist;
  idx_t jvtxidx;
  /* File operations */
  FILE *pCoord;
  FILE *pAdjcy;
  FILE *pState;
  char csrfile[100];

  // Open files for writing
  sprintf(csrfile, "%s%s.coord.%" PRIidx "", filebase.c_str(), filemod.c_str(), datidx);
  pCoord = fopen(csrfile,"w");
  sprintf(csrfile, "%s%s.adjcy.%" PRIidx "", filebase.c_str(), filemod.c_str(), datidx);
  pAdjcy = fopen(csrfile,"w");
  sprintf(csrfile, "%s%s.state.%" PRIidx "", filebase.c_str(), filemod.c_str(), datidx);
  pState = fopen(csrfile,"w");
  if (pCoord == NULL || pAdjcy == NULL || pState == NULL) {
    CkPrintf("Error opening files for writing %" PRIidx "\n", datidx);
    CkExit();
  }
  
  // Set up distribution
  rdist.resize(nprt);
  jvtxidx = 0;

  // Loop through parts
  for (idx_t k = 0; k < nprt; ++k) {
    rdist[k].prtidx = xprt+k;
    rdist[k].nvtx = norderprt[k];
    rdist[k].nedg = 0;
    rdist[k].nstate = 0;
    rdist[k].nstick = 0;

    // Graph adjacency information
    for (idx_t i = 0; i < norderprt[k]; ++i) {
      // vertex coordinates
      fprintf(pCoord, " %" PRIrealfull " %" PRIrealfull " %" PRIrealfull "\n",
          xyz[jvtxidx*3+0], xyz[jvtxidx*3+1], xyz[jvtxidx*3+2]);

      // vertex state
      fprintf(pState, " %s", modname[vtxmodidx[jvtxidx]].c_str());
      CkAssert(vtxmodidx[jvtxidx] > 0);
      rdist[k].nstate += state[jvtxidx][0].size();
      rdist[k].nstick += stick[jvtxidx][0].size();
      for (std::size_t s = 0; s < models[vtxmodidx[jvtxidx]-1].statetype.size(); ++s) {
        fprintf(pState, " %" PRIrealfull "", state[jvtxidx][0][s]);
      }
      for (std::size_t s = 0; s < models[vtxmodidx[jvtxidx]-1].sticktype.size(); ++s) {
        fprintf(pState, " %" PRItick "", stick[jvtxidx][0][s]);
      }
      
      // edge state
      rdist[k].nedg += edgmodidx[jvtxidx].size();
      CkAssert(state[jvtxidx].size() == edgmodidx[jvtxidx].size() + 1);
      for (std::size_t j = 0; j < edgmodidx[jvtxidx].size(); ++j) {
        fprintf(pState, " %s", modname[edgmodidx[jvtxidx][j]].c_str());
        rdist[k].nstate += state[jvtxidx][j+1].size();
        rdist[k].nstick += stick[jvtxidx][j+1].size();
        if (edgmodidx[jvtxidx][j] > 0) {
          for (std::size_t s = 0; s < models[edgmodidx[jvtxidx][j]-1].statetype.size(); ++s) {
            fprintf(pState, " %" PRIrealfull "", state[jvtxidx][j+1][s]);
          }
          for (std::size_t s = 0; s < models[edgmodidx[jvtxidx][j]-1].sticktype.size(); ++s) {
            fprintf(pState, " %" PRItick "", stick[jvtxidx][j+1][s]);
          }
        }
      }

      // adjacency information
      CkAssert(adjcy[jvtxidx].size() == edgmodidx[jvtxidx].size());
      for (std::size_t j = 0; j < adjcy[jvtxidx].size(); ++j) {
        fprintf(pAdjcy, " %" PRIidx "", adjcy[jvtxidx][j]);
      }

      // one set per vertex
      fprintf(pState, "\n");
      fprintf(pAdjcy, "\n");
      ++jvtxidx;
    }
  }
  CkAssert(jvtxidx == norderdat);

  // Cleanup
  fclose(pCoord);
  fclose(pAdjcy);
  fclose(pState);

  // return control to main
  contribute(nprt*sizeof(dist_t), rdist.data(), net_dist);
}

/**************************************************************************
* Main (network distribution file)
**************************************************************************/


// Read distributions (metis)
//
int Main::ReadMetis() {
  FILE *pMetis;
  char csrfile[100];
  char *line;
  char *oldstr, *newstr;

  // Prepare buffer
  line = new char[MAXLINE];
  
  // Open files for reading
  sprintf(csrfile, "%s.metis", filebase.c_str());
  pMetis = fopen(csrfile,"r");
  if (pMetis == NULL || line == NULL) {
    CkPrintf("Error opening file for reading\n");
    return 1;
  }

  vtxdist.resize(npdat+1);
  edgdist.resize(npdat+1);

  // Get distribution info
  for (idx_t i = 0; i < npdat+1; ++i) {
    while(fgets(line, MAXLINE, pMetis) && line[0] == '%');
    oldstr = line;
    newstr = NULL;
    // vtxdist
    vtxdist[i] = strtoidx(oldstr, &newstr, 10);
    oldstr = newstr;
    // edgdist
    edgdist[i] = strtoidx(oldstr, &newstr, 10);
    CkPrintf("  %" PRIidx " %" PRIidx "\n", vtxdist[i], edgdist[i]);
  }

  // Cleanup
  fclose(pMetis);
  delete[] line;

  return 0;
}

// Write distributions
//
int Main::WriteDist() {
  /* File operations */
  FILE *pDist;
  FILE *pMetis;
  char csrfile[100];
  /* Bookkeeping */
  idx_t nvtx;
  idx_t nedg;
  idx_t nstate;
  idx_t nstick;

  // Open File
  sprintf(csrfile, "%s%s.dist", filebase.c_str(), filemod.c_str());
  pDist = fopen(csrfile,"w");
  sprintf(csrfile, "%s%s.metis", filebase.c_str(), filemod.c_str());
  pMetis = fopen(csrfile,"w");
  if (pDist == NULL || pMetis == NULL) {
    CkPrintf("Error opening file for writing\n");
    return 1;
  }

  // Sort distribution
  std::sort(netdist.begin(), netdist.end());

  // Write to file
  CkPrintf("  Writing network distribution\n");
  nvtx = nedg = nstate = nstick = 0;
  fprintf(pDist, "%" PRIidx " %" PRIidx " %" PRIidx " %" PRIidx "\n", nvtx, nedg, nstate, nstick);
  for (std::size_t i = 0; i < netdist.size(); ++i) {
    nvtx += netdist[i].nvtx;
    nedg += netdist[i].nedg;
    nstate += netdist[i].nstate;
    nstick += netdist[i].nstick;
    fprintf(pDist, "%" PRIidx " %" PRIidx " %" PRIidx " %" PRIidx "\n", nvtx, nedg, nstate, nstick);
  }

  // For Metis
  nvtx = nedg = 0;
  fprintf(pMetis, "%" PRIidx " %" PRIidx "\n", nvtx, nedg);
  for (idx_t datidx = 0; datidx < npdat; ++datidx) {
    idx_t ndiv = npnet/npdat;
    idx_t nrem = npnet%npdat;
    idx_t nprt = ndiv + (datidx < nrem);
    idx_t xprt = datidx*ndiv + (datidx < nrem ? datidx : nrem);
    for (idx_t jprt = 0; jprt < nprt; ++jprt) {
      nvtx += netdist[xprt+jprt].nvtx;
      nedg += netdist[xprt+jprt].nedg;
    }
    fprintf(pMetis, "%" PRIidx " %" PRIidx "\n", nvtx, nedg);
  }

  // Cleanup
  fclose(pDist);
  fclose(pMetis);

  return 0;
}
