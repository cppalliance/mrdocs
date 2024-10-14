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
    const options = {year: 'numeric', month: 'long', day: 'numeric'};
    return new Date(date).toLocaleDateString('en-US', options);
}

/**
 * Formats a given time duration in milliseconds into a human-readable string.
 *
 * @param {number} milliseconds - The time duration in milliseconds.
 * @returns {string} - A formatted string representing the time duration in days, hours, minutes, seconds, and milliseconds.
 */
function formatTime(milliseconds) {
    const seconds = Math.floor(milliseconds / 1000);
    const remainingMilliseconds = milliseconds % 1000;

    const minutes = Math.floor(seconds / 60);
    const remainingSeconds = seconds % 60;

    const hours = Math.floor(minutes / 60);
    const remainingMinutes = minutes % 60;

    const days = Math.floor(hours / 24);
    const remainingHours = hours % 24;

    if (days > 0) {
        return `${days} days, ${remainingHours} hours, ${remainingMinutes} minutes, ${remainingSeconds} seconds, ${remainingMilliseconds} milliseconds`;
    }
    if (hours > 0) {
        return `${hours} hours, ${remainingMinutes} minutes, ${remainingSeconds} seconds, ${remainingMilliseconds} milliseconds`;
    }
    if (minutes > 0) {
        return `${minutes} minutes, ${remainingSeconds} seconds, ${remainingMilliseconds} milliseconds`;
    }
    if (seconds > 0) {
        return `${seconds} seconds, ${remainingMilliseconds} milliseconds`;
    }
    return `${milliseconds} milliseconds`;
}

/**
 * Fetches data from a URL with exponential backoff retry strategy.
 *
 * @param {string} url - The URL to fetch data from.
 * @param {Object} headers - The headers to include in the request.
 * @returns {string} - The response body.
 * @throws {Error} - If all retries fail.
 */
function fetchWithRetry(url, headers) {
    const maxRetries = 40;
    let retryDelay = 1000; // Initial delay in milliseconds

    for (let attempt = 1; attempt <= maxRetries; attempt++) {
        try {
            const response = request('GET', url, {headers});
            return response.getBody('utf-8');
        } catch (error) {
            if (attempt === maxRetries) {
                throw new Error(`Failed to fetch data after ${maxRetries} attempts: ${error.message}`);
            }
            console.error(`Failed to fetch data:\n${error.message}.\nRequest headers: ${JSON.stringify(headers)}.\nRetrying in ${formatTime(retryDelay)}.`);
            // Wait for retryDelay milliseconds before retrying
            Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, retryDelay);
            retryDelay *= 2; // Exponential backoff
            const maxDelay = 180000; // Maximum delay of 3 minutes
            if (retryDelay > maxDelay) {
                retryDelay = maxDelay;
            }
        }
    }
}

module.exports = function (registry) {
    registry.blockMacro('mrdocs-releases', function () {
        const self = this
        self.process(function (parent, target, attrs) {
            // Collect all release URLs
            let cacheFilenamePath = 'releasesResponse.json';
            let cachePath = `${__dirname}/../build/requests/${cacheFilenamePath}`;
            fs.mkdirSync(`${__dirname}/../build/requests/`, {recursive: true});
            const GH_TOKEN = process.env.GITHUB_TOKEN || process.env.GH_TOKEN || ''
            const apiHeaders = GH_TOKEN ? {
                'User-Agent': 'request',
                'Authorization': `Bearer ${GH_TOKEN}`
            } : {
                'User-Agent': 'request'
            }
            const readFromCacheFile = fs.existsSync(cachePath) && fs.statSync(cachePath).mtime > new Date(Date.now() - 1000 * 60 * 60 * 24);
            const releasesResponse =
                readFromCacheFile ?
                    fs.readFileSync(cachePath, 'utf-8') :
                    fetchWithRetry('https://api.github.com/repos/cppalliance/mrdocs/releases', apiHeaders)
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
