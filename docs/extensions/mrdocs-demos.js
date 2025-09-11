/*
    Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

    Official repository: https://github.com/cppalliance/mrdocs
*/

const https = require('https');
const request = require('sync-request');
const fs = require("fs");

function getSubdirectoriesSync(url) {
    try {
        // cache setup
        let urlPath = new URL(url).pathname;
        let cacheFilenamePath = urlPath.replace(/[^a-zA-Z0-9]/g, '') + '.json';
        let cachePath = `${__dirname}/../build/requests/${cacheFilenamePath}`;
        fs.mkdirSync(`${__dirname}/../build/requests/`, {recursive: true});
        const readFromCacheFile = fs.existsSync(cachePath) && fs.statSync(cachePath).mtime > new Date(Date.now() - 1000 * 60 * 60 * 24);

        // read file or make request
        const data =
            readFromCacheFile ?
                fs.readFileSync(cachePath, 'utf-8') :
                request('GET', url).getBody('utf-8')
        if (!readFromCacheFile) {
            fs.writeFileSync(cachePath, data);
        }

        // parse entries: name + date (if present)
        // Matches: <a href="name/">name/</a> [spaces] 11-Sep-2025 20:51
        const entryRe = /<a href="([^"]+)\/">[^<]*<\/a>\s+(\d{2}-[A-Za-z]{3}-\d{4}\s+\d{2}:\d{2})?/g;

        const month = { Jan:0,Feb:1,Mar:2,Apr:3,May:4,Jun:5,Jul:6,Aug:7,Sep:8,Oct:9,Nov:10,Dec:11 };
        const parseDate = (s) => {
          if (!s) return null; // missing or no date in listing
          // "11-Sep-2025 20:51"
          const m = /^(\d{2})-([A-Za-z]{3})-(\d{4})\s+(\d{2}):(\d{2})$/.exec(s.trim());
          if (!m) return null;
          const [, d, mon, y, hh, mm] = m;
          return new Date(Date.UTC(+y, month[mon], +d, +hh, +mm));
        };

        const isSemver = (name) => {
          // allow optional leading 'v', ignore pre-release/build metadata for ordering
          const m = /^v?(\d+)\.(\d+)\.(\d+)(?:[-+].*)?$/.exec(name);
          if (!m) return null;
          return { major: +m[1], minor: +m[2], patch: +m[3] };
        };

        const entries = [];
        let match;
        while ((match = entryRe.exec(data)) !== null) {
          const name = match[1];
          if (name === '.' || name === '..') continue;
          const dt = parseDate(match[2] || null);
          const sv = isSemver(name);
          entries.push({
            name,
            date: dt,               // may be null
            semver: !!sv,
            major: sv ? sv.major : 0,
            minor: sv ? sv.minor : 0,
            patch: sv ? sv.patch : 0,
          });
        }

        // sort
        entries.sort((a, b) => {
          // 1) non-semver first
          if (a.semver !== b.semver) return a.semver ? 1 : -1;

          if (!a.semver && !b.semver) {
            // non-semver: newer date first; if no date, push to end among non-semver
            const ad = a.date ? a.date.getTime() : -Infinity;
            const bd = b.date ? b.date.getTime() : -Infinity;
            if (ad !== bd) return bd - ad;
            // tie-breaker: name asc for stability
            return a.name.localeCompare(b.name);
          }

          // semver: major ↓, minor ↓, patch ↓
          if (a.major !== b.major) return b.major - a.major;
          if (a.minor !== b.minor) return b.minor - a.minor;
          if (a.patch !== b.patch) return b.patch - a.patch;
          // final tie-breaker: name asc
          return a.name.localeCompare(b.name);
        });

        return entries.map(e => e.name);
    } catch (error) {
        console.error('Error:', error);
        return [];
    }
}

function humanizePageType(pageType) {
    if (pageType === 'single') {
        return 'Single Page';
    } else if (pageType === 'multi') {
        return 'Multi Page';
    } else {
        return pageType;
    }
}

function humanizeFormat(format) {
    if (format === 'html') {
        return 'HTML';
    } else if (format === 'adoc') {
        return 'Asciidoc';
    } else if (format === 'adoc-asciidoc') {
        return 'Rendered AsciiDoc';
    } else if (format === 'xml') {
        return 'XML';
    }
    return format;
}

function humanizeLibrary(library) {
    if (library === 'boost-url') {
        return 'Boost.URL';
    }
    const boostLibrary = library.match(/boost-([\w]+)/);
    if (boostLibrary) {
        const capitalized = boostLibrary[1].charAt(0).toUpperCase() + boostLibrary[1].slice(1);
        return `Boost.${capitalized}`;
    }
    return library;
}

function libraryLink(library) {
    if (library === 'boost-url') {
        return 'https://github.com/boostorg/url[Boost.URL,window=_blank]';
    } else if (library === 'boost-scope') {
        return 'https://github.com/boostorg/scope[Boost.Scope,window=_blank]';
    }
    return humanizeLibrary(library);
}

