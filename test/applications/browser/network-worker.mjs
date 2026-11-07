// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import {load} from '/loader/webcpp-asio-loader.mjs'
const query = new URLSearchParams(location.search)
globalThis.beastSecurePort = query.get('securePort')
if (query.get('variant') === 'asyncify') delete WebAssembly.Suspending
const report = {lines:[], stderr:[], variant:null, error:null, network:[], host:'worker'}
const publish = () => postMessage(report)
try {
    const response = await fetch(query.get('manifest'))
    const artifacts = await response.clone().json()
    const importModule = async url => {
        const matches = ['jspi','asyncify'].filter(variant =>
            new URL(artifacts[variant].script,response.url).href === url)
        if (matches.length !== 1) throw Error('Unexpected worker artifact')
        report.variant = matches[0]
        publish()
        return import(url)
    }
    await load(query.get('manifest'), {fetch:async()=>response, importModule,
        module:{print:line=>{report.lines.push({line,at:performance.now()});publish()},
            printErr:line=>{report.stderr.push(line);publish()}}})
} catch (error) {
    report.error=String(error);publish()
}
addEventListener('unhandledrejection',event=>{report.error=String(event.reason);publish()})
addEventListener('error',event=>{report.error=event.message;publish()})
