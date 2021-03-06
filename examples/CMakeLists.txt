add_executable(hd.exe heatdis.c)
target_link_libraries(hd.exe fti.static ${MPI_C_LIBRARIES} m)
target_include_directories(hd.exe PUBLIC ${CMAKE_SOURCE_DIR}/include)
set_property(TARGET hd.exe APPEND PROPERTY COMPILE_FLAGS ${MPI_C_COMPILE_FLAGS})
set_property(TARGET hd.exe APPEND PROPERTY LINK_FLAGS ${MPI_C_LINK_FLAGS})

add_executable(hd2.exe heatd2.c)
target_link_libraries(hd2.exe fti.static ${MPI_C_LIBRARIES} m)
target_include_directories(hd2.exe PUBLIC ${CMAKE_SOURCE_DIR}/include)
set_property(TARGET hd2.exe APPEND PROPERTY COMPILE_FLAGS ${MPI_C_COMPILE_FLAGS})
set_property(TARGET hd2.exe APPEND PROPERTY LINK_FLAGS ${MPI_C_LINK_FLAGS})

if(ENABLE_FORTRAN)
    add_executable(hdf.exe fheatdis.f90)
    target_link_libraries(hdf.exe fti_f90.static ${MPI_Fortran_LIBRARIES} m)
    set_property(TARGET hdf.exe APPEND PROPERTY COMPILE_FLAGS ${MPI_Fortran_COMPILE_FLAGS})
    set_property(TARGET hdf.exe APPEND PROPERTY LINK_FLAGS ${MPI_Fortran_LINK_FLAGS})
    if(NOT MPI_USE_MOD)
	    target_compile_definitions(hdf.exe PRIVATE -DMPI_USE_HEADER)
    endif()
	target_compile_options(hdf.exe PRIVATE -cpp)
endif()

if (ENABLE_GPU)
  add_subdirectory(GPU)
endif()

if(NOT $MPIRUN)
	set(MPIRUN mpirun)
endif()

add_custom_target(init
    COMMAND cp configBkp.fti config.fti
)

add_custom_target(runall
	DEPENDS hd hd2 hdf
)

add_custom_target(hd
	COMMAND ${MPIRUN} -n 16 ./hd.exe 4 config.fti
)

add_custom_target(hd2
	COMMAND mkdir results
	COMMAND ${MPIRUN} -n 16 ./hd2.exe config.fti
)

add_custom_target(hdf
	COMMAND ${MPIRUN} -n 16 ./hdf.exe
)
