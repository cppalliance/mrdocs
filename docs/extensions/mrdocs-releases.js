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

/**
 * Converts a date to a human-readable string format.
 *
 * This function takes a date object or a date string and returns a formatted string
 * representing the date in the 'en-US' locale with the format 'Month Day, Year'.
 *
 * @param {Date|string} date - The date to be formatted.
 * @returns {string} - A human-readable string representing the date.
 */
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

/**
 * Gets the latest date among the various date fields in the release object.
 *
 * This function checks the `published_at` and `created_at` fields of the release,
 * as well as the `created_at` and `updated_at` fields of each asset in the release.
 * It returns the most recent date found among these fields.
 *
 * @param {Object} release - The release object containing date fields and assets.
 * @param {string} [release.published_at] - The published date of the release.
 * @param {string} [release.created_at] - The creation date of the release.
 * @param {Array} [release.assets] - An array of asset objects.
 * @param {string} [release.assets[].created_at] - The creation date of an asset.
 * @param {string} [release.assets[].updated_at] - The last updated date of an asset.
 * @returns {Date|null} - The latest date found among the release and asset dates, or null if no dates are found.
 */
function getReleaseDate(release) {
    const dates = [];

    if (release.published_at) {
        dates.push(new Date(release.published_at));
    }
    if (release.created_at) {
        dates.push(new Date(release.created_at));
    }
    if (release.assets && Array.isArray(release.assets)) {
        release.assets.forEach(asset => {
            if (asset.created_at) {
                dates.push(new Date(asset.created_at));
            }
            if (asset.updated_at) {
                dates.push(new Date(asset.updated_at));
            }
        });
    }

    return dates.length > 0 ? new Date(Math.max(...dates)) : null;
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
            const apple_icon = `image:https://raw.githubusercontent.com/cppalliance/mrdocs/refs/heads/develop/docs/modules/ROOT/images/icons/apple.svg[Apple Releases,width=16,height=16]`
            const linux_icon = `image:https://raw.githubusercontent.com/cppalliance/mrdocs/refs/heads/develop/docs/modules/ROOT/images/icons/linux.svg[Linux Releases,width=16,height=16]`
            const windows_icon = `image:https://raw.githubusercontent.com/cppalliance/mrdocs/refs/heads/develop/docs/modules/ROOT/images/icons/windows.svg[Windows Releases,width=16,height=16]`
            const package_icon = `image:https://raw.githubusercontent.com/cppalliance/mrdocs/refs/heads/develop/docs/modules/ROOT/images/icons/package.svg[Package,width=16,height=16]`

            let text = '|===\n'
            text += `|          3+| ${windows_icon} Windows                 2+| ${linux_icon} Linux               2+| ${apple_icon} macOS                \n`
            text += `| Release    | ${package_icon} 7z   | ${package_icon} msi  | ${package_icon} zip  | ${package_icon} tar.xz  | ${package_icon} tar.gz  | ${package_icon} tar.xz  | ${package_icon} tar.gz  \n`
            releases.sort((a, b) => getReleaseDate(b) - getReleaseDate(a));
            for (const release of releases) {
                if (release.name === 'llvm-package') continue
                const date = getReleaseDate(release)
                text += `| ${release.html_url}[${release.name},window=_blank]\n\n${humanizeDate(date)} `
                const assetSuffixes = ['win64.7z', 'win64.msi', 'win64.zip', 'Linux.tar.xz', 'Linux.tar.gz', 'Darwin.tar.xz', 'Darwin.tar.gz']
                for (const suffix of assetSuffixes) {
                    const asset = release.assets.find(asset => asset.name.endsWith(suffix))
                    if (asset) {
                        text += `| ${asset.browser_download_url}[${asset.name}]\n\n(${humanizeBytes(asset.size)}) `
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
