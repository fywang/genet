# genet
  Generate Spiking Neural Network

  Parallel build, with network partitioning via ParMETIS
  Model specification using YAML

# Dependencies
  * C++11 (gcc 4.8 or higher)
  * MPI (e.g. mpich-3.1.4)
  * Charm++ (e.g. charm-6.6.1)
  * YAML-cpp (YAML 1.2 spec)
  * METIS (metis-5.1.0)
  * ParMETIS (parmetis-4.0.3)

# Settings
  In metis.h (for METIS and ParMETIS):
  - IDXTYPEWIDTH 64
  - REALTYPEWIDTH 64

# Compiling Charm++
## On Linux
  `./build charm++ mpi-linux-x86_64 -j8 -O3`
## On OSX
  `./build charm++ mpi-darwin-x86_64 -j8 -O3`

# Compiling on OSX
  Note: if you run into an error about thread local variables while compiling, the culprit is likely because XCode's gcc versions aren't as up to date on these things as one might like. Make sure that the dependencies being compiled are using the right compilers (i.e. the Homebrew ones)
  
  Solution: Install homebrew and compilation tools
  1. `brew tap homebrew/versions`
  2. `brew install gcc48`
  3. make sure it's the 'default' gcc version
    1. update `.bash_profile` and set:
      - `HOMEBREW_CC=gcc-4.8`
      - `HOMEBREW_CXX=g++-4.8`
    2. some additional symlinking may be required (e.g. `ln -s g++-4.8 g++`)
    3. check by running `gcc --version`
  4. `brew install mpich2 --build-from-source`
  5. `brew install yaml-cpp --build-from-source`
    As this might install boost from source too, it may be quicker to:
    - `brew install yaml-cpp`
    - `brew reinstall yaml-cpp --build-from-source`
  6. install metis and parmetis (from Karypis lab page) using gcc-4.8 too
  7. install charm++ with gcc-4.8 (as above)

