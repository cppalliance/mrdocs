'use strict'

/** This function exports multiple Gulp tasks at once.

    If the first task appears more than once in the list of tasks,
    it removes the first task from the list and assigns it to the
    `default` property of the returned object.

    This means that the first task in the list will be the
    default task that Gulp runs when no task name is provided
    in the command line.

    If no tasks are provided, it simply returns an empty object.

    @param {...Function} tasks - The tasks to be exported.

    @returns {Object} An object where each task is a property
    of the object.

    The property key is the task's name, and the value is
    the task itself.

 */
function exportTasks (...tasks) {
  // Initialize an empty object
  const seed = {}

  // Check if there are tasks provided
  if (tasks.length) {
    // Check if the first task appears more than once in the list of tasks
    if (tasks.lastIndexOf(tasks[0]) > 0) {
      // Remove the first task from the list
      const task1 = tasks.shift()

      // Assign the first task to the `default` property of the `seed` object
      seed.default = Object.assign(task1.bind(null), { description: `=> ${task1.displayName}`, displayName: 'default' })
    }

    // Reduce the remaining tasks into the `seed` object
    // Each task is added as a property of the `seed` object
    // The task's name is the property key and the task itself is the value
    return tasks.reduce((acc, it) => (acc[it.displayName || it.name] = it) && acc, seed)
  } else {
    // If no tasks are provided, return the `seed` object
    return seed
  }
}

module.exports = exportTasks
