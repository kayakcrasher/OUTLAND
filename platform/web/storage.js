// Restore IndexedDB before C++ starts, using the existing shared save serializers.
Module['noInitialRun'] = true;
Module['preRun'] = Module['preRun'] || [];
Module['preRun'].push(function () {
    FS.mkdir('/saves');FS.mount(IDBFS, {autoPersist: true}, '/saves');
    addRunDependency('outland-storage');
    FS.syncfs(true, function(error) {
        if(error) Module.outlandStorageWarning('Persistent storage unavailable; this session uses temporary saves.');
        removeRunDependency('outland-storage');
    });
});
(function () {
    let requested=false, flushing=false, again=false;
    setInterval(()=>{requested=true;},30000);
    document.addEventListener('visibilitychange',()=>{
        if(document.hidden) requested=true;
    });
    Module.outlandTakeCheckpoint=()=>{
        const value=requested;requested=false;return value ? 1 : 0;
    };
    Module.outlandFlush=function () {
        if(flushing){again=true;return;}
        flushing=true;
        FS.syncfs(false,function(error){
            flushing=false;
            if(error)Module.outlandStorageWarning('Save synchronization failed. Keep this tab open and try returning Home.');
            if(again){again=false;Module.outlandFlush();}
        });
    };
})();
