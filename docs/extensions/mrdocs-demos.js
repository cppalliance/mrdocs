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
        let urlPath = new URL(url).pathname;
        let cacheFilenamePath = urlPath.replace(/[^a-zA-Z0-9]/g, '') + '.json';
        let cachePath = `${__dirname}/../build/requests/${cacheFilenamePath}`;
        fs.mkdirSync(`${__dirname}/../build/requests/`, {recursive: true});
        const readFromCacheFile = fs.existsSync(cachePath) && fs.statSync(cachePath).mtime > new Date(Date.now() - 1000 * 60 * 60 * 24);
        const data =
            readFromCacheFile ?
                fs.readFileSync(cachePath, 'utf-8') :
                request('GET', url).getBody('utf-8')
        if (!readFromCacheFile) {
            fs.writeFileSync(cachePath, data);
        }
        const regex = /<a href="([^"]+)\/">/g;
        const directories = [];
        let match;
        while ((match = regex.exec(data)) !== null) {
            if (match[1] !== '..' && match[1] !== '.') {
                directories.push(match[1]);
            }
        }
        return directories;
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
                // - The raw Asciidoc is already rendered by the mrdocs.com server as HTML
                versionFormats = versionFormats.filter(format => format !== 'adoc-asciidoc');
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
                                if (['adoc', 'xml', 'html'].includes(format)) {
                                    const adoc_icon = 'https://avatars.githubusercontent.com/u/3137042?s=200&v=4'
                                    const html_icon = 'https://raw.githubusercontent.com/FortAwesome/Font-Awesome/refs/heads/6.x/svgs/brands/html5.svg'
                                    const code_file_icon = 'https://raw.githubusercontent.com/FortAwesome/Font-Awesome/6.x/svgs/solid/file-code.svg'
                                    const icon = format === 'adoc' ? adoc_icon : format === 'html' ? html_icon : code_file_icon
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
