/**
 * Copyright (C) 2015 Felix Wang
 *
 * Simulation Tool for Asynchrnous Cortical Streams (stacs)
 *
 * gepart.cpp
 * partition the network adjacency using the ParMETIS libraries
 */

#include <cstdio>
#include <cstring>
#include <string>
#include <random>
#include <parmetis.h>
#include <mpi.h>
#include "typedefs.h"

// Using yaml-cpp (specification version 1.2)
#include "yaml-cpp/yaml.h"

#define MAXLINE 1280000
#define FILENAMESIZE 256

int main(int argc, char ** argv) {
  /* MPI */
  int npdat, datidx;
  MPI_Comm comm;
  double tstart, tfinish;
  /* ParMetis */
  int metisresult;
  idx_t *metisdist; // Distribution of vertices/edges
  idx_t *vtxdist;
  real_t *xyz;
  idx_t *xadj;
  idx_t *adjcy;
  idx_t *part;
  idx_t edgecut;
  idx_t npnet;
  idx_t *vwgt;
  idx_t *adjwgt = NULL;
  idx_t wgtflag = 2; // weights on the vertices only
  idx_t numflag = 0; // C-style numbering (starting with 0)
  idx_t ndims = 3;   // the number of dimensions of the space
  idx_t ncon = 1;    // number of weights that each vertex has
  real_t *tpwgts;    // fraction of vertex weight per part
  real_t ubvec;      // tolerance for balancing (1 is perfect balance)
  idx_t options[3];  // [0] is flag to use non default values
                     // [1] is information parmetis to output
                     // [2] is the random seed to use
  idx_t rngseed;
  /* Sizes */
  idx_t nvtx;
  idx_t nedg;
  /* Files */
  FILE *pDist;
  FILE *pAdjcy;
  FILE *pCoord;
  FILE *pPart;
  std::string filebase;
  char filename[FILENAMESIZE];
  char *line;
  char *oldstr, *newstr;
  

  // Initialize MPI
  //
  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD,&datidx);
  MPI_Comm_size(MPI_COMM_WORLD,&npdat);
  MPI_Comm_dup(MPI_COMM_WORLD, &comm);

  // Get command line arguments
  std::string configfile;
  if (argc < 2) {
    configfile = "config.yml"; // default
  }
  else {
    configfile = argv[1];
  }

  // Load configuration
  if (datidx == 0) {
    printf("Loading config from %s\n", configfile.c_str());
    YAML::Node config;
    try {
      config = YAML::LoadFile(configfile);
    } catch (YAML::BadFile& e) {
      printf("  %s\n", e.what());
      return 1;
    }

    // Get configuration
    // Network data file
    try {
      filebase = config["filebase"].as<std::string>();
      if (filebase.size() && filebase.size()+16 < FILENAMESIZE) {
        std::strncpy(filename, filebase.c_str(), filebase.size());
        filename[filebase.size()] = '\0';
      }
      else {
        printf("  filebase too long: %s\n", filebase.c_str());
      }
    } catch (YAML::RepresentationException& e) {
      printf("  filebase: %s\n", e.what());
      return 1;
    }
    // Number of data files
    try {
      idx_t npdatconf = config["npdat"].as<idx_t>();
      if (npdat != npdatconf) {
        printf("Error: npdat (%" PRIidx ") does not match MPI_Comm_size (%d)\n"
            "       Use '-np %d' to set %d ranks in MPI\n",
            npdatconf, npdat, npdat, npdat);
        return 1;
      }
    } catch (YAML::RepresentationException& e) {
      printf("  npdat: %s\n", e.what());
      return 1;
    }
    // Number of network parts
    try {
      npnet = config["npnet"].as<idx_t>();
    } catch (YAML::RepresentationException& e) {
      printf("  npnet: %s\n", e.what());
      return 1;
    }
    // Random number seed
    try {
      rngseed = config["rngmetis"].as<idx_t>();
    } catch (YAML::RepresentationException& e) {
      std::random_device rd;
      rngseed = rd();
      printf("  rngmetis not defined, seeding with: %" PRIidx "\n", rngseed);
    }
  }
  // Broadcast configuration
  MPI_Bcast(&npnet, 1, IDX_T, 0, comm);
  MPI_Bcast(&rngseed, 1, IDX_T, 0, comm);
  MPI_Bcast(filename, FILENAMESIZE, MPI_CHAR, 0, comm);
  int filesize = 0;
  while (filename[filesize] != '\0') { ++filesize; }
  filebase.assign(filename, filesize);

  // display some information
  if (datidx == 0) {
    // Display configuration information
    printf("Partition Network for STACS (gepart)\n"
           "Loaded config from %s\n"
           "  Data Files (npdat):     %d\n"
           "  Network Parts (npnet):  %" PRIidx "\n",
           configfile.c_str(), npdat, npnet);
  }
 
  // Vertex and Edge distributions
  //
  metisdist = new idx_t[(npdat+1)*2];
  line = new char[MAXLINE]; 
  if (line == NULL) {
    printf("Error allocating line in\n");
    return 1;
  }
  
  // read in distribution on rank 0 and broadcast
  if (datidx == 0) {
    sprintf(filename, "%s.metis", filebase.c_str());
    pDist = fopen(filename,"r");
    if (pDist == NULL) {
      printf("Error opening metis file\n");
      MPI_Finalize();
      return 1;
    }
    for (int i = 0; i < npdat+1; ++i) {
      while(fgets(line, MAXLINE, pDist) && line[0] == '%');
      oldstr = line;
      newstr = NULL;
      // vtx
      metisdist[i*2] = strtoll(oldstr, &newstr, 10);
      oldstr = newstr;
      // edg
      metisdist[i*2+1] = strtoll(oldstr, &newstr, 10);
    }
    fclose(pDist);
  }
  MPI_Bcast(metisdist, (npdat+1)*2, IDX_T, 0, comm);

  // Set up distributions
  vtxdist = new idx_t[npdat+1];
  for (idx_t i = 0; i < npdat+1; ++i) {
    vtxdist[i] = metisdist[i*2];
  }
  nvtx = metisdist[(datidx+1)*2] - metisdist[datidx*2];
  nedg = metisdist[(datidx+1)*2+1] - metisdist[datidx*2+1];
  printf("(nvtx nedg) on %d: %" PRIidx " %" PRIidx" \n", datidx, nvtx, nedg);
  xadj = new idx_t[nvtx+1];
  adjcy = new idx_t[nedg];
  xyz = new real_t[nvtx*ndims];
  vwgt = new idx_t[nvtx];
  tpwgts = new real_t[npnet];
  part = new idx_t[nvtx];

  // Read in Graph Information
  //
  sprintf(filename, "%s.adjcy.%d", filebase.c_str(), datidx);
  pAdjcy = fopen(filename,"r");
  sprintf(filename, "%s.coord.%d", filebase.c_str(), datidx);
  pCoord = fopen(filename, "r");
  if (pAdjcy == NULL || pCoord == NULL) {
    printf("Error opening network files on %d\n",datidx);
    MPI_Finalize();
    return 1;
  }
  
  // prefixes start at zero
  xadj[0] = 0;
  idx_t jadjcy = 0;
  for (idx_t i = 0; i < nvtx; ++i) {
    // Adjacency information
    while(fgets(line, MAXLINE, pAdjcy) && line[0] == '%');
    oldstr = line;
    newstr = NULL;
    for(;;) {
      idx_t edge = strtoidx(oldstr, &newstr, 10);
      if (edge == 0 && oldstr != line)
        break;
      oldstr = newstr;
      // adj
      adjcy[jadjcy++] = edge;
    }
    // xadj
    xadj[i+1] = jadjcy;

    // vwgt
    vwgt[i] = 1;

    // Coordinates
    while(fgets(line, MAXLINE, pCoord) && line[0] == '%');
    oldstr = line;
    newstr = NULL;
    for(int j = 0; j < ndims; j++) {
      // xyz
      xyz[i*ndims+j] = strtoreal(oldstr, &newstr);
      oldstr = newstr;
    }
  }
  fclose(pAdjcy);
  fclose(pCoord);
  
  if (datidx == 0) {
    printf("Network order: %" PRIidx "\n", vtxdist[npdat]);
  }
  
  // Parmetis Balance
  ubvec = 0.0;
  for (idx_t i = 0; i < npnet; i++) {
    tpwgts[i] = 1.0/npnet;
    ubvec += tpwgts[i];
  }
  tpwgts[0] += (1.0 - ubvec);
  ubvec = 1.000001;

  // Compute partitioning
  options[0] = 1;
  options[1] = PARMETIS_DBGLVL_TIME |
               PARMETIS_DBGLVL_INFO |
               PARMETIS_DBGLVL_PROGRESS; // Debug (timing, matching)
  options[2] = rngseed; // Random seed
  

  // start timing
  tstart = MPI_Wtime();
  
  // Partition
  metisresult = ParMETIS_V3_PartGeomKway(vtxdist, xadj, adjcy,
                                         vwgt, adjwgt, &wgtflag, &numflag,
                                         &ndims, xyz, &ncon, &npnet,
                                         tpwgts, &ubvec, options,
                                         &edgecut, part, &comm);
  if (metisresult != METIS_OK) {
    if (datidx == 0) {
      printf("Error during partitioning\n");
    }
  }

  // finish timing
  tfinish = MPI_Wtime();
  tfinish -= tstart;
  MPI_Reduce(&tfinish, &tstart, 1, MPI_DOUBLE, MPI_SUM, 0, comm);

  // Edge cut for the partition
  if (datidx == 0) {
    printf("edgecut: %" PRIidx "\n", edgecut);
    printf("%" PRIidx" on %d: %.12e\n", npnet, npdat, tstart/npdat);
  }

  // Write partitioning
  //
  sprintf(filename,"%s.part.%d",filebase.c_str(),datidx);
  pPart = fopen(filename,"w");
  if (pPart == NULL) {
    printf("Error opening partition file\n");
    return 1;
  }
  for (idx_t i = 0; i < nvtx; ++i) {
    fprintf(pPart, "%" PRIidx "\n", part[i]);
  }
  fclose(pPart);
  
  // Cleanup
  delete[] metisdist;
  delete[] vtxdist;
  delete[] xyz;
  delete[] xadj;
  delete[] adjcy;
  delete[] part;
  delete[] vwgt;
  delete[] tpwgts;

  // Finalize
  MPI_Finalize();
  return 0;
}
