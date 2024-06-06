/*
    Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

    Official repository: https://github.com/cppalliance/mrdocs
*/

const https = require('https');
const request = require('sync-request');
const fs = require("fs");

function humanizeBytes(bytes) {
    const sizes = ['Bytes', 'KB', 'MB', 'GB', 'TB'];
    if (bytes == 0) return '0 Byte';
    const i = parseInt(Math.floor(Math.log(bytes) / Math.log(1024)));
    return Math.round(bytes / Math.pow(1024, i), 2) + ' ' + sizes[i];
}

function humanizeDate(date) {
    const options = { year: 'numeric', month: 'long', day: 'numeric' };
    return new Date(date).toLocaleDateString('en-US', options);
}

module.exports = function (registry) {
    registry.blockMacro('mrdocs-releases', function () {
        const self = this
        self.process(function (parent, target, attrs) {
            // Collect all release URLs
            let cacheFilenamePath = 'releasesResponse.json';
            let cachePath = `${__dirname}/../build/requests/${cacheFilenamePath}`;
            fs.mkdirSync(`${__dirname}/../build/requests/`, { recursive: true });
            const readFromCacheFile = fs.existsSync(cachePath) && fs.statSync(cachePath).mtime > new Date(Date.now() - 1000 * 60 * 60 * 24);
            const releasesResponse =
                readFromCacheFile ?
                fs.readFileSync(cachePath, 'utf-8') :
                request('GET', 'https://api.github.com/repos/cppalliance/mrdocs/releases', {
                    headers: {
                        'User-Agent': 'request'
                    }
                }).getBody('utf-8')
            if (!readFromCacheFile) {
                fs.writeFileSync(cachePath, releasesResponse);
            }
            const releases = JSON.parse(releasesResponse)

            // Create table
            let text = '|===\n'
            text += '|          3+| 🪟 Windows                2+| 🐧 Linux                 \n'
            text += '| 📃 Release | 📦 7z   | 📦 msi  | 📦 zip  | 📦 tar.xz  | 📦 tar.gz  \n'
            for (const release of releases) {
                if (release.name === 'llvm-package') continue
                text += `| ${release.html_url}[${release.name},window=_blank]\n\n${humanizeDate(release.published_at)} `
                const assetSuffixes = ['win64.7z', 'win64.msi', 'win64.zip', 'Linux.tar.xz', 'Linux.tar.gz']
                for (const suffix of assetSuffixes) {
                    const asset = release.assets.find(asset => asset.name.endsWith(suffix))
                    if (asset) {
                        text += `| ${asset.browser_download_url}[🔗 ${asset.name}]\n\n(${humanizeBytes(asset.size)}) `
                    } else {
                        text += '| - '
                    }
                }
                text += '\n'
            }
            text += '|===\n'
            return self.parseContent(parent, text)
        })
    })
}
