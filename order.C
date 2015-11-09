/**
 * Copyright (C) 2015 Felix Wang
 *
 * Simulation Tool for Asynchrnous Cortical Streams (stacs)
 */

#include "genet.h"

/**************************************************************************
* Charm++ Read-Only Variables
**************************************************************************/
extern /*readonly*/ idx_t npdat;
extern /*readonly*/ idx_t npnet;


/**************************************************************************
* Partition
**************************************************************************/

// Scatter Partitions across Network
//
void GeNet::ScatterPart() {
  // Compute which part goes to which data
  for (idx_t datidxpart = 0; datidxpart < npdat; ++datidxpart) {
    // Which parts on this data
    idx_t ndivpart = npnet/npdat;
    idx_t nrempart = npnet%npdat;
    idx_t nprtpart = ndivpart + (datidxpart < nrempart);
    idx_t xprtpart = datidxpart*ndivpart + (datidxpart < nrempart ? datidxpart : nrempart);

    // Loop through parts
    for (idx_t prtidx = xprtpart; prtidx < xprtpart + nprtpart; ++prtidx) {
      // Count sizes
      idx_t nedgidx = 0;
      idx_t nstate = 0;
      idx_t nstick = 0;
      for (idx_t i = 0; i < vtxidxpart[prtidx].size(); ++i) {
        nedgidx += adjcypart[prtidx][i].size();
      }
      for (idx_t i = 0; i < statepart[prtidx].size(); ++i) {
        nstate += statepart[prtidx][i].size();
      }
      for (idx_t i = 0; i < stickpart[prtidx].size(); ++i) {
        nstick += stickpart[prtidx][i].size();
      }

      // Initialize connection message
      int msgSize[MSG_Part];
      msgSize[0] = vtxidxpart[prtidx].size();     // vtxidx
      msgSize[1] = vtxidxpart[prtidx].size();     // vtxmodidx
      msgSize[2] = vtxidxpart[prtidx].size() * 3; // xyz
      msgSize[3] = vtxidxpart[prtidx].size() + 1; // xadj
      msgSize[4] = nedgidx;                       // adjcy
      msgSize[5] = nedgidx;                       // edgmodidx
      msgSize[6] = nstate;                        // state
      msgSize[7] = nstick;                        // stick
      mPart *mpart = new(msgSize, 0) mPart;
      // Sizes
      mpart->datidx = datidx;
      mpart->prtidx = prtidx;
      mpart->nvtx = vtxidxpart[prtidx].size();
      mpart->nstate = nstate;
      mpart->nstick = nstick;

      // set up counters
      idx_t jedgidx = 0;
      idx_t jstate = 0;
      idx_t jstick = 0;
      // prefixes start at zero
      mpart->xadj[0] = 0;


      for (idx_t i = 0; i < vtxidxpart[prtidx].size(); ++i) {
        // vtxidx
        mpart->vtxidx[i] = vtxidxpart[prtidx][i];
        // vtxmodidx
        mpart->vtxmodidx[i] = vtxmodidxpart[prtidx][i];
        // xyz
        mpart->xyz[i*3+0] = xyzpart[prtidx][i*3+0];
        mpart->xyz[i*3+1] = xyzpart[prtidx][i*3+1];
        mpart->xyz[i*3+2] = xyzpart[prtidx][i*3+2];
        // xadj
        mpart->xadj[i+1] = mpart->xadj[i] + adjcypart[prtidx][i].size();
        for (idx_t j = 0; j < adjcypart[prtidx][i].size(); ++j) {
          // adjcy
          mpart->adjcy[jedgidx] = adjcypart[prtidx][i][j];
          // edgmodidx
          mpart->edgmodidx[jedgidx++] = edgmodidxpart[prtidx][i][j];
        }
      }
      CkAssert(jedgidx == nedgidx);
      for (idx_t i = 0; i < statepart[prtidx].size(); ++i) {
        for (idx_t s = 0; s < statepart[prtidx][i].size(); ++s) {
          mpart->state[jstate++] = statepart[prtidx][i][s];
        }
        for (idx_t s = 0; s < stickpart[prtidx][i].size(); ++s) {
          mpart->stick[jstick++] = stickpart[prtidx][i][s];
        }
      }
      CkAssert(jstate == nstate);
      CkAssert(jstick == nstick);

      // Send part
      thisProxy(datidxpart).GatherPart(mpart);
    }
  }
}


