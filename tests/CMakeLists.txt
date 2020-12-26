enable_testing()

# define a function to simplify adding tests
function(do_test target)
  add_executable(${target} ${target}.cc rand_dag.c)
  target_link_libraries(${target} PUBLIC dwork)
  add_test(NAME ${target} COMMAND ${target} ${ARGN})
endfunction(do_test)

do_test(dtest)
do_test(rtest)
do_test(proto_writer out.pb)
do_test(taskHeap)
do_test(taskDB)

set(CMAKE_CXX_FLAGS "-O3 -fopenmp")
add_executable(ktest test_kernel.cc kernel.cc)
add_test(NAME ktest COMMAND ktest 512 512)

find_package(MPI)
if(MPI_FOUND)
    add_executable(kworker kworker.cc kernel.cc)
    target_link_libraries(kworker PUBLIC dwork MPI::MPI_CXX)
    #add_test(NAME kworker COMMAND ${target} 512 512)
endif(MPI_FOUND)

add_executable(stencil stencil.cc kernel.cc)
target_link_libraries(stencil PUBLIC dwork)
