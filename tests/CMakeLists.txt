enable_testing()

# define a function to simplify adding tests
function(do_test target)
  add_executable(${target} ${target}.cc rand_dag.c)
  target_link_libraries(${target} PUBLIC dwork)
  add_test(NAME ${target} COMMAND ${target})
endfunction(do_test)

do_test(dtest)
do_test(rtest)
