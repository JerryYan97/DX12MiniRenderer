#pragma once

#include "RendererBackend.h"

class HWRTRenderBackend : public RendererBackend
{
    public:
        HWRTRenderBackend() : RendererBackend(RendererBackendType::PathTracing) {}
        ~HWRTRenderBackend() {}

    protected:
        virtual void CustomInit() override;
    private:
        // Various Init Functions

};