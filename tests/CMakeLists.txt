enable_testing()

# define a function to simplify adding tests
function(do_test target)
  add_executable(${target} ${target}.cc rand_dag.c)
  target_link_libraries(${target} PUBLIC dwork)
  add_test(NAME ${target} COMMAND ${target} ${ARGN})
endfunction(do_test)

do_test(dtest)
do_test(rtest)
do_test(qredis)
do_test(proto_writer out.pb)
do_test(taskHeap)
do_test(taskDB)
