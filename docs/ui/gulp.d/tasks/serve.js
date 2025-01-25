'use strict'

const connect = require('gulp-connect')
const os = require('os')

// Constants
const ANY_HOST = '0.0.0.0'
const URL_RX = /(https?):\/\/(?:[^/: ]+)(:\d+)?/

/** Decorate the log messages of gulp-connect.

    This function replaces the log function of gulp-connect
    with a new function that modifies the log messages.

    If a message starts with 'Server started ',
    it replaces the URL in the message with 'localhost' and
    the local IP address.

    @param {Object} _ - The request object (not used).
    @param {Object} app - The gulp-connect app.
    @returns {Array} An empty array (required by gulp-connect).
 */
function decorateLog (_, app) {
  const _log = app.log
  app.log = function modifyLogMessage (msg) {
    if (msg.startsWith('Server started ')) {
      const localIp = getLocalIp()
      const replacement = '$1://localhost$2' + (localIp ? ` and $1://${localIp}$2` : '')
      msg = msg.replace(URL_RX, replacement)
    }
    _log(msg)
  }
  return []
}

/** Get the local IP address.

    This function iterates over the network interfaces of
    the OS and returns the first non-internal IPv4 address it finds.
    If no such address is found, it returns 'localhost'.

    @returns {string} The local IP address or 'localhost'.
 */
function getLocalIp () {
  for (const records of Object.values(os.networkInterfaces())) {
    for (const record of records) {
      if (!record.internal && record.family === 'IPv4') return record.address
    }
  }
  return 'localhost'
}

/** Serve a directory using gulp-connect.

    This function starts a server using gulp-connect to serve the specified root directory.
    It also sets up a middleware to decorate the log messages if the host is ANY_HOST.
    When the server is closed, it calls the done callback.
    If a watch function is provided, it is called after the server is started.

    @param {string} root - The root directory to serve.
    @param {Object} opts - The options for gulp-connect.
    @param {Function} watch - The function to call after the server is started.
    @returns {Function} A function that starts the server when called.
 */
function serve (root, opts = {}, watch = undefined) {
  return function startServer (done) {
    connect.server({ ...opts, middleware: opts.host === ANY_HOST ? decorateLog : undefined, root }, function () {
      this.server.on('close', done)
      if (watch) watch()
    })
  }
}

module.exports = serve
