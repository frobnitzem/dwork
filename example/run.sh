get() {
    ./dquery steal -q
}
fin() {
    ./dquery complete "$@"
}

while true; do
    task=`get` || break
    echo "task: $task"
    fin "$task"
done