module.exports = function (registry) {
    registry.blockMacro('mrdocs-demos', function () {
        const self = this
        self.process(function (parent, target, attrs) {
            // Collect all demo URLs
            let finalDemoDirs = [];
            const versions = getSubdirectoriesSync('https://mrdocs.com/demos/');
            for (const version of versions) {
                const demoLibraries = getSubdirectoriesSync(`https://mrdocs.com/demos/${version}/`);
                for (const demoLibrary of demoLibraries) {
                    const pageTypes = getSubdirectoriesSync(`https://mrdocs.com/demos/${version}/${demoLibrary}/`);
                    for (const pageType of pageTypes) {
                        const demoFormats = getSubdirectoriesSync(`https://mrdocs.com/demos/${version}/${demoLibrary}/${pageType}/`);
                        for (const demoFormat of demoFormats) {
                            finalDemoDirs.push({
                                url: `https://mrdocs.com/demos/${version}/${demoLibrary}/${pageType}/${demoFormat}`,
                                version: version,
                                library: demoLibrary,
                                pageType: pageType,
                                format: demoFormat
                            });
                        }
                    }
                }
            }

            // Create arrays for all unique versions, libraries, page types and formats
            let allVersions = [];
            for (const demoDir of finalDemoDirs) {
                if (!allVersions.includes(demoDir.version)) {
                    allVersions.push(demoDir.version);
                }
            }

            // Create tables
            let text = ''
            for (const version of allVersions) {
                text += '\n'
                text += `**${version}**\n\n`;
                text += `|===\n`;

                let versionPageTypes = [];
                let versionFormats = [];
                let versionLibraries = [];
                for (const demoDir of finalDemoDirs) {
                    if (demoDir.version !== version) {
                        continue
                    }
                    if (!versionPageTypes.includes(demoDir.pageType)) {
                        versionPageTypes.push(demoDir.pageType);
                    }
                    if (!versionFormats.includes(demoDir.format)) {
                        versionFormats.push(demoDir.format);
                    }
                    if (!versionLibraries.includes(demoDir.library)) {
                        versionLibraries.push(demoDir.library);
                    }
                }

                // Remove Rendered Asciidoc from the list of formats
                let multipageFormats = versionFormats.filter(format => format !== 'xml');

                let versionFormatColumns = versionFormats.map(format => `*${humanizeFormat(format)}*`).join(' | ');
                let multipageFormatColumns = multipageFormats.map(format => `*${humanizeFormat(format)}*`).join(' | ');

                // Multicells for the page format taking as many formats as possible for that page type
                const toPageTypeCell = pageType => `${(pageType === 'multi' ? multipageFormats : versionFormats).length}+| *${humanizePageType(pageType)}*`;
                text += `| ${versionPageTypes.map(toPageTypeCell).join(' ')}\n`;

                text += `| *Library* | ${multipageFormatColumns} | ${versionFormatColumns}\n\n`;
                for (const library of versionLibraries) {
                    text += `| ${libraryLink(library)}`
                    for (const pageType of versionPageTypes) {
                        const pageTypeFormats = pageType === 'multi' ? multipageFormats : versionFormats;
                        for (const format of pageTypeFormats) {
                            const demoDir = finalDemoDirs.find(
                                demoDir => demoDir.version === version &&
                                    demoDir.library === library &&
                                    demoDir.pageType === pageType &&
                                    demoDir.format === format);
                            if (demoDir) {
                                const demoUrlWithSuffix = demoDir.url + (() => {
                                    if (format === 'xml')
                                    {
                                        return '/reference.xml';
                                    }
                                    if (format === 'adoc')
                                    {
                                        if (pageType === 'multi')
                                        {
                                            return '/index.adoc'
                                        }
                                        else {
                                            return '/reference.adoc'
                                        }
                                    }
                                    if (format === 'html')
                                    {
                                        if (pageType === 'multi')
                                        {
                                            return '/index.html'
                                        }
                                        else {
                                            return '/reference.html'
                                        }
                                    }
                                    return '';
                                })()
                                if (['adoc', 'xml', 'html', 'adoc-asciidoc'].includes(format)) {
                                    const formatIcons = {
                                        adoc: 'https://avatars.githubusercontent.com/u/3137042?s=200&v=4',
                                        html: 'https://raw.githubusercontent.com/FortAwesome/Font-Awesome/refs/heads/6.x/svgs/brands/html5.svg',
                                        'adoc-asciidoc': 'https://raw.githubusercontent.com/FortAwesome/Font-Awesome/refs/heads/6.x/svgs/brands/html5.svg',
                                        default: 'https://raw.githubusercontent.com/FortAwesome/Font-Awesome/6.x/svgs/solid/file-code.svg'
                                    };
                                    const icon = formatIcons[format] || formatIcons.default;
                                    text += `| image:${icon}[${humanizeLibrary(library)} reference in ${humanizeFormat(format)} format,width=16,height=16,link=${demoUrlWithSuffix},window=_blank]`
                                } else {
                                    text += `| ${demoUrlWithSuffix}[🔗,window=_blank]`
                                }
                            } else {
                                text += `|     `
                            }
                        }
                    }
                    text += `\n`
                }
                text += `|===\n\n`
            }

            return self.parseContent(parent, text)
        })
    })
}
