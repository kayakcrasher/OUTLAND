// Headless boundary test, never starts the renderer/game or creates a GL context.
globalThis.window={innerWidth:2400,innerHeight:1200,visualViewport:{height:1200}};
globalThis.requestAnimationFrame=callback=>setTimeout(callback,0);
Module.outlandTakeCheckpoint=()=>1;
Module.outlandFlush=()=>{};
