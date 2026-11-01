#pragma once
#include "Modules/ModuleManager.h"

class FScopePluginModule : public IModuleInterface
{
public:
    virtual void StartupModule()  override;
    virtual void ShutdownModule() override;

private:
    void RegisterMenuExtensions();
};
