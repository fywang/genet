/**
 * Copyright (C) 2015 Felix Wang
 *
 * Simulation Tool for Asynchrnous Cortical Streams (stacs)
 *
 * Glue code between Charm++ and MPI
 */

#ifndef __STACS_GENET_MPI_H__
#define __STACS_GENET_MPI_H__

// Calls main control loop
void GeNet_MainControl();

// Calls partitioning
int GeNet_Partition(int argc, char **argv);

// Unsets done flag
void GeNet_UnSetDoneFlag();

// Sets partitioning flag
void GeNet_SetPartFlag();

#endif //__STACS_GENET_MPI_H__
