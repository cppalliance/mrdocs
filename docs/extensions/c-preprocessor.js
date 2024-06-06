/*
    Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

    Official repository: https://github.com/cppalliance/mrdocs
*/

module.exports = function (registry) {
    // Make sure registry is defined
    if (!registry) {
        throw new Error('registry must be defined');
    }

    registry.block('c-preprocessor', function () {
        const self = this
        var compiler = require("c-preprocessor");
        self.onContext('example')
        self.process((parent, reader) => {
            let code = reader.getLines().join('\n')
            let processed = ''
            const options = {
                commentEscape: true
            }
            compiler.compile(code, options, function (err, result) {
                if (err)
                    return console.log(err);
                processed = result
            });
            // Remove any comments from the processed code
            processed = processed.replace(/\/\/.*\n/g, '')
            processed = processed.replace(/\/\*[\s\S]*?\*\//g, '')
            return self.createBlock(parent, 'open', processed)
        })
    })
}
