#!/bin/bash

BIN=.

# start task hub
$BIN/dhub &

# add 5 jobs, ignoring returned job-IDs
for((i=1;i<=5;i++)); do
    $BIN/dquery create "echo $i"
done

# start 2 workers
for((j=1;j<=2;j++)); do
  $BIN/tests/kworker &
done

wait
# wait 60 seconds for tasks to run
# TODO: poll until "task count == 0" or subscribe to 'queue empty' notification
#sleep 60

# shutdown gracefully by killing task hub
kill %1
wait