// Gather Partitions and perform reordering
//
void GeNet::GatherPart(mPart *msg) {
  // Bookkeeping
  idx_t prtidx = msg->prtidx - xprt;
  idx_t jstate = 0;
  idx_t jstick = 0;
  idx_t xvtx = vtxorder[prtidx].size();
  norderdat += msg->nvtx;
  norderprt[prtidx] += msg->nvtx;

  // allocate data
  vtxorder[prtidx].resize(norderprt[prtidx]);
  xyzorder[prtidx].resize(norderprt[prtidx]*3);
  adjcyorder[prtidx].resize(norderprt[prtidx]);
  edgmodidxorder[prtidx].resize(norderprt[prtidx]);
  stateorder[prtidx].resize(norderprt[prtidx]);
  stickorder[prtidx].resize(norderprt[prtidx]);

  // copy part data (unsorted)
  for (idx_t i = 0; i < msg->nvtx; ++i) {
    // vtxidx
    vtxorder[prtidx][xvtx+i].vtxidx = msg->vtxidx[i];
    // vtxmodidx
    vtxorder[prtidx][xvtx+i].modidx = msg->vtxmodidx[i];
    // localidx
    vtxorder[prtidx][xvtx+i].vtxidxloc = xvtx+i;
    // xyz
    xyzorder[prtidx][(xvtx+i)*3+0] = msg->xyz[i*3+0];
    xyzorder[prtidx][(xvtx+i)*3+1] = msg->xyz[i*3+1];
    xyzorder[prtidx][(xvtx+i)*3+2] = msg->xyz[i*3+2];
    // vertex state
    stateorder[prtidx][xvtx+i].push_back(std::vector<real_t>());
    stickorder[prtidx][xvtx+i].push_back(std::vector<tick_t>());
    CkAssert(vtxorder[prtidx][xvtx+i].modidx > 0);
    stateorder[prtidx][xvtx+i][0].resize(models[vtxorder[prtidx][xvtx+i].modidx-1].statetype.size());
    stickorder[prtidx][xvtx+i][0].resize(models[vtxorder[prtidx][xvtx+i].modidx-1].sticktype.size());
    for(idx_t s = 0; s < stateorder[prtidx][xvtx+i][0].size(); ++s) {
      stateorder[prtidx][xvtx+i][0][s] = msg->state[jstate++];
    }
    for(idx_t s = 0; s < stickorder[prtidx][xvtx+i][0].size(); ++s) {
      stickorder[prtidx][xvtx+i][0][s] = msg->stick[jstick++];
    }

    // handle edges
    idx_t xedg = adjcyorder[prtidx][xvtx+i].size();
    adjcyorder[prtidx][xvtx+i].resize(xedg + msg->xadj[i+1] - msg->xadj[i]);
    edgmodidxorder[prtidx][xvtx+i].resize(xedg + msg->xadj[i+1] - msg->xadj[i]);
    for (idx_t j = 0; j < msg->xadj[i+1] - msg->xadj[i]; ++j) {
      // adjcy
      adjcyorder[prtidx][xvtx+i][xedg+j] = msg->adjcy[msg->xadj[i] + j];
      // edgmodidx
      edgmodidxorder[prtidx][xvtx+i][xedg+j] = msg->edgmodidx[msg->xadj[i] + j];
      // state
      stateorder[prtidx][xvtx+i].push_back(std::vector<real_t>());
      stickorder[prtidx][xvtx+i].push_back(std::vector<tick_t>());
      // only push edge state if model and not 'none'
      if (edgmodidxorder[prtidx][xvtx+i][xedg+j] > 0) {
        stateorder[prtidx][xvtx+i][xedg+j+1].resize(models[edgmodidxorder[prtidx][xvtx+i][xedg+j]-1].statetype.size());
        stickorder[prtidx][xvtx+i][xedg+j+1].resize(models[edgmodidxorder[prtidx][xvtx+i][xedg+j]-1].sticktype.size());
        for(idx_t s = 0; s < stateorder[prtidx][xvtx+i][xedg+j+1].size(); ++s) {
          stateorder[prtidx][xvtx+i][xedg+j+1][s] = msg->state[jstate++];
        }
        for(idx_t s = 0; s < stickorder[prtidx][xvtx+i][xedg+j+1].size(); ++s) {
          stickorder[prtidx][xvtx+i][xedg+j+1][s] = msg->stick[jstick++];
        }
      }
    }
  }
  CkAssert(jstate == msg->nstate);
  CkAssert(jstick == msg->nstick);

  // cleanup
  delete msg;

  // When all parts are gathered from all other data,
  // Perform reordering of vertex indices and start reordering
  if (++cpdat == npdat*nprt) {
    // cleanup finished data structures?
    vtxdistmetis.clear();
    edgdistmetis.clear();
    partmetis.clear();
    vtxidxpart.clear();
    vtxmodidxpart.clear();
    xyzpart.clear();
    adjcypart.clear();
    edgmodidxpart.clear();
    statepart.clear();
    stickpart.clear();

    // collect order parts
    std::string orderprts;
    for (idx_t jprt = 0; jprt < nprt; ++jprt) {
      std::ostringstream orderprt;
      orderprt << " " << norderprt[jprt];
      orderprts.append(orderprt.str());
    }
    CkPrintf("  Reordered File: %" PRIidx "   Vertices: %" PRIidx " {%s }\n",
             datidx, norderdat, orderprts.c_str());

    // set up containers
    vtxmodidx.resize(norderdat);
    xyz.resize(norderdat*3);
    adjcy.resize(norderdat);
    edgmodidx.resize(norderdat);
    state.resize(norderdat);
    stick.resize(norderdat);

    // Go through part data and reorder
    idx_t xvtx = 0;
    for (idx_t jprt = 0; jprt < nprt; ++jprt) {
      CkPrintf("  Reordering part %" PRIidx "\n", xprt+jprt);
      // reorder based on modidx
      std::sort(vtxorder[jprt].begin(), vtxorder[jprt].end());

      // add to data structures
      for (idx_t i = 0; i < norderprt[jprt]; ++i) {
        // vtxmodidx
        vtxmodidx[xvtx+i] = vtxorder[jprt][i].modidx;
        // xyz
        xyz[(xvtx+i)*3+0] = xyzorder[jprt][(vtxorder[jprt][i].vtxidxloc)*3+0];
        xyz[(xvtx+i)*3+1] = xyzorder[jprt][(vtxorder[jprt][i].vtxidxloc)*3+1];
        xyz[(xvtx+i)*3+2] = xyzorder[jprt][(vtxorder[jprt][i].vtxidxloc)*3+2];
        // state (for vertex)
        state[xvtx+i].push_back(stateorder[jprt][vtxorder[jprt][i].vtxidxloc][0]);
        stick[xvtx+i].push_back(stickorder[jprt][vtxorder[jprt][i].vtxidxloc][0]);
      }

      // increment xvtx
      xvtx += norderprt[jprt];
    }

    // Prepare for reordering
    cpdat = 0;
    
    // Broadcast ordering in order of file
    if (cpdat == datidx) {
      mOrder *morder = BuildOrder();
      thisProxy.Reorder(morder);
    }
  }
}


