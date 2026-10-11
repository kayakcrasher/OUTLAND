// Headless boundary test, never starts the renderer/game or creates a GL context.
globalThis.window={innerWidth:1560,innerHeight:900,visualViewport:{height:900}};
globalThis.requestAnimationFrame=callback=>setTimeout(callback,0);
Module.outlandTakeCheckpoint=()=>1;
Module.outlandFlush=()=>{};
