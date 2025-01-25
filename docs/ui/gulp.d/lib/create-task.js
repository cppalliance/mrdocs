'use strict'

const metadata = require('undertaker/lib/helpers/metadata')
const { watch } = require('gulp')

/** This function creates a Gulp task from an object with the task properties.

    A Gulp task is a function that can be run from the command line
    using Gulp CLI.
    The task can perform various operations such as file manipulation,
    code transpiling, etc.

    This function takes an object as an argument, which contains
    properties that define the task.
    The properties include the task's name, description, options,
    the function to be called when the task is run, and the files
    or directories to watch.

    The returned task is a function object with extra fields
    defining values required to export the task as a Gulp task.

    The function does the following operations:
    - If a name is provided, it sets the displayName of the
      function to the provided name.
    - If the displayName of the function is '<series>'
      or '<parallel>', it updates the label of the function's metadata.
    - If a loop is provided, it creates a watch task.
      The watch task will run the original task whenever the specified
      files or directories change.
    - If a description is provided, it sets the description of the function.
    - If options are provided, it sets the flags of the function.

    @param {Object} taskObj - The object containing the task properties.
    @param {string} taskObj.name - The name of the task.
    @param {string} taskObj.desc - The description of the task.
    @param {Object} taskObj.opts - The options for the task.
    @param {Function} taskObj.call - The function to be called when the task is run.
    @param {Array} taskObj.loop - The files or directories to watch.

    @returns {Function} The created Gulp task.
 */
function createTask ({ name, desc, opts, call: fn, loop }) {
  // If a name is provided, set the displayName of the function to the provided name
  if (name) {
    const displayName = fn.displayName
    // If the displayName of the function is '<series>' or '<parallel>', update the label of the function's metadata
    if (displayName === '<series>' || displayName === '<parallel>') {
      metadata.get(fn).tree.label = `${displayName} ${name}`
    }
    fn.displayName = name
  }

  // If a loop is provided, create a watch task
  if (loop) {
    const delegate = fn
    name = delegate.displayName
    delegate.displayName = `${name}:loop`
    fn = () => watch(loop, { ignoreInitial: false }, delegate)
    fn.displayName = name
  }

  // If a description is provided, set the description of the function
  if (desc) fn.description = desc

  // If options are provided, set the flags of the function
  if (opts) fn.flags = opts

  // Return the created task
  return fn
}

module.exports = createTask
