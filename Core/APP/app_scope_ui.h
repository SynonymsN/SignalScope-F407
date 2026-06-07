#ifndef APP_SCOPE_UI_H
#define APP_SCOPE_UI_H

#include "app_scope.h"

#ifdef __cplusplus
extern "C" {
#endif

void AppScopeUi_Create(void);
void AppScopeUi_Update(const AppScopeSnapshot_t *snapshot);
void AppScopeUi_ShowAction(const char *text);

#ifdef __cplusplus
}
#endif

#endif /* APP_SCOPE_UI_H */
