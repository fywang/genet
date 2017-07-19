/**
 * Copyright (C) 2015 Felix Wang
 *
 * Simulation Tool for Asynchrnous Cortical Streams (stacs)
 */

#include "genet.h"

/**************************************************************************
* Charm++ Read-Only Variables
**************************************************************************/
extern /*readonly*/ idx_t netparts;
extern /*readonly*/ int netfiles;


/**************************************************************************
* Partition
**************************************************************************/

// Scatter Partitions across Network
//
void GeNet::ScatterPart() {
  // Compute which part goes to which data
  for (int datidxpart = 0; datidxpart < netfiles; ++datidxpart) {
    // Which parts on this data
    idx_t ndivpart = netparts/netfiles;
    idx_t nrempart = netparts%netfiles;
    idx_t nprtpart = ndivpart + (datidxpart < nrempart);
    idx_t xprtpart = datidxpart*ndivpart + (datidxpart < nrempart ? datidxpart : nrempart);

    // Loop through parts
    for (idx_t prtidx = xprtpart; prtidx < xprtpart + nprtpart; ++prtidx) {
      // Count sizes
      idx_t nedgidx = 0;
      idx_t nstate = 0;
      idx_t nstick = 0;
      idx_t nevent = 0;
      for (std::size_t i = 0; i < vtxidxpart[prtidx].size(); ++i) {
        nedgidx += adjcypart[prtidx][i].size();
      }
      for (std::size_t i = 0; i < statepart[prtidx].size(); ++i) {
        nstate += statepart[prtidx][i].size();
      }
      for (std::size_t i = 0; i < stickpart[prtidx].size(); ++i) {
        nstick += stickpart[prtidx][i].size();
      }
      for (std::size_t i = 0; i < eventpart[prtidx].size(); ++i) {
        nevent += eventpart[prtidx][i].size();
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
      msgSize[8] = vtxidxpart[prtidx].size() + 1; // xevent
      msgSize[9] = nevent;                        // diffuse
      msgSize[10] = nevent;                       // type
      msgSize[11] = nevent;                       // source
      msgSize[12] = nevent;                       // index
      msgSize[13] = nevent;                       // data
      mPart *mpart = new(msgSize, 0) mPart;
      // Sizes
      mpart->datidx = datidx;
      mpart->prtidx = prtidx;
      mpart->nvtx = vtxidxpart[prtidx].size();
      mpart->nstate = nstate;
      mpart->nstick = nstick;
      mpart->nevent = nevent;

      // set up counters
      idx_t jedgidx = 0;
      idx_t jstate = 0;
      idx_t jstick = 0;
      idx_t jevent = 0;
      // prefixes start at zero
      mpart->xadj[0] = 0;
      mpart->xevent[0] = 0;

      for (std::size_t i = 0; i < vtxidxpart[prtidx].size(); ++i) {
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
        for (std::size_t j = 0; j < adjcypart[prtidx][i].size(); ++j) {
          // adjncy
          mpart->adjcy[jedgidx] = adjcypart[prtidx][i][j];
          // edgmodidx
          mpart->edgmodidx[jedgidx++] = edgmodidxpart[prtidx][i][j];
        }
        // xevent
        mpart->xevent[i+1] = mpart->xevent[i] + eventpart[prtidx][i].size();
        for (std::size_t j = 0; j < eventpart[prtidx][i].size(); ++j) {
          //event
          mpart->diffuse[jevent] = eventpart[prtidx][i][j].diffuse;
          mpart->type[jevent] = eventpart[prtidx][i][j].type;
          mpart->source[jevent] = eventpart[prtidx][i][j].source;
          mpart->index[jevent] = eventpart[prtidx][i][j].index;
          mpart->data[jevent++] = eventpart[prtidx][i][j].data;
        }
      }
      CkAssert(jedgidx == nedgidx);
      CkAssert(jevent == nevent);
      for (std::size_t i = 0; i < statepart[prtidx].size(); ++i) {
        for (std::size_t s = 0; s < statepart[prtidx][i].size(); ++s) {
          mpart->state[jstate++] = statepart[prtidx][i][s];
        }
        for (std::size_t s = 0; s < stickpart[prtidx][i].size(); ++s) {
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
  idx_t jevent = 0;
  idx_t xvtx = vtxorder[prtidx].size();
  norderdat += msg->nvtx;
  norderprt[prtidx] += msg->nvtx;

  // allocate data
  vtxorder[prtidx].resize(norderprt[prtidx]);
  xyzorder[prtidx].resize(norderprt[prtidx]*3);
  adjcyorder[prtidx].resize(norderprt[prtidx]);
  adjcyreorder[prtidx].resize(norderprt[prtidx]);
  edgmodidxorder[prtidx].resize(norderprt[prtidx]);
  edgmodidxreorder[prtidx].resize(norderprt[prtidx]);
  stateorder[prtidx].resize(norderprt[prtidx]);
  statereorder[prtidx].resize(norderprt[prtidx]);
  stickorder[prtidx].resize(norderprt[prtidx]);
  stickreorder[prtidx].resize(norderprt[prtidx]);
  eventorder[prtidx].resize(norderprt[prtidx]);

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
    for(std::size_t s = 0; s < stateorder[prtidx][xvtx+i][0].size(); ++s) {
      stateorder[prtidx][xvtx+i][0][s] = msg->state[jstate++];
    }
    for(std::size_t s = 0; s < stickorder[prtidx][xvtx+i][0].size(); ++s) {
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
        for(std::size_t s = 0; s < stateorder[prtidx][xvtx+i][xedg+j+1].size(); ++s) {
          stateorder[prtidx][xvtx+i][xedg+j+1][s] = msg->state[jstate++];
        }
        for(std::size_t s = 0; s < stickorder[prtidx][xvtx+i][xedg+j+1].size(); ++s) {
          stickorder[prtidx][xvtx+i][xedg+j+1][s] = msg->stick[jstick++];
        }
      }
    }

    // events
    eventorder[prtidx][xvtx+i].resize(msg->xevent[i+1] - msg->xevent[i]);
    for (idx_t e = 0; e < msg->xevent[i+1] - msg->xevent[i]; ++e) {
      eventorder[prtidx][xvtx+i][e].diffuse = msg->diffuse[jevent];
      eventorder[prtidx][xvtx+i][e].type = msg->type[jevent];
      eventorder[prtidx][xvtx+i][e].source = msg->source[jevent];
      eventorder[prtidx][xvtx+i][e].index = msg->index[jevent];
      eventorder[prtidx][xvtx+i][e].data = msg->data[jevent++];
    }
  }
  CkAssert(jstate == msg->nstate);
  CkAssert(jstick == msg->nstick);
  CkAssert(jevent == msg->nevent);

  // cleanup
  delete msg;

  // When all parts are gathered from all other data,
  // Perform reordering of vertex indices and start reordering
  if (++cpprt == netfiles*nprt) {
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
    eventpart.clear();

    // collect order parts
    std::string orderprts;
    for (idx_t jprt = 0; jprt < nprt; ++jprt) {
      std::ostringstream orderprt;
      orderprt << " " << norderprt[jprt];
      orderprts.append(orderprt.str());
    }
    CkPrintf("  Reordered File: %d   Vertices: %" PRIidx " {%s }\n",
             datidx, norderdat, orderprts.c_str());

    // set up containers
    vtxmodidx.resize(norderdat);
    xyz.resize(norderdat*3);
    adjcy.resize(norderdat);
    edgmodidx.resize(norderdat);
    state.resize(norderdat);
    stick.resize(norderdat);
    event.resize(norderdat);
    eventsourceorder.resize(norderdat);
    eventindexorder.resize(norderdat);

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
        // adjcy
        adjcyreorder[jprt][i] = adjcyorder[jprt][vtxorder[jprt][i].vtxidxloc];
        edgmodidxreorder[jprt][i] = edgmodidxorder[jprt][vtxorder[jprt][i].vtxidxloc];
        // state (for vertex)
        state[xvtx+i].push_back(stateorder[jprt][vtxorder[jprt][i].vtxidxloc][0]);
        stick[xvtx+i].push_back(stickorder[jprt][vtxorder[jprt][i].vtxidxloc][0]);
        // rest of state
        statereorder[jprt][i] = stateorder[jprt][vtxorder[jprt][i].vtxidxloc];
        stickreorder[jprt][i] = stickorder[jprt][vtxorder[jprt][i].vtxidxloc];
        // events
        event[xvtx+i] = eventorder[jprt][vtxorder[jprt][i].vtxidxloc];
        eventsourceorder[xvtx+i].resize(event[xvtx+i].size());
        for (std::size_t e = 0; e < event[xvtx+i].size(); ++e) {
          eventsourceorder[xvtx+i][e] = event[xvtx+i][e].source;
        }
        eventindexorder[xvtx+i].push_back(0);
      }

      // increment xvtx
      xvtx += norderprt[jprt];
    }

    // Take care of any ordering that may have come in
    // Go through ordering list
    for (std::list<mOrder *>::iterator iordidx = ordering.begin(); iordidx != ordering.end(); ++iordidx) {
      if ((*iordidx)->datidx == cpdat) {
        // Perform reordering
        Reorder((*iordidx));
        // Move to next part
        ++cpdat;
        // Erase element from list
        iordidx = ordering.erase(iordidx);
      }
    }

    // Broadcast ordering in order of file
    if (cpdat == datidx) {
      mOrder *morder = BuildOrder();
      thisProxy.Order(morder);
    }
  }
}


// Handle ordering messages
//
void GeNet::Order(mOrder *msg) {
  // Save message for processing
  ordering.push_back(msg);

  if (adjcy.size()) {
    // Go through ordering list
    for (std::list<mOrder *>::iterator iordidx = ordering.begin(); iordidx != ordering.end(); ++iordidx) {
      if ((*iordidx)->datidx == cpdat) {
        // Perform reordering
        Reorder((*iordidx));
        // Move to next part
        ++cpdat;
        // Erase element from list
        iordidx = ordering.erase(iordidx);
      }
    }
  }

  // Check if done reordering
  if (cpdat == netfiles) {
    // reindex events
    for (idx_t i = 0; i < norderdat; ++i) {
      CkAssert(eventindexorder[i].size() == adjcy[i].size()+1);
      // create map
      std::unordered_map<idx_t, idx_t> oldtonew;
      for (std::size_t j = 0; j < eventindexorder[i].size(); ++j) {
        oldtonew[eventindexorder[i][j]] = j;
      }
      // modify index
      for (std::size_t j = 0; j < event[i].size(); ++j) {
        event[i][j].index = oldtonew[event[i][j].index];
      }
    }
    // return control to main when done
    contribute(0, NULL, CkReduction::nop);
  }
  // Broadcast ordering in order of file
  else if (cpdat == datidx) {
    mOrder *morder = BuildOrder();
    thisProxy.Order(morder);
  }
}

// Reordering indices based on given ordering
//
void GeNet::Reorder(mOrder *msg) {
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
      for (std::size_t j = 0; j < adjcyreorder[jprt][i].size(); ++j) {
        if (oldtonew.find(adjcyreorder[jprt][i][j]) == oldtonew.end()) {
          continue;
        }
        else {
          edgorder.push_back(edgorder_t());
          edgorder.back().edgidx = oldtonew[adjcyreorder[jprt][i][j]];
          edgorder.back().modidx = edgmodidxreorder[jprt][i][j];
          edgorder.back().state = statereorder[jprt][i][j+1];
          edgorder.back().stick = stickreorder[jprt][i][j+1];
          edgorder.back().evtidx = j+1;
          // resource events
          for (std::size_t e = 0; e < eventsourceorder[xvtx+i].size(); ++e) {
            if (eventsourceorder[xvtx+i][e] == adjcyreorder[jprt][i][j]) {
              event[xvtx+i][e].source = oldtonew[eventsourceorder[xvtx+i][e]];
            }
          }
        }
      }
      // sort newly added indices
      std::sort(edgorder.begin(), edgorder.end());
      // add indices to data structures
      for (std::size_t j = 0; j < edgorder.size(); ++j) {
        adjcy[xvtx+i].push_back(edgorder[j].edgidx);
        edgmodidx[xvtx+i].push_back(edgorder[j].modidx);
        state[xvtx+i].push_back(edgorder[j].state);
        stick[xvtx+i].push_back(edgorder[j].stick);
        eventindexorder[xvtx+i].push_back(edgorder[j].evtidx);
      }
    }
    xvtx += norderprt[jprt];
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
