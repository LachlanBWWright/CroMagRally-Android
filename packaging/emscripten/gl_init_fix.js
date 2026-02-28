/**
 * Fix for LEGACY_GL_EMULATION with SDL3's Emscripten backend.
 *
 * SDL3 creates the WebGL context via emscripten_webgl_create_context() then
 * calls emscripten_webgl_make_context_current(). This bypasses
 * Browser.createContext() which normally fires moduleContextCreatedCallbacks
 * to initialize GLImmediate.  Without GLImmediate.init(), every legacy GL
 * call (glEnable(GL_TEXTURE_2D), etc.) crashes.
 *
 * We hook Module['onRuntimeInitialized'] to patch _emscripten_webgl_make_context_current
 * BEFORE main() runs.
 */
if (typeof Module === 'undefined') Module = {};
var __origOnInit = Module['onRuntimeInitialized'];
Module['onRuntimeInitialized'] = function() {
  // At this point all JS functions are defined but main() hasn't started
  if (typeof _emscripten_webgl_make_context_current === 'function') {
    var _orig = _emscripten_webgl_make_context_current;
    var _fired = false;
    _emscripten_webgl_make_context_current = function(ctx) {
      var r = _orig(ctx);
      if (!_fired && ctx && r === 0) {
        _fired = true;
        if (typeof Browser !== 'undefined' && Browser.moduleContextCreatedCallbacks) {
          Browser.moduleContextCreatedCallbacks.forEach(function(cb) { cb(); });
        }
      }
      return r;
    };
  }
  if (__origOnInit) __origOnInit();
};
