// android_assets.h
// Android asset extraction declarations.

#pragma once

#ifdef __ANDROID__

#ifdef __cplusplus
extern "C" {
#endif

bool Android_ExtractAssets(void);
const char *Android_GetDataPath(void);

#ifdef __cplusplus
}
#endif

#endif // __ANDROID__
