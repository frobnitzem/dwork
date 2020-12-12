# dwork

Task graph scheduler with a minimalistic API.


# redis database fields:

  * Central fields:

    - ready ~> list of "ready" <TaskID>s (removed when assigned)

    - workers ~> set of active <Worker>s

    - assigned/<Worker> ~> set of <TaskID>s assigned to worker

  * Additional field to handle task-associated info:

    - waiting ~> set of "waiting" <TaskID>s (removed when ready)

    - tasks/<TaskID> ~> protobuf <Task> message associated to <TaskID>

  * Additional fields needed to handle work stealing:

    - location/<TaskID> ~> (<Worker>, <Worker>) first and last-known owners of task

# routing

  Full task references are (<TaskID>, <Worker>).  When a reference
is encountered during processing, it triggers a `retrieve()`,
`add_successor()` or a `delete()` message.
The first two use the last-known task owner, which is updated
if incorrect (lookup caching).
Delete messages traverse the entire task
ownership chain, and need to use the `first-known` task owner link.
