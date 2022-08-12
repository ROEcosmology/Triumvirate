from libcpp.string cimport string


cdef extern from "include/parameters.hpp":
    cppclass CppParameterSet "trv::scheme::ParameterSet":
        string catalogue_dir
        string measurement_dir
        string data_catalogue_file
        string rand_catalogue_file
        # string catalogue_header
        string output_tag

        double boxsize[3]
        int ngrid[3]
        # double padfactor

        # string alignment
        # string padscale
        string assignment
        string interlace
        string norm_convention
        string shotnoise_convention

        string catalogue_type
        string measurement_type

        int ell1
        int ell2
        int ELL

        int i_wa
        int j_wa

        string binning
        string form

        double kmin
        double kmax
        int num_kbin
        int ith_kbin
        double rmin
        double rmax
        int num_rbin
        int ith_rbin

        double volume
        int nmesh

        int printout()
        int validate()


cdef class ParameterSet:
    cdef CppParameterSet* thisptr
    cdef string _source
    cdef string _status
    cdef dict _params
    cdef object _logger
