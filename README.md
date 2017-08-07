## ThreadInstrument library</p>

 ThreadInstrument is a C++ library to help analyze multithreaded applications by (1) profiling activities and (2) logging events. 

###  Installation

 Building and installation are based on [CMake](https://cmake.org) following the usual process. If, for example, CMake is directed to generate UNIX makefiles, the library, its tests and its documentation will be built by running `make`. The tests that come with the library can be optionally tried at this point by running `make check`. Finally, the library is installed by running `make install`. The documentation is based on [Doxygen](http://www.doxygen.org) and it can be found in the `doc` directory of the building directory. After installation it can be found in `${CMAKE_INSTALL_PREFIX}/share/thread_instrument/doc`
 
### License

ThreadInstrument is available under the MIT license.