// Reorder indices given the reordering
//
void GeNet::Reorder(mOrder *msg) {
  // Sanity check
  CkAssert(msg->datidx == cpdat);

  // Add to vtxdist
  vtxdist[cpdat+1] = vtxdist[cpdat] + msg->nvtx;

  // create map
  std::unordered_map<idx_t, idx_t> oldtonew;
  for (idx_t i = 0; i < msg->nvtx; ++i) {
    oldtonew[msg->vtxidxold[i]] = vtxdist[cpdat] + msg->vtxidxnew[i];
  }

  // cleanup
  delete msg;

  // Reorder edges
  idx_t xvtx = 0;
  for (idx_t jprt = 0; jprt < nprt; ++jprt) {
    for (idx_t i = 0; i < norderprt[jprt]; ++i) {
      edgorder.clear();
      for (idx_t j = 0; j < adjcyorder[jprt][i].size(); ++j) {
        if (oldtonew.find(adjcyorder[jprt][i][j]) == oldtonew.end()) {
          continue;
        }
        else {
          edgorder.push_back(edgorder_t());
          edgorder.back().edgidx = oldtonew[adjcyorder[jprt][i][j]];
          edgorder.back().modidx = edgmodidxorder[jprt][i][j];
          edgorder.back().state = stateorder[jprt][i][j+1];
          edgorder.back().stick = stickorder[jprt][i][j+1];
        }
      }
      // sort newly added indices
      std::sort(edgorder.begin(), edgorder.end());
      // add indices to data structures
      for (idx_t j = 0; j < edgorder.size(); ++j) {
        adjcy[xvtx+i].push_back(edgorder[j].edgidx);
        edgmodidx[xvtx+i].push_back(edgorder[j].modidx);
        state[xvtx+i].push_back(edgorder[j].state);
        stick[xvtx+i].push_back(edgorder[j].stick);
      }
    }
    xvtx += norderprt[jprt];
  }


  // Move to next part
  ++cpdat;
  // return control to main when done
  if (cpdat == npdat) {
    contribute(0, NULL, CkReduction::nop);
  }
  // Broadcast ordering in order of file
  else if (cpdat == datidx) {
    mOrder *morder = BuildOrder();
    thisProxy.Reorder(morder);
  }
}


/**************************************************************************
* Ordering messages
**************************************************************************/

// Build Order (from old vertices to new indices)
//
mOrder* GeNet::BuildOrder() {
  // Initialize connection message
  int msgSize[MSG_Order];
  msgSize[0] = norderdat;   // vtxidxold
  msgSize[1] = norderdat;   // vtxidxnew
  mOrder *morder = new(msgSize, 0) mOrder;
  // sizes
  morder->datidx = datidx;
  morder->nvtx = norderdat;

  // set up counters
  idx_t jvtxidx = 0;

  // load data
  for (idx_t jprt = 0; jprt < nprt; ++jprt) {
    for (idx_t i = 0; i < norderprt[jprt]; ++i) {
      morder->vtxidxold[jvtxidx] = vtxorder[jprt][i].vtxidx;
      morder->vtxidxnew[jvtxidx] = jvtxidx;
      ++jvtxidx;
    }
  }
  CkAssert(jvtxidx == norderdat);

  return morder;
}
