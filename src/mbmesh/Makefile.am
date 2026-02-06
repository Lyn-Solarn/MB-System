# Makefile template for mbmesh

AM_CPPFLAGS = 
AM_CPPFLAGS += -I${top_srcdir}/src/mbio 
AM_CPPFLAGS += -I${top_srcdir}/src/mbaux
AM_CPPFLAGS += -I${top_srcdir}/src/utilities

bin_PROGRAMS = mbmesh

mbmesh_SOURCES = \
	mbmesh.cpp \
	mbmesh_core.cpp \
	mbmesh_io.cpp \
	mbmesh_geometry.cpp \
	mbmesh.h

mbmesh_LDADD = \
	${top_builddir}/src/mbio/libmbio.la \
	${top_builddir}/src/mbaux/libmbaux.la \
	${top_builddir}/src/utilities/libmbutil.la

# Include GMT libraries if available for enhanced functionality
if BUILD_GMT
mbmesh_LDADD += $(GMT_LIBS)
AM_CPPFLAGS += $(GMT_CPPFLAGS)
endif

# Include GSF libraries if available for reading GSF format files  
if BUILD_GSF
mbmesh_LDADD += $(GSF_LIBS)
AM_CPPFLAGS += $(GSF_CPPFLAGS)
endif

# Include NetCDF libraries for grid file support
if BUILD_NETCDF
mbmesh_LDADD += $(NETCDF_LIBS)
AM_CPPFLAGS += $(NETCDF_CPPFLAGS)
endif

# Include PROJ libraries for coordinate transformations
if BUILD_PROJ
mbmesh_LDADD += $(PROJ_LIBS)
AM_CPPFLAGS += $(PROJ_CPPFLAGS)
endif
