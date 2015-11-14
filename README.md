# genet
  Generate Spiking Neural Network

  Parallel build, with network partitioning via ParMETIS
  Model specification using YAML

# Dependencies
  * C++11 (e.g. gcc-4.8)
  * MPI (e.g. mpich-3.1.4)
  * Charm++ (e.g. charm-6.6.1)
  * YAML-cpp (YAML 1.2 spec)
  * METIS (metis-5.1.0)
  * ParMETIS (parmetis-4.0.3)

# Settings
  In `include/metis.h` (for METIS and ParMETIS):
  - `IDXTYPEWIDTH 64`
  - `REALTYPEWIDTH 64`

# Compiling Charm++
  * On Linux: `./build charm++ mpi-linux-x86_64 -j8 -O3`
  * On OSX `./build charm++ mpi-darwin-x86_64 -j8 -O3`
  * Set environment variable `CHARMDIR` to where you installed Charm++

# Building genet
  * `make`

# Configuring network
  - Default config file is `config.yml`
  - `npdat` is the number of processors the network data will be stored to
  - `npnet` is the number of partitions the network will be split into
  - `filebase` is the location of where to read/write the files for the network

# Running genet
  - `charmrun +p{npdat} ./genet [config file] [mode]`
  - `npdat` should match the one in the config file (defaults to `config.yml`)
  - The different modes are:
    1. `build` builds the network (default if no mode specified)
    2. `part` partition the network (requires network to have been built)
    3. `order` reorders the network based on partitioning (requires partitioning)

# Compiling on OSX
  1. For the OSX compilation, you'll want to have XCode installed.
  2. For some of the dependencies, you'll want to get them from Homebrew (http://brew.sh/), e.g.:
    - `brew install mpich2`
    - `brew install yaml-cpp`
  3. For METIS and ParMETIS, use the versions from the Karypis lab page (make sure you have MPI installed before trying to install ParMETIS)
  4. For Charm++, to enable thread local storage for the installation on OSX (needed for the ParMETIS MPI interoperation), go to `$CHARMDIR/src/arch/mpi-darwin-x86_64/conv_mach.sh`
    - Set `MACOSX_DEPLOYMENT_TARGET=10.7`
