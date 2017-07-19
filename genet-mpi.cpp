/**
 * Copyright (C) 2015 Felix Wang
 *
 * Simulation Tool for Asynchrnous Cortical Streams (stacs)
 */

#include <mpi.h>
#include "mpi-interoperate.h"

#include "genet-mpi.h"


/**************************************************************************
* Global Glue Variables
**************************************************************************/
bool doneflag;
bool partflag;


/**************************************************************************
* Main entry point
**************************************************************************/

// Main
//
int main(int argc, char ** argv) {
  /* MPI */
  int netfiles, datidx;
  MPI_Comm comm;
  
  // Initialize MPI
  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &datidx);
  MPI_Comm_size(MPI_COMM_WORLD, &netfiles);
  MPI_Comm_dup(MPI_COMM_WORLD, &comm);
  // Initialize Charm++
  doneflag = true;
  CharmLibInit(comm, argc, argv);
  MPI_Barrier(comm);

  // Exit if not initialized
  if (doneflag) {
    // Finalize Charm++
    CharmLibExit();
    // Finalize MPI
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Finalize();

    // exit
    return 0;
  }

  // Control Loop
  doneflag = false;
  partflag = false;
  while (!doneflag) {
    doneflag = true;
    // Partition using ParMETIS
    if (partflag) {
      partflag = false;
      GeNet_Partition(argc, argv);
    }

    // Main control loop
    GeNet_MainControl();
    MPI_Barrier(comm);
  }
  
  // Finalize Charm++
  CharmLibExit();
  // Finalize MPI
  MPI_Barrier(MPI_COMM_WORLD);
  MPI_Finalize();

  // exit successfully
  return 0;
}


/**************************************************************************
* Charm++ MPI glue code
**************************************************************************/

// Not done with control loop
//
void GeNet_UnSetDoneFlag() {
  doneflag = false;
}

// Set the partitioning flag
//
void GeNet_SetPartFlag() {
  partflag = true;
}
