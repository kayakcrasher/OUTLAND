// Exercise the production page's PLAY gate without launching the game.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const html = fs.readFileSync(process.argv[2], 'utf8');
const script = html.match(/<script>([\s\S]*?)<\/script>/)[1];
function page() {
    const elements = new Map();
    function element(id) {
        if (!elements.has(id)) elements.set(id, {
            style: {}, disabled: id === 'start', textContent: '', events: {},
            focus() {}, addEventListener(event, callback) { this.events[event] = callback; }
        });
        return elements.get(id);
    }
    const events = {};
    const context = vm.createContext({
        document: { getElementById: element, documentElement: {} },
        window: { addEventListener: (event, callback) => { events[event] = callback; } },
        screen: {}, console
    });
    vm.runInContext(script, context);
    return {context, element, events};
}
{
    const {context, element} = page();
    let calls = 0;
    context.Module.callMain = () => { calls++; };
    context.Module.onRuntimeInitialized();
    assert.equal(element('start').disabled, false);
    element('start').events.click.call(element('start'));
    assert.equal(calls, 1);
    assert.equal(element('gate').style.display, 'none');
    element('start').events.click.call(element('start'));
    assert.equal(calls, 1, 'Repeated taps must not start concurrent game loops');
}
{
    const {context, element} = page();
    context.Module.onRuntimeInitialized();
    assert.equal(element('start').disabled, true);
    assert.match(element('status').textContent, /launcher is missing/);
    assert.equal(element('gate').style.display, 'flex');
}
{
    const {context, element, events} = page();
    context.Module.callMain = () => { throw new Error('startup failed'); };
    context.Module.onRuntimeInitialized();
    element('start').events.click.call(element('start'));
    assert.match(element('warning').textContent, /startup failed/);
    assert.equal(element('gate').style.display, 'flex');
    events.error({message: 'asynchronous failure'});
    assert.match(element('status').textContent, /asynchronous failure/);
    events.unhandledrejection({reason: 'memory failure'});
    assert.match(element('status').textContent, /memory failure/);
    let prevented = false;
    element('canvas').events.webglcontextlost({preventDefault: () => { prevented = true; }});
    assert.equal(prevented, true);
    assert.match(element('warning').textContent, /graphics context was lost/);
}
(async () => {
    const {context, element} = page();
    let fullscreen = false;
    context.document.documentElement.requestFullscreen = async () => { fullscreen = true; };
    await element('fullscreen').events.click();
    assert.equal(fullscreen, true, 'Fullscreen must retain the loading/error overlay');
    console.log('Web bootstrap GREEN: PLAY, missing export, startup errors, graphics loss, fullscreen');
})().catch(error => { console.error(error); process.exitCode = 1; });
